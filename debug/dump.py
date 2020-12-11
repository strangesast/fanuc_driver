import os
import time
import json
import psycopg2
import itertools
from pprint import pprint
from datetime import datetime
from kafka import KafkaConsumer


'''
cur.execute("INSERT INTO test (num, data) VALUES (%s, %s)",
...      (100, "abc'def"))
'''

db_params = os.environ.get('DB_PARAMS', 'dbname=testing user=postgres password=password host=postgres')
bootstrap_servers = os.environ.get('BOOTSTRAP_SERVERS', 'localhost:9092')
client_id = os.environ.get('CLIENT_ID', 'python-client')
table_name = "input-avro"

with open('table.txt', 'r') as f:
    columns = [s.strip() for s in f.readlines()]

db = psycopg2.connect(db_params)
consumer = KafkaConsumer('input', client_id=client_id, bootstrap_servers=bootstrap_servers, auto_offset_reset='earliest')

transformers = {'axes': json.dumps, 'actual': json.dumps, 'relative': json.dumps, 'absolute': json.dumps, 'load': json.dumps}
cur = db.cursor()

t = time.time()
try:
    for i, msg in enumerate(consumer):
        try:
            # "O0000%\xe8V\\u0018\xed\xe8V\\u0003"
            v = json.loads(msg.value)
        except Exception as e:
            continue
    
        if (i & (i - 1)) == 0:
            print(i)
    
        meta = v.pop('meta')
        values = v.pop('values')
    
        items = [('machine_id', msg.key.decode()), ('offset', msg.offset), ('timestamp', msg.timestamp), ('partition', msg.partition)] + [(f'meta_{k}', v) for k, v in meta.items()] + list(values.items())
        items = [(k, fn(v) if (fn := transformers.get(k)) else v) for k, v in items if k in columns]
        keys, values = zip(*items)
    
        qk = ', '.join(f'"{k}"' for k in keys)
        qv = ', '.join(['%s' for _ in values])
        q = f"insert into \"{table_name}\" ({qk}) VALUES ({qv}) on conflict do nothing"


        nt = time.time()
        if nt - t > 5:
            t = nt
            print('commit', datetime.fromtimestamp(msg.timestamp / 1000).isoformat(), flush=True)
            db.commit()

        try:
            cur.execute(q, values)
        except Exception as e:
            print(q)
            print(keys)
            print(values)
            raise e
    
finally:
    cur.close()
    db.close()
