docker run --rm -it --network=host wurstmeister/kafka /opt/kafka/bin/kafka-run-class.sh kafka.tools.GetOffsetShell \
  --broker-list localhost:9092 \
  --time -1 \
  --topic ${1:-input}
