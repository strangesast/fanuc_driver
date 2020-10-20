#from wurstmeister/kafka
from confluentinc/cp-kafka-connect:6.0.0

#run apk add gettext && \
#  mkdir -p /kafka/ && \
#  mkdir -p /opt/kafka/plugins &&\
#  wget -P /opt/kafka/plugins https://packages.confluent.io/maven/io/confluent/kafka-connect-jdbc/5.5.0/kafka-connect-jdbc-5.5.0.jar && \
#  wget -P /opt/kafka/plugins https://packages.confluent.io/maven/io/confluent/kafka-connect-avro-converter/5.5.0/kafka-connect-avro-converter-5.5.0.jar && \
#  wget -P /opt/kafka/plugins https://packages.confluent.io/maven/io/confluent/common-config/5.5.0/common-config-5.5.0.jar && \
#  wget -P /opt/kafka/plugins https://packages.confluent.io/maven/io/confluent/kafka-schema-registry-client/5.5.0/kafka-schema-registry-client-5.5.0.jar && \
#  wget -P /opt/kafka/plugins https://packages.confluent.io/maven/io/confluent/kafka-schema-registry/5.5.0/kafka-schema-registry-5.5.0.jar && \
run wget -P /etc/kafka-connect/jars/ https://jdbc.postgresql.org/download/postgresql-42.2.13.jar
run confluent-hub install --no-prompt confluentinc/kafka-connect-jdbc:latest


#copy connect-log4j.properties /opt/kafka/plugins/connect-log4j.properties
copy connect/*.properties connect/entrypoint.sh /etc/kafka-connect/
#env CLASSPATH=/opt/kafka/plugins/postgresql-42.2.13.jar


#entrypoint ["sh", "/kafka/entrypoint.sh"]

cmd connect-standalone /etc/kafka-connect/worker.properties /etc/kafka-connect/connector0.properties
