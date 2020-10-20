#!/bin/sh
export KAFKA_HOSTS="${KAFKA_HOSTS:-localhost:9092}"
export FLUSH_INTERVAL="${FLUSH_INTERVAL:-10000}"
export DATABASE_URL="${DATABASE_URL:-postgresql://localhost:5432/development}"
export DATABASE_USER="${DATABASE_USER:-postgres}"
export DATABASE_PASSWORD="${DATABASE_PASSWORD:-password}"

cd /etc/kafka-connect/
envsubst < lib/worker.properties > worker.properties
envsubst < lib/connector0.properties > connector0.properties

connect-standalone /etc/kafka-connect/worker.properties /etc/kafka-connect/connector0.properties
