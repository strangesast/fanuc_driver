#/bin/bash
#https://cwiki.apache.org/confluence/display/KAFKA/KIP-122%3A+Add+Reset+Consumer+Group+Offsets+tooling
#--to-datetime YYYY-MM-DDTHH:mm:SS.sss
#--shift-by
#--by-duration
docker run --rm -it --network=host wurstmeister/kafka /opt/kafka/bin/kafka-streams-application-reset.sh \
  --input-topics input,input-avro \
  --bootstrap-servers localhost:9092 \
  --shift-by -1000000 \
  --application-id ${1:-staging-streams-monitoring}
