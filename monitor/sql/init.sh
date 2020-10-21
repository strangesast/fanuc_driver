#!/bin/bash
set -e

psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "$POSTGRES_DB" <<-EOSQL
  CREATE DATABASE $DATABASE_NAME;
  GRANT ALL PRIVILEGES ON DATABASE $DATABASE_NAME TO postgres;
EOSQL

psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "$DATABASE_NAME" <<-EOSQL
  CREATE ROLE readaccess;
  GRANT USAGE ON SCHEMA public TO readaccess;
  GRANT SELECT ON ALL TABLES IN SCHEMA public TO readaccess;
  ALTER DEFAULT PRIVILEGES IN SCHEMA public GRANT SELECT ON TABLES TO readaccess;

  CREATE USER "user" with password 'password';
  GRANT readaccess TO "user";

  CREATE EXTENSION pgcrypto;

  CREATE TABLE machine_pings (
    id            integer GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    date          timestamp DEFAULT CURRENT_TIMESTAMP,
    machine_id    text NOT NULL,
    machine_ip    text NOT NULL,
    machine_port  integer NOT NULL,
    machine_axes  varchar(4)[]
  );

  CREATE TABLE machines (
    machine_id    text PRIMARY KEY,
    machine_ip    text NOT NULL,
    machine_port  integer NOT NULL,
    modified_at   timestamp DEFAULT CURRENT_TIMESTAMP,
    created_at    timestamp
  );
EOSQL
