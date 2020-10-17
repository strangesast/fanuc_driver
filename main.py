"""
monitor machine table, monitor processes polling machines
"""
import asyncpg
import asyncio
import signal
from aiohttp import web

async def init_db(app):
    async with app['db'].acquire() as con:
        await con.execute('''
            drop table if exists machine_monitor;
            create table machine_monitor(
              id           text,
              machine_ip   varchar(40),
              machine_port integer
            );
            insert into machine_monitor(id,machine_ip,machine_port) values ('test','localhost',8193);
        ''')



async def handle(request):
    name = request.match_info.get('name', "Anonymous")
    text = "Hello, " + name
    return web.Response(text=text)


tasks = {}


async def create_proc(id, machine_ip, machine_port):
    try:
        while True:
            proc = await asyncio.create_subprocess_shell(f'while true; do echo "{id=} {machine_ip=} {machine_port=}"; sleep 1; done', stdout=asyncio.subprocess.PIPE)
            async for line in proc.stdout:
                print('line', line)
            await asyncio.sleep(5)
    except asyncio.CancelledError:
        proc.kill()
        await proc.wait()


async def init_tasks(app: web.Application):
    async with app['db'].acquire() as con:
        rows = await con.fetch('''select id,machine_ip,machine_port from machine_monitor''')

        for row in rows:
            tasks[row['id']] = asyncio.create_task(create_proc(*tuple(row)))


async def start_background_tasks(app):
    app['db'] = await asyncpg.create_pool(user='postgres', password='password', database='testing')
    await init_db(app)
    await init_tasks(app)


async def cleanup_background_tasks(app):
    tasks = tasks.values()
    for task in tasks:
        task.cancel()
    await asyncio.gather(*tasks)
    await app['db'].close();
    app['loop'].cancel()
    await app['loop']


async def init():
    app = web.Application()

    #app.router.add_get('/', handler)

    app.add_routes([web.get('/', handle),
                    web.get('/{name}', handle)])
    
    app.on_startup.append(start_background_tasks)
    app.on_cleanup.append(cleanup_background_tasks)

    #app.on_startup.append(on_startup)
    #app.on_cleanup.append(on_cleanup)
    #runner = web.AppRunner(app)
    #try:
    #    await runner.setup()
    #    site = web.TCPSite(runner, 'localhost', 8080)
    #    await site.start()
    #    await asyncio.sleep(1000)

    #except asyncio.CancelledError:
    #    await runner.cleanup()
    return app


if __name__ == '__main__':
    #asyncio.run(init())
    #signal.signal(signal.SIGINT, lambda: task.cancel())
    #asyncio.run(task)

    web.run_app(init())
