#from wurstmeister/kafka
from confluentinc/cp-kafka-connect:6.0.0

user root
run yum install -y gettext
user appuser
run wget -P /etc/kafka-connect/jars/ https://jdbc.postgresql.org/download/postgresql-42.2.13.jar
run confluent-hub install --no-prompt confluentinc/kafka-connect-jdbc:latest

copy connect/*.properties.template connect/entrypoint.sh /etc/kafka-connect/

#cmd connect-standalone /etc/kafka-connect/worker.properties /etc/kafka-connect/connector0.properties
cmd /etc/kafka-connect/entrypoint.sh
