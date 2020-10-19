from wurstmeister/kafka

run apk add gettext && \
  mkdir -p /kafka/ && \
  mkdir -p /opt/kafka/plugins &&\
  wget -P /opt/kafka/plugins https://packages.confluent.io/maven/io/confluent/kafka-connect-jdbc/5.5.0/kafka-connect-jdbc-5.5.0.jar && \
  wget -P /opt/kafka/plugins https://packages.confluent.io/maven/io/confluent/kafka-connect-avro-converter/5.5.0/kafka-connect-avro-converter-5.5.0.jar && \
  wget -P /opt/kafka/plugins https://packages.confluent.io/maven/io/confluent/common-config/5.5.0/common-config-5.5.0.jar && \
  wget -P /opt/kafka/plugins https://packages.confluent.io/maven/io/confluent/kafka-schema-registry-client/5.5.0/kafka-schema-registry-client-5.5.0.jar && \
  wget -P /opt/kafka/plugins https://packages.confluent.io/maven/io/confluent/kafka-schema-registry/5.5.0/kafka-schema-registry-5.5.0.jar && \
  wget -P /opt/kafka/plugins https://jdbc.postgresql.org/download/postgresql-42.2.13.jar

#copy connect-log4j.properties /opt/kafka/plugins/connect-log4j.properties
copy *.properties entrypoint.sh /kafka/
#env CLASSPATH=/opt/kafka/plugins/postgresql-42.2.13.jar

entrypoint ["sh", "/kafka/entrypoint.sh"]
