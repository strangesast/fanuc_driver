#!/bin/sh
export KAFKA_HOSTS="${KAFKA_HOSTS:-localhost:9092}"
export FLUSH_INTERVAL="${FLUSH_INTERVAL:-10000}"
export DATABASE_URL="${DATABASE_URL:-postgresql://localhost:5432/development}"
export DATABASE_USER="${DATABASE_USER:-postgres}"
export DATABASE_PASSWORD="${DATABASE_PASSWORD:-password}"

mkdir config
envsubst < worker.properties > config/worker.properties
envsubst < connector0.properties > config/connector0.properties
