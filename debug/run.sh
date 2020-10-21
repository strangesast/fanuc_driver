#!/bin/sh
docker run --rm -it -e BOOTSTRAP_SERVERS=kafka:29092 --network=fanuc_driver_default $(docker build -q .)
