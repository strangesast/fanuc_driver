#/bin/bash
docker run --rm -it --network=host wurstmeister/kafka /opt/kafka/bin/kafka-streams-application-reset.sh \
  --input-topics input,input-avro \
  --bootstrap-servers localhost:9092 \
  --application-id ${1:-connector-consumer-input_sink-0}
