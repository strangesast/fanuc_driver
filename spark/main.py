from pyspark import SparkContext
from pyspark.sql import SparkSession
from pyspark.streaming import StreamingContext

spark = SparkSession.builder.appName("Spark-Kafka-Integration").master("local").getOrCreate()
df = spark.readStream.format("kafka").option("kafka.bootstrap.servers", "host1:port1,host2:port2").option("subscribe", "input").option("includeHeaders", "true").load()
