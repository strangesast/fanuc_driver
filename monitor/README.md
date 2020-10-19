
add ip address
check for machine at address
if response:
  great, add to db
or
  skip waiting for response. add anyway.
or
  indicate failure

compare with ids already in table
update if new ip address, add if new id
start polling process


get list of machines, ip addresses from db
each machine:
  poll for connectivity, id
  start fanuc-driver processes
  monitor process health


               /--> fanuc-driver --> kafka
process manager --> fanuc-driver --> kafka
               \--> fanuc-driver --> kafka


# Development
To install deps:  
`pip install -r requirements.txt`  

To edit:  
`PYTHONPATH=env/lib/python3.9/site-packages/ vim ...`  

Install fwlib  
`pip install git+https://github.com/strangesast/fanuc-python-extension`  
