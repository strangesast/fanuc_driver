import asyncio
import asyncpg
import itertools
import json


class AdapterMonitor:
    tasks: dict[str, asyncio.Task] = {}
    executable = "adapter_fanuc"

    async def connect(self, dsn):
        self.pool = await asyncpg.create_pool(dsn)

    async def init_db(self, con: asyncpg.Connection):
        await con.execute(
            """
        CREATE TABLE IF NOT EXISTS machine_monitor_status_updates (
          id             serial,
          machine_id     text,
          status         varchar(100),
          status_message text,
          ts             timestamp default current_timestamp,
          PRIMARY KEY ("id")
        );
        CREATE TABLE IF NOT EXISTS machine_monitor(
          machine_id   text,
          machine_ip   varchar(40),
          machine_port integer,
          PRIMARY KEY("machine_id")
        );
        CREATE TABLE IF NOT EXISTS machine_monitor_status (
          machine_id  text,
          status_text text,
          status_id   integer,
          ts          timestamp default current_timestamp,
          PRIMARY KEY ("machine_id"),
          FOREIGN KEY ("machine_id")
              REFERENCES machine_monitor("machine_id"),
          FOREIGN KEY ("status_id")
              REFERENCES machine_monitor_status_updates("id")
        );

        CREATE OR REPLACE FUNCTION notify_event() RETURNS TRIGGER AS $$
          DECLARE
            payload JSON;
          BEGIN
            IF (TG_OP = 'DELETE') THEN
              payload = json_build_object(
                'machine_id',
                OLD.machine_id,
                'payload',
                NULL
              );
              PERFORM pg_notify('events', payload::text);
            ELSE
              payload = json_build_object(
                'machine_id',
                NEW.machine_id,
                'payload',
                row_to_json(NEW)
              );
              PERFORM pg_notify('events', payload::text);
            END IF;

            RETURN NULL;
          END;
        $$ LANGUAGE plpgsql;

        CREATE TRIGGER notify_event
        AFTER INSERT OR UPDATE OR DELETE ON machine_monitor
        FOR EACH ROW EXECUTE PROCEDURE notify_event();

        """
        )

    async def __status_worker(self):
        try:
            while True:
                machine_id, status, status_msg = await self.status_queue.get()
                print(f"{machine_id=} {status=}")
                async with self.pool.acquire() as con:
                    async with con.transaction():
                        status_id = await con.fetchval(
                            """
                            insert into machine_monitor_status_updates(
                                machine_id,
                                status,
                                status_message)
                            values ($1,$2,$3) returning id
                        """,
                            machine_id,
                            status,
                            status_msg,
                        )
                        await con.execute(
                            """
                            update machine_monitor_status
                            set status_id=$2, status_text=$3
                            where machine_id=$1
                        """,
                            machine_id,
                            status_id,
                            status,
                        )
                self.status_queue.task_done()
        except asyncio.CancelledError:
            pass

    async def monitor_machine(
        self, machine_id: str, machine_ip: str, machine_port: int, timeout=5
    ):
        try:
            for i in itertools.count(0):
                await self.status_queue.put(
                    (machine_id, "RESTARTING" if i > 0 else "STARTING", None)
                )
                args = [
                    self.executable,
                    f"{machine_id=}",
                    f"{machine_ip=}",
                    f"{machine_port=}",
                ]
                proc = await asyncio.create_subprocess_exec(
                    *args,
                    stdout=asyncio.subprocess.PIPE,
                    stderr=asyncio.subprocess.PIPE,
                )
                await self.status_queue.put((machine_id, "STARTED", None))

                # careful: stdout may grow
                stdout, stderr = await proc.communicate()

                returncode = await proc.wait()
                s = "FAILED" if returncode else "STOPPED"
                msg = stderr.decode() if stderr else None
                await self.status_queue.put((machine_id, s, msg))
                await asyncio.sleep(timeout)

        except asyncio.CancelledError:
            if proc.returncode is None:
                proc.terminate()
                await proc.wait()
            await self.status_queue.put((machine_id, "SHUTDOWN", None))

    async def __updates_worker(self):
        async with self.pool.acquire() as con:
            q = """select machine_id,machine_ip,machine_port
                from machine_monitor"""
            for row in await con.fetch(q):
                machine_id, *params = row
                self.updates_queue.put_nowait((machine_id, params))

            def cb(*args):
                data = json.loads(args[3])
                machine_id, d = data["machine_id"], data["payload"]
                if d:
                    payload = (d["machine_ip"], d["machine_port"])
                else:
                    payload = None
                self.updates_queue.put_nowait((machine_id, payload))

            try:
                await con.add_listener("events", cb)
                while True:
                    machine_id, config = await self.updates_queue.get()

                    if machine_id in self.tasks:
                        s = "RELOADING" if config else "DESTROYING"
                        await self.status_queue.put((machine_id, s, None))
                        task = self.tasks.pop(machine_id)
                        task.cancel()
                        await task
                        if not config:
                            s = "DESTROYED"
                            await self.status_queue.put((machine_id, s, None))
                    elif config is None:
                        # invalid condition
                        continue

                    if config:
                        machine_ip, machine_port = config
                        coro = self.monitor_machine(
                            machine_id, machine_ip, machine_port
                        )
                        self.tasks[machine_id] = asyncio.create_task(coro)

                    self.updates_queue.task_done()

            except asyncio.CancelledError:
                await con.remove_listener("events", cb)

    async def run(self):
        self.updates_queue = asyncio.Queue()
        self.status_queue = asyncio.Queue()

        async with self.pool.acquire() as con:
            await self.init_db(con)

        status_task = asyncio.create_task(self.__status_worker())
        worker_task = asyncio.create_task(self.__updates_worker())

        try:
            # shield workers from first CancelledError
            await asyncio.shield(asyncio.gather(worker_task, status_task))
        except asyncio.CancelledError:
            # cancel worker and worker tasks
            worker_task.cancel()
            _tasks = self.tasks.values()
            for task in _tasks:
                task.cancel()
            await asyncio.gather(*_tasks, worker_task)

            # wait for last status msgs to be processed
            await self.status_queue.join()

            # finally, close status worker
            status_task.cancel()
            await status_task


class TestAdapterMonitor(AdapterMonitor):
    executable = "./mock-test.sh"

    async def init_db(self, con: asyncpg.Connection):
        await con.execute(
            """
            drop table if exists machine_monitor_status_updates CASCADE;
            drop table if exists machine_monitor CASCADE;
            drop table if exists machine_monitor_status;
        """
        )
        await AdapterMonitor.init_db(self, con)
        await con.execute(
            """
           insert into machine_monitor(machine_id,machine_ip,machine_port)
           values ('test','localhost',8193);
           insert into machine_monitor_status(machine_id)
           values ('test');
        """
        )


if __name__ == "__main__":
    # asyncio.run cancels tasks before main soo

    o = TestAdapterMonitor()
    loop = asyncio.new_event_loop()

    loop.run_until_complete(
        o.connect("postgresql://postgres:password@localhost:5432/testing")
    )

    task = loop.create_task(o.run())
    try:
        loop.run_until_complete(task)
    except KeyboardInterrupt:
        task.cancel()
        loop.run_until_complete(task)
