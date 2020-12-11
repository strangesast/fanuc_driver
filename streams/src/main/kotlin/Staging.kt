package main

import com.google.gson.Gson
import io.confluent.kafka.serializers.AbstractKafkaSchemaSerDeConfig
import org.apache.kafka.clients.consumer.ConsumerConfig
import org.apache.kafka.common.serialization.Serde
import org.apache.kafka.common.serialization.Serdes
import org.apache.kafka.streams.KafkaStreams
import org.apache.kafka.streams.StreamsBuilder
import org.apache.kafka.streams.StreamsConfig
import org.slf4j.LoggerFactory
import java.util.*
import java.util.concurrent.CountDownLatch
import kotlin.system.exitProcess
import io.confluent.kafka.streams.serdes.avro.SpecificAvroSerde
import org.apache.avro.Schema
import org.apache.commons.codec.digest.DigestUtils
import org.apache.kafka.streams.KeyValue
import org.apache.kafka.streams.kstream.*
import org.apache.kafka.streams.processor.ProcessorContext
import org.apache.kafka.streams.processor.StateStore
import org.apache.kafka.streams.state.KeyValueStore
import org.apache.kafka.streams.state.SessionStore
import org.apache.kafka.streams.state.Stores
import strangesast.*
import java.text.SimpleDateFormat
import java.time.Duration
import java.time.Instant
import java.util.concurrent.TimeUnit

