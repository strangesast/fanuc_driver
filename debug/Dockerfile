from python:3

workdir /usr/src/app
copy requirements.txt .
run pip install -r requirements.txt
copy . .

cmd python3 dump.py
