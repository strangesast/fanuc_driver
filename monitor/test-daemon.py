import asyncpg
import asyncio
from daemon import AdapterMonitor


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

    dsn = "postgresql://postgres:password@localhost:5432/testing"
    loop.run_until_complete(o.connect(dsn))

    task = loop.create_task(o.run())
    try:
        loop.run_until_complete(task)
    except KeyboardInterrupt:
        task.cancel()
        loop.run_until_complete(task)
