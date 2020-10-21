from i386/python:3-slim-buster

from python:3-slim-buster

run apt-get update && apt-get install -y git build-essential
workdir /usr/src/app

copy external external
copy requirements.txt .

run pip install -r requirements.txt

copy . .

cmd ["python3", "main.py"]
