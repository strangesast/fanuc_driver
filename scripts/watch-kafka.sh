#/bin/bash
# --from-beginning \
docker run --rm -it --network=host wurstmeister/kafka /opt/kafka/bin/kafka-console-consumer.sh \
  --property print.timestamp=true \
  --property print.key=true \
  --bootstrap-server localhost:9092 \
  --topic ${1:-input}
