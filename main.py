"""
monitor machine table, monitor processes polling machines
"""
import asyncio
from asyncio.subprocess import Process
from contextlib import contextmanager
from typing import Dict, Tuple
from datetime import datetime
import asyncpg  # type: ignore
from aiohttp import web
from aiojobs.aiohttp import setup, spawn  # type: ignore
import fwlib  # type: ignore

lock = asyncio.Lock()


def validate_machine_port(port: str):
    """validate and return port number"""
    iport = 0
    try:
        iport = int(port)
    except ValueError:
        return None
    if not 0 < iport < 65535:
        return None
    return iport


@contextmanager
def get_machine_connection(machine_ip, machine_port=8193, timeout=10):
    """wrap machine connection setup / cleanup methods"""
    fwlib.allclibhndl3(machine_ip, machine_port, timeout)
    try:
        yield
    finally:
        fwlib.freelibhndl()


async def check_device(device_ip, device_port):
    """attempt to connect to and query machine"""

    def tester():
        try:
            with get_machine_connection(device_ip, device_port):
                cnc_id = fwlib.rdcncid()
                axis_data = fwlib.rdaxisname()
                return {**cnc_id, "axes": axis_data}
        except Exception as e:
            print(e)
            return None

    async with lock:
        result = await asyncio.get_event_loop().run_in_executor(None, tester)
    return result


async def app_check(request):
    """probe a machine at ip / port.  store results in "machine-pings" table"""
    qry = request.query
    device_ip = qry.get("ip")

    device_port = validate_machine_port(qry.get("port", 8193))
    if device_port is None:
        return web.HTTPBadRequest()

    job = await spawn(request, check_device(device_ip, device_port))
    result = await job.wait()
    if result is None:
        return web.json_response({'machine': None})

    await request.app["db"].execute(
        """ INSERT INTO machine_pings(
           date,
           machine_id,
           machine_ip,
           machine_port,
           machine_axes) VALUES($1, $2, $3, $4, $5) """,
        datetime.utcnow(),
        result["id"],
        device_ip,
        device_port,
        [a["id"].strip() for a in result["axes"]],
    )
    return web.json_response({'machine': result})


async def app_update(request):
    """update machine network / device parameters based on previous probe (app_check)"""
    qry = request.query
    machine_id, machine_ip = qry.get("id"), qry.get("ip")
    machine_port = validate_machine_port(qry.get("port", "8193"))

    row = await request.app["db"].fetchrow(
        "select machine_id, machine_ip, machine_port from machine_pings order by date desc"
    )
    if row is None:
        return web.HTTPNotFound()

    # must match previous ping
    if tuple(row) != (machine_id, machine_ip, machine_port):
        return web.HTTPBadRequest()

    existing_machine = await request.app["db"].fetchrow(
        "select * from machines where machine_id = $1", machine_id
    )
    if existing_machine:
        await request.app["db"].execute(
            """update machines set machine_ip = $2, machine_port = $3, modified_at = DEFAULT
               where machine_id = $1""",
            machine_id,
            machine_ip,
            machine_port,
        )
    else:
        await request.app["db"].execute(
            """insert into machines(machine_id, machine_ip, machine_port, created_at)
               values($1, $2, $3, $4)""",
            machine_id,
            machine_ip,
            machine_port,
            datetime.utcnow(),
        )

    return web.HTTPOk()


async def looper(_app: web.Application):
    """maintain machine monitoring processes"""
    # machine_ids -> connection properties
    machines: Dict[str, Tuple[Tuple[str, int], Process]] = {}

    async def start(machine_id, machine_port):
        proc = None
        ret = 0
        try:
            while True:
                try:
                    print("starting...")
                    proc = await asyncio.create_subprocess_exec(
                        "fanuc_driver", machine_id, str(machine_port)
                    )
                    print("running...")
                    ret = await proc.wait()
                except:
                    pass
                print(f"stopped ({ret})...")
                await asyncio.sleep(5)
        except asyncio.CancelledError:
            print("cancelled...")
            if proc:
                proc.terminate()
                await proc.wait()

    tasks = []

    task = asyncio.create_task(start("127.0.0.1", 8193))
    tasks.append(task)

    machines["00000000-00000000-00000000-00000000"] = (("127.0.0.1", 8193), task)

    try:
        await asyncio.gather(*tasks)
    except asyncio.CancelledError:
        for task in tasks:
            task.cancel()
        await asyncio.gather(*tasks)
    # try:
    #    while True:
    #        print("loop")
    #        await asyncio.sleep(1)
    # except asyncio.CancelledError:
    #    print("cancelled")
    # finally:
    #    pass


async def start_background_tasks(_app):
    """background task start callback"""
    _app["db"] = await asyncpg.connect(
        "postgresql://postgres:password@localhost/testing"
    )
    _app["loop"] = asyncio.create_task(looper(_app))


async def cleanup_background_tasks(_app):
    """background task cleanup callback"""
    _app["loop"].cancel()
    await asyncio.gather(_app["loop"], _app["db"].close())


app = web.Application()
app.add_routes(
    [
        web.get("/check", app_check),
        web.get("/update", app_update),
    ]
)

app.on_startup.append(start_background_tasks)
app.on_cleanup.append(cleanup_background_tasks)

if __name__ == "__main__":
    setup(app)
    web.run_app(app)
