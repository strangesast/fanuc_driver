import time
import asyncio
from itertools import count

#loop.run_in_executor(None, a)
lasttime = None

def sleeper():
    time.sleep(1)


async def t(lock, i):
    async with lock:
        lasttime = time.time()
        await asyncio.get_event_loop().run_in_executor(None, sleeper)
        print(f'{i=} {time.time() - lasttime}')


async def main():
    tasks = []
    lock = asyncio.Lock()
    for i in range(10):
        task = asyncio.create_task(t(lock, i))
        tasks.append(task)

    await asyncio.gather(*tasks)


if __name__ == '__main__':
    asyncio.run(main())
