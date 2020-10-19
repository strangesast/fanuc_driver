import asyncio
import asyncpg
import itertools
from pprint import pprint


async def init(pool):
    async with pool.acquire() as con:
        await con.execute('''
            drop table if exists machine_monitor;
            create table machine_monitor(
              machine_id   text PRIMARY KEY,
              machine_ip   varchar(40),
              machine_port integer
            );
            create table machine_monitor_status(
              id          serial
              machine_id  text
              status      varchar(100)
              ts          timestamp default current_timestamp
            );
            insert into machine_monitor(id,machine_ip,machine_port) values ('test','localhost',8193);
        ''')


async def update_status(pool: asyncpg.Pool, machine_id, status):
    con.execute('''
        insert into machine_monitor_status(machine_id,status) values ($1,$2)
    ''', machine_id, status)


async def monitor_machine(pool: asyncpg.Pool, machine_id, machine_ip, machine_port):
    try:
        for i in itertools.count(0):
            print(machine_id, 'RESTARTING' if i > 0 else 'STARTING')
            proc = await asyncio.create_subprocess_shell('''
                for i in 1 2; do echo "{} {} {}"; sleep 1; done
            '''.format(machine_id, machine_ip, machine_port), stdout=asyncio.subprocess.PIPE)
            print(machine_id, 'STARTED')
            async for line in proc.stdout:
                # indicate STARTED
                print('line', line)
            returncode = await proc.wait()
            if returncode:
                print(machine_id, 'FAILED')
            else:
                print(machine_id, 'STOPPED')
            await asyncio.sleep(5)
    except asyncio.CancelledError:
        if proc.returncode is None:
            proc.terminate()
            await proc.wait()


async def worker(tasks, queue: asyncio.Queue):
    while True:
        machine_id, config = await queue.get()
        exists = machine_id in tasks

        if exists:
            # if config, indicate RELOADING else indicate DESTROYING
            print(machine_id, 'RELOADING' if config else 'DESTROYING')
            task = tasks.pop(machine_id)
            task.cancel()
            await task 
            if not config:
                print(machine_id, 'DESTROYED')
            # if not config indicate DESTROYED
        elif config is None:
            # invalid condition
            continue

        if config is not None:
            machine_ip, machine_port = config
            tasks[machine_id] = asyncio.create_task(monitor_machine(machine_id, machine_ip, machine_port))

        queue.task_done()


async def main():
    queue = asyncio.Queue()

    #pool = await asyncpg.create_pool(user='postgres', password='password', database='testing')
    #await init(pool)
    
    #async with pool.acquire() as con:
    #    rows = await con.fetch('''select id,machine_ip,machine_port from machine_monitor''')

    #    for row in rows:
    #        tasks[row['id']] = asyncio.create_task(create_proc(*tuple(row)))
    tasks = {}
    worker_task = asyncio.create_task(worker(tasks, queue))

    args = [('machine_{}'.format(i), ('10.0.0.{}'.format(i + 200), 8193)) for i in range(4)]
    for arg in args:
        queue.put_nowait(arg)

    await queue.join()
    #tasks = [asyncio.create_task(monitor_machine(*a)) for a in args]

    try:
        await worker_task
    except asyncio.CancelledError:
        _tasks = tasks.values()
        for task in _tasks:
            task.cancel()
        await asyncio.gather(*_tasks)


if __name__ == '__main__':
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
