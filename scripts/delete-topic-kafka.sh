#!/bin/bash
docker run --rm -it --network=host wurstmeister/kafka /opt/kafka/bin/kafka-topics.sh \
  --bootstrap-server localhost:9092 \
  --delete \
  --topic ${1:-output}