fun main() {

    val logger = LoggerFactory.getLogger("streams")
    val schemaRegistryUrl = System.getenv("SCHEMA_REGISTRY_URL") ?: "http://localhost:8081"

    val props = Properties()
    props[StreamsConfig.APPLICATION_ID_CONFIG] = System.getenv("STREAMS_APPLICATION_ID") ?: "staging-streams-monitoring"
    props[StreamsConfig.BOOTSTRAP_SERVERS_CONFIG] = System.getenv("KAFKA_HOSTS") ?: "localhost:9092"
    props[StreamsConfig.DEFAULT_KEY_SERDE_CLASS_CONFIG] = Serdes.String().javaClass
    props[StreamsConfig.DEFAULT_VALUE_SERDE_CLASS_CONFIG] = Serdes.String().javaClass
    props[StreamsConfig.PROCESSING_GUARANTEE_CONFIG] = "exactly_once"
    props[AbstractKafkaSchemaSerDeConfig.SCHEMA_REGISTRY_URL_CONFIG] = schemaRegistryUrl
    props[ConsumerConfig.AUTO_OFFSET_RESET_CONFIG] = "earliest"
    props[StreamsConfig.CACHE_MAX_BYTES_BUFFERING_CONFIG] = 0
    /*
    props[StreamsConfig.DEFAULT_DESERIALIZATION_EXCEPTION_HANDLER_CLASS_CONFIG] = "org.apache.kafka.streams.errors.LogAndContinueExceptionHandler"
    */
    props[StreamsConfig.DEFAULT_TIMESTAMP_EXTRACTOR_CLASS_CONFIG] = "org.apache.kafka.streams.processor.UsePartitionTimeOnInvalidTimestamp"

    val gson = Gson()
    val builder = StreamsBuilder()


    logger.info("starting staging...")

    // When you want to override serdes explicitly/selectively
    val serdeConfig = Collections.singletonMap(
        "schema.registry.url",
        schemaRegistryUrl
    )

    val adapterDatumSerde: Serde<strangesast.AdapterDatum> = SpecificAvroSerde()
    adapterDatumSerde.configure(serdeConfig, false)

    val adapterDatumSerSerde: Serde<strangesast.AdapterDatumSer> = SpecificAvroSerde()
    adapterDatumSerSerde.configure(serdeConfig, false)

    val executionDatumSerde: Serde<strangesast.ExecutionDatum> = SpecificAvroSerde()
    executionDatumSerde.configure(serdeConfig, false)

    val partCountDatumSerde: Serde<strangesast.PartCountDatum> = SpecificAvroSerde()
    partCountDatumSerde.configure(serdeConfig, false)

    val sampleCountDatumSerde: Serde<strangesast.SampleCountDatum> = SpecificAvroSerde()
    sampleCountDatumSerde.configure(serdeConfig, false)

    val programDetailDatumSerde: Serde<strangesast.ProgramDetailDatum> = SpecificAvroSerde()
    programDetailDatumSerde.configure(serdeConfig, false)

    val partCountDetailDatumSerde: Serde<strangesast.PartCountDetailDatum> = SpecificAvroSerde()
    partCountDetailDatumSerde.configure(serdeConfig, false)

    val partCompletedEventDatumSerde: Serde<strangesast.PartCompletedEventDatum> = SpecificAvroSerde()
    partCompletedEventDatumSerde.configure(serdeConfig, false)

    val executionWithDurationDatumSerde: Serde<strangesast.ExecutionWithDurationDatum> = SpecificAvroSerde()
    executionWithDurationDatumSerde.configure(serdeConfig, false)

    val partCountStoreBuilder = Stores.keyValueStoreBuilder(
        Stores.persistentKeyValueStore("part_counts"),
        Serdes.String(),
        Serdes.Long()
    ).withCachingEnabled()

    builder.addStateStore(partCountStoreBuilder)

    val executionsStoreBuilder = Stores.keyValueStoreBuilder(
        Stores.persistentKeyValueStore("executions"),
        Serdes.String(),
        Serdes.String()
    )
    builder.addStateStore(executionsStoreBuilder)

    val executionsDurationStoreBuilder = Stores.keyValueStoreBuilder(
        Stores.persistentKeyValueStore("executions_duration"),
        Serdes.String(),
        executionDatumSerde
    )
    builder.addStateStore(executionsDurationStoreBuilder)

    val input = builder
        .stream("input", Consumed.with(Serdes.String(), Serdes.String()))
        .mapValues { str -> gson.fromJson(str, strangesast.AdapterDatum::class.java) }

    val vals = input
        .transform(TransformerSupplier { AdapterDatumTransformer() })

    val format = SimpleDateFormat("yyyy.MM.dd HH:mm:ss.SSS")

    // .groupByKey(Grouped.with(Serdes.String(), partCountDetailDatumSerde))
        // use valuetransformer

    val programsStream = vals
        .filterNot { _, value -> value.getProgram().isNullOrEmpty() }
        .mapValues { value ->
            val contents = value.getProgram()
            val hash = DigestUtils.sha1Hex(contents)
            ProgramDetailDatum.newBuilder()
                .setMachineId(value.machineId)
                .setContents(contents)
                .setHash(hash)
                .setTimestamp(value.getTimestamp())
                .setOffset(value.getOffset())
                .build()
        }

    // programsStream.foreach { key, value ->
    //     println("program $key ${value.getHash()}")
    // }

    // map hash -> program detail
    val machineProgramTable = programsStream
        .toTable(Materialized.with(Serdes.String(), programDetailDatumSerde))

    // map machine_id -> hash
    val programHashTable = programsStream
        .map { _, value -> KeyValue(value.getHash(), value) }
        .toTable(Materialized.with(Serdes.String(), programDetailDatumSerde))

    // vals.to("programs", Produced.with(Serdes.String(), programDetailDatumSerde))

    val partCountStream = vals
        .filter { _, value -> value.partCount != null && value.partCount > 0L}
        .mapValues { value -> PartCountDetailDatum.newBuilder()
            .setMachineId(value.machineId)
            .setOffset(value.getOffset())
            .setTimestamp(value.getTimestamp())
            .setPartCount(value.partCount)
            .build()
        }
        .transformValues(ValueTransformerWithKeySupplier { DedupTransformer("part_counts", KeyValueMapper<String, PartCountDetailDatum, Long> { _, v: PartCountDetailDatum -> v.partCount }) }, "part_counts")
        .filter { _, value -> value != null}

    // partCountStream.foreach { key, value ->
    //     println("part count key $key, timestamp ${format.format(Date.from(Instant.ofEpochMilli(value.getTimestamp())))}, count ${value.partCount}")
    // }

    val partCompletedEvents = partCountStream
        .join( machineProgramTable, { partCount, program ->
            PartCompletedEventDatum.newBuilder()
                .setMachineId(partCount.machineId)
                .setTimestamp(partCount.getTimestamp())
                .setOffset(partCount.getOffset())
                .setPartCount(partCount.partCount)
                .setPartCountTimestamp(partCount.getTimestamp())
                .setProgram(program.getContents())
                .setProgramHash(program.getHash())
                .setProgramTimestamp(program.getTimestamp())
                .build()
        }, Joined.with(Serdes.String(), partCountDetailDatumSerde, programDetailDatumSerde))

    // partCompletedEvents
    //     .foreach { key, value ->
    //         println("key: $key")
    //         println("value: ${gson.toJson(value)}")
    //     }

    val sessionCounts = vals
        .groupByKey(Grouped.with(Serdes.String(), adapterDatumSerSerde))
        .windowedBy(SessionWindows.with(Duration.ofMinutes(5)).grace(Duration.ofMinutes(1)))
        .count(Materialized.`as`("sessions"))
        // .suppress(Suppressed.untilWindowCloses(Suppressed.BufferConfig.unbounded()))

    val executions = vals
        .filter { _, value -> !value.getExecution().isNullOrEmpty() }
        .mapValues { value ->
            ExecutionDatum.newBuilder()
                .setMachineId(value.machineId)
                .setExecution(value.getExecution())
                .setTimestamp(value.getTimestamp())
                .setPartition(value.getPartition())
                .setOffset(value.getOffset())
                .build()
        }
        .transformValues(ValueTransformerWithKeySupplier { DedupTransformer("executions", KeyValueMapper<String, ExecutionDatum, String> { _, v: ExecutionDatum -> v.getExecution() }) }, "executions")
        .filter { _, value -> value != null }

    executions
        .transformValues(ValueTransformerWithKeySupplier { ExecutionDurationTransformer<String>("executions_duration") }, "executions_duration")
        .filter { _, value -> value != null }
        .foreach { key, value ->
            println("machine $key had ${value.getExecution()} for ${value.getDuration()} from ${format.format(Date.from(Instant.ofEpochMilli(value.getStart())))} to ${format.format(Date.from(Instant.ofEpochMilli(value.getEnd())))}")
        }

    executions
        .transformValues(ValueTransformerWithKeySupplier { SessionTransformer<String>("sessions") }, "sessions")

    /*
        .foreach { key, value ->
            val a = Date.from(Instant.ofEpochMilli(value.getTimestamp()))
            val b = Date.from(Instant.ofEpochMilli(value.getSession()))

            println("machine $key timestamp ${format.format(a)} session ${format.format(b)} ${value.getExecution()}")
        }
    */


    // executions.to("execution", Produced.with(Serdes.String(), executionDatumSerde))
    /*
    vals
        .groupByKey(Grouped.with(Serdes.String(), adapterDatumSerSerde))
        .windowedBy(sessionWindow)
        .count()
        .toStream()
        .map { key, count ->
            val w = key.window()
            val machine_id = key.key()
            val start = w.start()
            val end = w.end()
            KeyValue(Pair(machine_id, start), mapOf("machine_id" to machine_id, "start" to start, "end" to end, "count" to count))
        }

    val windows = TimeWindows.of( Duration.ofMinutes(5)).advanceBy(Duration.ofMinutes(1))
    */



    /*
    vals
        .mapValues { value ->
            value.getOffset()
        }
        .filter { _, i ->
            (i and (i - 1)) == 0L
        }
        .foreach { _, i ->
            println("at $i")
        }
    */

    /*
    vals
        .filter { _, value -> value.partCount != null }
        .mapValues { value -> PartCountDatum.newBuilder().setPartCount(value.partCount).setTimestamp(value.getTimestamp()).build() }
        .groupByKey(Grouped.with(Serdes.String(), partCountDatumSerde))
        .aggregate(
            { PartCountDatum() },
            { key, value, agg ->
                if (value.partCount != agg.partCount) {
                    value
                } else {
                    agg
                }
            },
            Materialized.with(Serdes.String(), partCountDatumSerde)
        )
        .toStream()
        .foreach { key, value ->
            val d = Date(value.getTimestamp())
            println("$key ${format.format(d)} ${value.partCount}")
        }
    */

    /*
    vals.to("input-avro", Produced.with(Serdes.String(), adapterDatumSerSerde))


    */

    val topology = builder.build()
    logger.info(topology.describe().toString())
    val streams = KafkaStreams(topology, props)
    val latch = CountDownLatch(1)

    Runtime.getRuntime().addShutdownHook(object : Thread() {
        override fun run() {
            streams.close()
            latch.countDown()
        }
    })
    println("starting...")


    try {
        // temp
        streams.cleanUp()

        streams.start()
        latch.await()
    } catch (e: Throwable) {
        exitProcess(1)
    }
    exitProcess(0)
}

