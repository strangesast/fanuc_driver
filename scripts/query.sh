docker run -e PGPASSWORD=password --network=host --rm -it postgres psql \
  -p 5433 \
  -U postgres \
  -h localhost \
  -d ${1:-testing}
