docker run -e PGPASSWORD=password --network=host --rm -it postgres psql \
  -p 5433 \
  -U postgres \
  -h localhost \
  -d ${1:-testing} \
  -c "select distinct on (machine_id) machine_id, to_timestamp(timestamp / 1000) at time zone 'America/New_York' from \"input-avro\" order by machine_id, timestamp desc"