// receives the next execution state and returns the previous with duration added
class ExecutionDurationTransformer<K> constructor(val storeName: String): ValueTransformerWithKey<K, ExecutionDatum, ExecutionWithDurationDatum> {
    private var context: ProcessorContext? = null
    private lateinit var store: KeyValueStore<K, ExecutionDatum>

    override fun init(context: ProcessorContext) {
        this.context = context
        this.store = context.getStateStore(storeName) as KeyValueStore<K, ExecutionDatum>
    }

    override fun transform(readOnlyKey: K, value: ExecutionDatum): ExecutionWithDurationDatum? {
        val storeValue = this.store.get(readOnlyKey)
        this.store.put(readOnlyKey, value)
        if (storeValue == null) {
            return null
        }
        val builder = ExecutionWithDurationDatum.newBuilder()
        val start = storeValue.getTimestamp()
        val end = value.getTimestamp()
        builder.start = start
        builder.end = end
        builder.partition = storeValue.getPartition()
        builder.offset = storeValue.getOffset()
        builder.execution = storeValue.getExecution()
        builder.machineId = storeValue.machineId

        var duration: Long = end - start
        builder.duration = duration

        return builder.build()
    }

    override fun close() {
    }

}


class SessionTransformer<K> constructor(val storeName: String): ValueTransformerWithKey<K, ExecutionDatum, ExecutionWithSessionDatum> {
    private var context: ProcessorContext? = null
    private lateinit var store: SessionStore<K, Long>

