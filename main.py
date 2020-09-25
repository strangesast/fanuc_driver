"""
monitor machine table, monitor processes polling machines
"""
import asyncio
from aiohttp import web


async def handle(request):
    name = request.match_info.get('name', "Anonymous")
    text = "Hello, " + name
    return web.Response(text=text)


async def looper(app: web.Application):
    try:
        while True:
            print('loop')
            await asyncio.sleep(1)
    except asyncio.CancelledError:
        print('cancelled')
    finally:
        pass


async def start_background_tasks(app):
    app['loop'] = asyncio.create_task(looper(app));


async def cleanup_background_tasks(app):
    app['loop'].cancel()
    await app['loop']


app = web.Application()
app.add_routes([web.get('/', handle),
                web.get('/{name}', handle)])

app.on_startup.append(start_background_tasks)
app.on_cleanup.append(cleanup_background_tasks)

if __name__ == '__main__':
    web.run_app(app)
