#/bin/bash
# --from-beginning \
docker run --rm -it --network=host confluentinc/cp-kafka-connect kafka-avro-console-consumer \
  --bootstrap-server localhost:9092 \
  --topic ${1:-input-avro}
