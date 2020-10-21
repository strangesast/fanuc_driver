# Serial Monitoring Processor

If MTConnect Adapters use a similar protocol the raw text socket method can be used.

1. Connect to socket on Windows / Linux (hopefully)
2. Split incoming data on \n then | separators
3. Associate socket with machine.  Parse timestamp, split strings into protobuf then into Kafka
4. Interpret split strings as key-value pairs
  a. All but "message" key seem to be odd-even
  b. Above is not perfect / thoroughly tested
5. Use timestamp / subset of key-value pairs to determine activity
  a. "program_comment" and "execution" are promising
6. Stream Kafka topic through streams for kstream / ktable store 
7. Documents into postgres
8. Client proxy through graphql-engine (hasura)
9. Utilize graphql subscriptions for interesting dashboard
