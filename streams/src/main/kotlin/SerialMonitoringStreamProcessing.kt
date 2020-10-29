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
import org.apache.kafka.streams.KeyValue
import org.apache.kafka.streams.kstream.*
import org.apache.kafka.streams.processor.ProcessorContext
import strangesast.SampleCountDatum
import java.text.SimpleDateFormat
import java.time.Duration
import java.util.concurrent.TimeUnit

val FIELDS = listOf(
    "max_axis", "addinfo", "cnc_type", "mt_type", "series", "version",
    "axes_count_chk", "axes_count", "ether_type", "ether_device", "axes",
    "alarm", "hdck", "tool_group", "message_number", "message_text", "mprogram",
    "part_count", "mode", "estop", "aut", "emergency", "run", "execution",
    "tool_id", "sequence", "cprogram", "program_number", "program_header",
    "mstb", "edit", "block", "motion", "actf", "acts", "actual", "relative",
    "absolute", "load"
)

fun main() {

    val logger = LoggerFactory.getLogger("streams")
    val schemaRegistryUrl = System.getenv("SCHEMA_REGISTRY_URL") ?: "http://localhost:8081"

    val props = Properties()
    props[StreamsConfig.APPLICATION_ID_CONFIG] = System.getenv("STREAMS_APPLICATION_ID") ?: "alt-alt-streams-monitoring"
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

    logger.info("starting...")

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

    val input = builder
        .stream("input", Consumed.with(Serdes.String(), Serdes.String()))
        .mapValues { str -> gson.fromJson(str, strangesast.AdapterDatum::class.java) }

    val vals = input
        .transform(TransformerSupplier { AdapterDatumTransformer() })


    // val format = SimpleDateFormat("yyyy.MM.dd HH:mm")

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


    val inputGroup = input
        .groupByKey(Grouped.with(Serdes.String(), adapterDatumSerde))

    val inputSessions = inputGroup.windowedBy(SessionWindows.with(Duration.ofMinutes(5))).count()
        .suppress(Suppressed.untilWindowCloses(Suppressed.BufferConfig.unbounded()))

    inputSessions.mapValues { readOnlyKey, value ->
        SampleCountDatum.newBuilder()
            .setCount(value)
            .setWindowStart(readOnlyKey.window().start())
            .setWindowEnd(readOnlyKey.window().end())
            .setMachineId(readOnlyKey.key())
            .build()
    }
        .toStream()
        .selectKey { key, _ ->  key.key()}
        .to("counts-sessions", Produced.with(Serdes.String(), sampleCountDatumSerde))

    inputGroup.windowedBy(TimeWindows.of(Duration.ofMinutes(1)).advanceBy(Duration.ofMinutes(1)))
        .count()
        .suppress(Suppressed.untilWindowCloses(Suppressed.BufferConfig.unbounded()))
        .mapValues { readOnlyKey, value ->
            SampleCountDatum.newBuilder()
                .setCount(value)
                .setWindowStart(readOnlyKey.window().start())
                .setWindowEnd(readOnlyKey.window().end())
                .setMachineId(readOnlyKey.key())
                .build()
        }
        .toStream()
        .selectKey { key, _ -> key.key() }
        .to("counts", Produced.with(Serdes.String(), sampleCountDatumSerde))
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

    vals.to("input-avro", Produced.with(Serdes.String(), adapterDatumSerSerde))

    val executions = vals
        .filterNot { _, value -> value.getExecution().isNullOrEmpty() }
        .mapValues { value ->
            strangesast.ExecutionDatum.newBuilder()
                .setMachineId(value.machineId)
                .setExecution(value.getExecution())
                .setTimestamp(value.getTimestamp())
                .setPartition(value.getPartition())
                .setOffset(value.getOffset())
                .build()
        }

    executions.to("execution", Produced.with(Serdes.String(), executionDatumSerde))

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
        streams.start()
        latch.await()
    } catch (e: Throwable) {
        exitProcess(1)
    }
    exitProcess(0)
}

fun convertAdapterDatum(
    machineID: String,
    datum: strangesast.AdapterDatum,
    offset: Long,
    timestamp: Long,
    partition: Int
): strangesast.AdapterDatumSer {
    val gson = Gson()
    val out = strangesast.AdapterDatumSer.newBuilder()
    out.machineId = machineID
    out.offset = offset
    out.timestamp = timestamp
    out.partition = partition
    val _meta = datum.getMeta()
    val _in = datum.getValues()
    out.maxAxis = _in.maxAxis
    out.addinfo = _in.getAddinfo()
    out.cncType = _in.cncType
    out.mtType = _in.mtType
    out.series = _in.getSeries()
    out.version = _in.getVersion()
    out.axesCountChk = _in.axesCountChk
    out.axesCount = _in.axesCount
    out.etherType = _in.etherType
    out.etherDevice = _in.etherDevice
    if (!_in.getAxes().isNullOrEmpty()) out.axes = gson.toJson(_in.getAxes())
    out.alarm = _in.getAlarm()
    out.hdck = _in.getHdck()
    out.toolGroup = _in.toolGroup
    out.messageNumber = _in.messageNumber
    out.messageText = _in.messageText
    out.mprogram = _in.getMprogram()
    out.partCount = _in.partCount
    out.mode = _in.getMode()
    out.estop = _in.getEstop()
    out.aut = _in.getAut()
    out.emergency = _in.getEmergency()
    out.run = _in.getRun()
    out.execution = _in.getExecution()
    out.toolId = _in.toolId
    out.sequence = _in.getSequence()
    out.cprogram = _in.getCprogram()
    out.programNumber = _in.programNumber
    out.programHeader = _in.programHeader
    out.mstb = _in.getMstb()
    out.edit = _in.getEdit()
    out.block = _in.getBlock()
    out.motion = _in.getMotion()
    out.actf = _in.getActf()
    out.acts = _in.getActs()

    if (!_in.getActual().isNullOrEmpty()) {
        out.actual = gson.toJson(_in.getActual())
    }
    if (!_in.getRelative().isNullOrEmpty()) {
        out.relative = gson.toJson(_in.getRelative())
    }
    if (!_in.getAbsolute().isNullOrEmpty()) {
        out.absolute = gson.toJson(_in.getAbsolute())
    }
    if (!_in.getLoad().isNullOrEmpty()) {
        out.load = gson.toJson(_in.getLoad())
    }
    out.blockNum = _in.blockNum
    out.cycleTime = _in.cycleTime
    out.programName = _in.programName
    out.programPath = _in.programPath
    out.program = _in.getProgram()
    out.programSize = _in.programSize

    out.metaInfo = _meta.get("info")
    out.metaPartCount = _meta.get("part_count")
    out.metaStatus = _meta.get("status")
    out.metaTool = _meta.get("tool")
    out.metaDynamic = _meta.get("dynamic")
    out.metaMessage = _meta.get("message")
    out.metaProgramName = _meta.get("program_name")
    out.metaProgramHeader = _meta.get("program_header")
    out.metaProgram = _meta.get("program")
    out.metaBlock = _meta.get("block")
    out.metaTotal = _meta.get("total")

    return out.build()
}