    override fun init(context: ProcessorContext) {
        this.context = context
        this.store = context.getStateStore(storeName) as SessionStore<K, Long>
    }

    override fun transform(readOnlyKey: K, value: ExecutionDatum): ExecutionWithSessionDatum {
        val builder = ExecutionWithSessionDatum.newBuilder()
        builder.execution = value.getExecution()
        builder.machineId = value.machineId
        builder.offset = value.getOffset()
        builder.partition = value.getPartition()
        builder.timestamp = value.getTimestamp()

        val iter = this.store.fetch(readOnlyKey)
        if (iter.hasNext()) {
            val session = iter.next()
            val window = session.key.window().start()
            builder.session = window
        } else {
            builder.session = -1
        }
        iter.close()
        return builder.build()
    }

    override fun close() {
    }
}


class DedupTransformer<K, V, E> constructor(val storeName: String, val valueExtractor: KeyValueMapper<K, V, E>): ValueTransformerWithKey<K, V, V> {
    private var context: ProcessorContext? = null
    private lateinit var store: KeyValueStore<K, E>

    override fun init(context: ProcessorContext) {
        this.context = context
        this.store = context.getStateStore(storeName) as KeyValueStore<K, E>
    }

    override fun transform(readOnlyKey: K, value: V): V? {
        val v = valueExtractor.apply(readOnlyKey, value)
        if (v == null) {
            return value
        } else {
            val lastVal = store.get(readOnlyKey)
            if (lastVal != null) {
                if (v == lastVal) {
                    return null
                }
            }
            store.put(readOnlyKey, v)
            return value
        }
    }

    override fun close() {
    }
}