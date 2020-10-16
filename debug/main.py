import os
from kafka import KafkaConsumer
from collections import defaultdict
from itertools import groupby
from pprint import pprint
from datetime import datetime
import pytz
import json

tz = pytz.timezone('US/Eastern')

bootstrap_servers = os.environ.get('BOOTSTRAP_SERVERS', 'localhost:9092')
print(f'using {bootstrap_servers=}')
consumer = KafkaConsumer('input', bootstrap_servers=bootstrap_servers, auto_offset_reset='earliest')

def check_pkey_distribution():
    last = set()
    d = defaultdict(int)
    i = 0
    while True:
        msg, v = yield
        d[msg.key] += 1

        if msg.key not in last:
            print(msg.key)
            last.add(msg.key)
        
        if (i & (i - 1)) == 0:
            print(i)
            pprint(d)

        i += 1


def check_value_key_distribution():
    total = 0
    d = defaultdict(int)
    i = 0
    while True:
        msg, v = yield
    
        values, meta = v['values'], v['meta']
    
        keys = values.keys()
        if len(keys):
            for key in keys:
                total += 1
                d[key] += 1
        else:
            d[None] += 1
    
        if (i & (i - 1)) == 0:
            print(i, total)
            pprint(sorted([tuple(p) for p in d.items()], key=lambda x: x[1]))

        i += 1


def check_meta_averages():
    d = defaultdict(lambda: (0, 0.0))
    i = 0
    while True:
        msg, v = yield
        values, meta = v['values'], v['meta']
        
        for k, v in meta.items():
            n, t = d[k]
            d[k] = (n + 1, t + v)
    
        if (i & (i - 1)) == 0:
            print(i)
            pprint(d)
            pprint(sorted([(k, t / n) for k, (n, t) in d.items()], key=lambda x: x[1]))

        i += 1


def check_execution_changes():
    d = {}
    while True:
        msg, v = yield

        values, meta = v['values'], v['meta']
        i, ts, k, v = msg.offset, msg.timestamp, msg.key, values

        if (n := v.get('execution')) is not None:
            a, b, _ = e if (e := d.get(k)) is not None else (None, None, None)
            if n != b:
                d[k] = [b, n, str(datetime.fromtimestamp(ts / 1000).astimezone(tz))]
                print()
                for k, (a, b, t) in sorted(d.items(), key=lambda x: x[0]):
                    a = a or ''
                    b = b or ''
                    print(f'{k} {a.rjust(12)} -> {b.ljust(12)} at {t}')

def check():
    d = {}
    while True:
        msg, v = yield
    
        if msg.key in d and d[msg.key] > msg.timestamp:
            print('fuck')
            print(msg)
    
        d[msg.key] = msg.timestamp


def check_key():
    i = 0
    while True:
        msg, v = yield
        values, meta = v['values'], v['meta']
    
        if (value := values.get('block')):
            print('value', value)


pkey_distribution = check_pkey_distribution()
next(pkey_distribution)

value_key_distribution = check_value_key_distribution()
next(value_key_distribution)

execution_changes = check_execution_changes()
next(execution_changes)

meta_averages = check_meta_averages()
next(meta_averages)

for msg in consumer:
    try:
        # "O0000%\xe8V\\u0018\xed\xe8V\\u0003"
        v = json.loads(msg.value)
    except Exception as e:
        continue

    p = (msg, v)

    pkey_distribution.send(p)
    #execution_changes.send(p)

    value_key_distribution.send(p)
    #meta_averages.send(p)

