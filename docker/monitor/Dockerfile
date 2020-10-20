#from python:3.9-slim
from python:3.9

workdir /usr/src/app
copy monitor/requirements.txt .
run pip install -r requirements.txt

copy monitor/ .

cmd ["python3.9", "v2.py"]
