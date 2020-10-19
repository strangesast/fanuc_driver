#!/bin/sh
export KAFKA_HOSTS="${KAFKA_HOSTS:-localhost:9092}"
export FLUSH_INTERVAL="${FLUSH_INTERVAL:-10000}"
export DATABASE_URL="${DATABASE_URL:-postgresql://localhost:5432/development}"
export DATABASE_USER="${DATABASE_USER:-postgres}"
export DATABASE_PASSWORD="${DATABASE_PASSWORD:-password}"

envsubst < /kafka/worker.properties > worker.properties
envsubst < /kafka/connector0.properties > connector0.properties

/opt/kafka/bin/connect-standalone.sh worker.properties connector0.properties
