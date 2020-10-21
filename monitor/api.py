"""
monitor machine table, monitor processes polling machines
"""
import asyncpg
import asyncio
import json

from contextlib import contextmanager
from datetime import datetime, date
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


async def start_background_tasks(_app):
    dsn = "postgresql://postgres:password@localhost/testing"
    _app["db"] = await asyncpg.create_pool(dsn)


async def cleanup_background_tasks(app):
    """background task cleanup callback"""
    await app["db"].close()


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


def default(o):
    if isinstance(o, (date, datetime)):
        return o.isoformat()

async def app_status(request):
    async with request.app["db"].acquire() as con:
        rows = await con.fetch('''
            select * from machine_monitor_status
            ''')
    return web.Response(text=json.dumps([dict(r) for r in rows], default=default))


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
        return web.json_response({"machine": None})

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
    return web.json_response({"machine": result})


async def app_update(request):
    """
    update machine network / device parameters based on previous
    probe (app_check)
    """
    qry = request.query
    machine_id, machine_ip = qry.get("id"), qry.get("ip")
    machine_port = validate_machine_port(qry.get("port", "8193"))

    row = await request.app["db"].fetchrow(
        """
        select machine_id, machine_ip, machine_port
        from machine_pings
        order by date desc"""
    )
    if row is None:
        return web.HTTPNotFound()

    # must match previous ping
    if tuple(row) != (machine_id, machine_ip, machine_port):
        return web.HTTPBadRequest()

    existing_machine = await request.app["db"].fetchrow(
        "select * from machines where machine_id = $1", machine_id
    )
    q = (
        """
        update machines
        set machine_ip=$2, machine_port=$3, modified_at=$4
        where machine_id = $1"""
        if existing_machine
        else """
        insert into machines(machine_id, machine_ip, machine_port, created_at)
        values($1, $2, $3, $4)
        """
    )
    args = [machine_id, machine_ip, machine_port, datetime.utcnow()]
    await request.app["db"].execute(q, *args)

    return web.HTTPOk()


app = web.Application()
app.add_routes(
    [
        web.get("/check", app_check),
        web.get("/update", app_update),
        web.get("/status", app_status),
    ]
)
app.on_startup.append(start_background_tasks)
app.on_cleanup.append(cleanup_background_tasks)


setup(app)
web.run_app(app)
