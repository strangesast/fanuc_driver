from kafka import KafkaConsumer
from collections import defaultdict
from itertools import groupby
from pprint import pprint
from datetime import datetime
import pytz
import json

tz = pytz.timezone('US/Eastern')

consumer = KafkaConsumer('input', bootstrap_servers='localhost:9092', auto_offset_reset='earliest')

def check_pkey_distribution():
    last = set()
    d = defaultdict(int)
    for i, msg in enumerate(consumer):
        d[msg.key] += 1
        if msg.key not in last:
            print(msg.key)
            last.add(msg.key)
        
        if (i & (i - 1)) == 0:
            print(i)
            pprint(d)


def check_value_key_distribution():
    total = 0
    d = defaultdict(int)
    for i, msg in enumerate(consumer):
        #print(dir(msg))
        v = json.loads(msg.value)
        #print(f'{msg.offset=} {msg.partition=}', v)
    
        values, meta = v['values'], v['meta']
    
        for key in values:
            total += 1
            d[key] += 1
    
        if (i & (i - 1)) == 0:
            print(i, total)
            pprint(sorted([tuple(p) for p in d.items()], key=lambda x: x[1]))


def check_meta_averages():
    d = defaultdict(lambda: (0, 0.0))
    for i, msg in enumerate(consumer):
        v = json.loads(msg.value)
        values, meta = v['values'], v['meta']
        
        for k, v in meta.items():
            n, t = d[k]
            d[k] = (n + 1, t + v)
    
        if (i & (i - 1)) == 0:
            print(i)
            pprint(d)
            pprint(sorted([(k, t / n) for k, (n, t) in d.items()], key=lambda x: x[1]))


#check_value_key_distribution()

def check_execution_changes():
    def it():
        for msg in consumer:
            v = json.loads(msg.value)
            values, meta = v['values'], v['meta']
            yield (msg.offset, msg.timestamp, msg.key, values)
    
    d = {}
    for i, ts, k, v in it():
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
    for i, msg in enumerate(consumer):
        v = json.loads(msg.value)
        values, meta = v['values'], v['meta']
    
        if msg.key in d and d[msg.key] > msg.timestamp:
            print('fuck')
            print(msg)
            break
    
        d[msg.key] = msg.timestamp

#check_pkey_distribution()
#check_value_key_distribution()
check_execution_changes()
#check_meta_averages()
