import json
import asyncio
import asyncpg
from aiokafka import AIOKafkaConsumer


with open("table.txt", "r") as f:
    columns = [s.strip() for s in f.readlines()]

query = f"""
insert into "input-avro"({','.join(f'"{c}"' for c in columns)})
values ({','.join(f'${i+1}' for i, _ in enumerate(columns))})
on conflict do nothing
"""

transformers = {
    "axes": json.dumps,
    "actual": json.dumps,
    "relative": json.dumps,
    "absolute": json.dumps,
    "load": json.dumps,
}


def tr(msg, v):
    meta = v.pop("meta")
    values = v.pop("values")

    obj = dict(
        [
            ("machine_id", msg.key.decode()),
            ("offset", msg.offset),
            ("timestamp", msg.timestamp),
            ("partition", msg.partition),
        ]
        + [(f"meta_{k}", v) for k, v in meta.items()]
        + [(k, fn(v) if (fn := transformers.get(k)) else v) for k, v in values.items()]
    )
    row = [
        fn(v) if (v := obj.get(k)) is not None and (fn := transformers.get(k)) else v
        for k in columns
    ]
    return row


async def main():

    consumer = AIOKafkaConsumer(
        "input", bootstrap_servers="localhost:9092", auto_offset_reset="earliest"
    )

    conn = await asyncpg.connect(
        "postgresql://postgres:password@localhost:5433/testing"
    )
    await consumer.start()
    try:
        q = asyncio.Queue(1000)

        async def a():
            async for msg in consumer:
                try:
                    v = json.loads(msg.value)
                except json.JSONDecodeError:
                    continue

                vv = tr(msg, v)
                try:
                    q.put_nowait(vv)
                except asyncio.QueueFull:
                    await q.join()
                    await q.put(vv)

        async def b():
            to_insert = []
            while True:
                try:
                    vv = q.get_nowait()
                    to_insert.append(vv)
                    q.task_done()
                except asyncio.QueueEmpty:
                    if len(to_insert):
                        print(f"inserting... {len(to_insert)}")
                        await conn.executemany(query, to_insert)
                        to_insert = []

                    vv = await q.get()
                    to_insert.append(vv)
                    q.task_done()

        await asyncio.gather(a(), b())
        # await conn.execute(q, *row)
    finally:
        await consumer.stop()
        await conn.close()


if __name__ == "__main__":
    asyncio.run(main())
