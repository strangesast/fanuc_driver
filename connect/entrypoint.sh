#!/bin/sh
export KAFKA_HOSTS="${KAFKA_HOSTS:-localhost:9092}"
export FLUSH_INTERVAL="${FLUSH_INTERVAL:-10000}"
export DATABASE_URL="${DATABASE_URL:-postgresql://localhost:5432/development}"
export DATABASE_USER="${DATABASE_USER:-postgres}"
export DATABASE_PASSWORD="${DATABASE_PASSWORD:-password}"

cd /etc/kafka-connect/
envsubst < worker.properties.template > worker.properties
envsubst < connector0.properties.template > connector0.properties
envsubst < connector1.properties.template > connector1.properties

connect-standalone /etc/kafka-connect/worker.properties /etc/kafka-connect/connector0.properties /etc/kafka-connect/connector1.properties
