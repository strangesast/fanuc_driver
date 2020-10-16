docker run --rm -it --network=fanuc_driver_default wurstmeister/kafka /opt/kafka/bin/kafka-run-class.sh kafka.tools.GetOffsetShell \
  --broker-list kafka:29092 \
  --time -1 \
  --topic ${1:-input}
