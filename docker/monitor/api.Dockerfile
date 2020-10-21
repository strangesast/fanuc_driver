#from python:3.9-slim
from python:3.9

workdir /usr/src/app
copy monitor/requirements requirements
run pip install -r requirements/api.txt

copy monitor/ .

cmd ["python3.9", "api.py"]
