import asyncio
import asyncpg
import itertools
from contextlib import suppress
import json


async def init(pool: asyncpg.pool.Pool):
    async with pool.acquire() as con:
        await con.execute('''
            drop table if exists machine_monitor_status_updates CASCADE;
            create table machine_monitor_status_updates (
              id          serial PRIMARY KEY,
              machine_id  text,
              status      varchar(100),
              ts          timestamp default current_timestamp
            );

            drop table if exists machine_monitor CASCADE;
            create table machine_monitor(
              machine_id   text,
              machine_ip   varchar(40),
              machine_mac  char(17),
              machine_port integer,
              PRIMARY KEY("machine_id")
            );

            drop table if exists machine_monitor_status;
            create table machine_monitor_status (
              machine_id   text,
              status_text  text,
              status_id    integer,
              PRIMARY KEY("machine_id"),
              FOREIGN KEY ("machine_id") REFERENCES machine_monitor("machine_id"),
              FOREIGN KEY ("status_id") REFERENCES machine_monitor_status_updates("id")
            );


            CREATE OR REPLACE FUNCTION notify_event() RETURNS TRIGGER AS $$
              DECLARE
                payload JSON;
              BEGIN
                IF (TG_OP = 'DELETE') THEN
                  payload = json_build_object('machine_id', OLD.machine_id, 'payload', NULL);
                  PERFORM pg_notify('events', payload::text);
                ELSE
                  payload = json_build_object('machine_id', NEW.machine_id, 'payload', row_to_json(NEW));
                  PERFORM pg_notify('events', payload::text);
                END IF;
            
                RETURN NULL;
              END;
            $$ LANGUAGE plpgsql;
        
            CREATE TRIGGER notify_event
            AFTER INSERT OR UPDATE OR DELETE ON machine_monitor
            FOR EACH ROW EXECUTE PROCEDURE notify_event();
        
        ''')
        #insert into machine_monitor(machine_id,machine_ip,machine_port) values ('test','localhost',8193);
        #insert into machine_monitor_status(machine_id) values ('test');


async def status_worker(pool: asyncpg.pool.Pool, status_queue: asyncio.Queue):
    try:
        while True:
            machine_id, status = await status_queue.get()
            print(f'{machine_id=} {status=}')
            async with pool.acquire() as con:
                async with con.transaction():
                    status_id = await con.fetchval('''
                        insert into machine_monitor_status_updates(machine_id,status) values ($1,$2) returning id
                    ''', machine_id, status)
                    await con.execute('''
                        update machine_monitor_status set status_id=$2, status_text=$3 where machine_id=$1
                    ''', machine_id, status_id, status)
            status_queue.task_done()
    except asyncio.CancelledError:
        pass


async def monitor_machine(status_queue: asyncio.Queue, machine_id, machine_ip, machine_port, sleep_timeout=5):
    try:
        for i in itertools.count(0):
            await status_queue.put((machine_id, 'RESTARTING' if i > 0 else 'STARTING'))
            args = ['./mock-test.sh', f'{machine_id=}', f'{machine_ip=}', f'{machine_port=}']
            proc = await asyncio.create_subprocess_exec(*args, stdout=asyncio.subprocess.PIPE)
            await status_queue.put((machine_id, 'STARTED'))
            async for line in proc.stdout:
                # indicate STARTED
                print('line', line)
            returncode = await proc.wait()
            await status_queue.put((machine_id, 'FAILED' if returncode else 'STOPPED'))
            await asyncio.sleep(sleep_timeout)

    except asyncio.CancelledError:
        if proc.returncode is None:
            proc.terminate()
            await proc.wait()
        await status_queue.put((machine_id, 'SHUTDOWN'))


async def worker(pool: asyncpg.pool.Pool, tasks: dict[str, asyncio.Task], updates_queue: asyncio.Queue, status_queue: asyncio.Queue):
    async with pool.acquire() as con:
        for row in await con.fetch('''
            select machine_id,machine_ip,machine_port,machine_mac from machine_monitor
        '''):
            machine_id, *params = row
            updates_queue.put_nowait((machine_id, params))

        def cb(*args):
            data = json.loads(args[3])
            machine_id, d = data['machine_id'], data['payload']
            if d:
                machine_ip, machine_port, machine_mac = d['machine_ip'], d['machine_port'], d['machine_mac']
                payload = (machine_ip, machine_port, machine_mac)
            else:
                payload = None
            updates_queue.put_nowait((machine_id, payload))


        try:
            await con.add_listener('events', cb)
            while True:
                machine_id, config = await updates_queue.get()
                exists = machine_id in tasks
        
                if exists:
                    await status_queue.put((machine_id, 'RELOADING' if config else 'DESTROYING'))
                    task = tasks.pop(machine_id)
                    task.cancel()
                    await task 
                    if not config:
                        await status_queue.put((machine_id, 'DESTROYED'))
                    # if not config indicate DESTROYED
                elif config is None:
                    # invalid condition
                    continue
        
                if config is not None:
                    machine_ip, machine_port, machine_mac = config
                    tasks[machine_id] = asyncio.create_task(monitor_machine(status_queue, machine_id, machine_ip, machine_port))
        
                updates_queue.task_done()

        except asyncio.CancelledError:
            await con.remove_listener('events', cb)


async def main():
    updates_queue = asyncio.Queue()
    status_queue = asyncio.Queue()

    pool = await asyncpg.create_pool(user='postgres', password='password', database='testing')
    await init(pool)
    
    tasks = {}
    status_task = asyncio.create_task(status_worker(pool, status_queue))
    worker_task = asyncio.create_task(worker(pool, tasks, updates_queue, status_queue))

    try:
        # shield workers from first CancelledError
        await asyncio.shield(asyncio.gather(worker_task, status_task))
    except asyncio.CancelledError:
        # cancel worker and worker tasks
        worker_task.cancel()
        _tasks = tasks.values()
        for task in _tasks:
            task.cancel()
        await asyncio.gather(*_tasks, worker_task)

        # wait for last status msgs to be processed
        await status_queue.join()

        # finally, close status worker
        status_task.cancel()
        await status_task


if __name__ == '__main__':
    loop = asyncio.new_event_loop()
    
    task = loop.create_task(main())
    try:
        loop.run_until_complete(task)
    except KeyboardInterrupt:
        pass
    finally:
        task.cancel()
        loop.run_until_complete(task)
