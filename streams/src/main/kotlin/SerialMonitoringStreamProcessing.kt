package main

import AdapterDatum
import AdapterDatumSer
import com.google.gson.Gson
import io.confluent.kafka.serializers.AbstractKafkaSchemaSerDeConfig
import org.apache.kafka.clients.consumer.ConsumerConfig
import org.apache.kafka.common.serialization.Serde
import org.apache.kafka.common.serialization.Serdes
import org.apache.kafka.streams.KafkaStreams
import org.apache.kafka.streams.StreamsBuilder
import org.apache.kafka.streams.StreamsConfig
import org.apache.kafka.streams.kstream.Consumed
import org.apache.kafka.streams.kstream.Produced
import org.slf4j.LoggerFactory
import java.util.*
import java.util.concurrent.CountDownLatch
import kotlin.system.exitProcess
import io.confluent.kafka.streams.serdes.avro.SpecificAvroSerde
import org.apache.avro.Schema
import org.apache.kafka.streams.KeyValue
import org.apache.kafka.streams.kstream.Transformer
import org.apache.kafka.streams.kstream.TransformerSupplier
import org.apache.kafka.streams.processor.ProcessorContext

val FIELDS = listOf(
    "max_axis", "addinfo", "cnc_type", "mt_type", "series", "version",
    "axes_count_chk", "axes_count", "ether_type", "ether_device", "axes",
    "alarm", "hdck", "tool_group", "message_number", "message_text", "mprogram",
    "part_count", "mode", "estop", "aut", "emergency", "run", "execution",
    "tool_id", "sequence", "cprogram", "program_number", "program_header",
    "mstb", "edit", "block", "motion", "actf", "acts", "actual", "relative",
    "absolute", "load"
)
val gson = Gson()

fun main() {

    val logger = LoggerFactory.getLogger("streams")
    val schemaRegistryUrl = "http://localhost:8081"

    val props = Properties()
    props[StreamsConfig.APPLICATION_ID_CONFIG] = System.getenv("STREAMS_APPLICATION_ID") ?: "alt-streams-monitoring"
    props[StreamsConfig.BOOTSTRAP_SERVERS_CONFIG] = System.getenv("KAFKA_HOSTS") ?: "localhost:9092"
    props[StreamsConfig.DEFAULT_KEY_SERDE_CLASS_CONFIG] = Serdes.String().javaClass
    props[StreamsConfig.DEFAULT_VALUE_SERDE_CLASS_CONFIG] = Serdes.String().javaClass
    props[AbstractKafkaSchemaSerDeConfig.SCHEMA_REGISTRY_URL_CONFIG] = schemaRegistryUrl


    props[ConsumerConfig.AUTO_OFFSET_RESET_CONFIG] = "earliest"
    props[StreamsConfig.CACHE_MAX_BYTES_BUFFERING_CONFIG] = 0

    val builder = StreamsBuilder()

    logger.info("starting...")

    // When you want to override serdes explicitly/selectively
    val serdeConfig = Collections.singletonMap(
        "schema.registry.url",
        schemaRegistryUrl
    )

    val valueSpecificAvroSerde: Serde<AdapterDatumSer> = SpecificAvroSerde()
    valueSpecificAvroSerde.configure(serdeConfig, false)

    val input = builder
        .stream("input", Consumed.with(Serdes.String(), Serdes.String()))

    input
        .mapValues { str -> gson.fromJson(str, AdapterDatum::class.java) }
        .transform(TransformerSupplier { AdapterDatumTransformer() })
        .to("input-avro", Produced.with(Serdes.String(), valueSpecificAvroSerde))

    /*
    input.map { key, str ->
        val d = gson.fromJson(str, AdapterDatum::class.java)
        KeyValue(key, convertAdapterDatum(key, d))
    }
    */

    // t.foreach { key, value ->
    //     logger.info("key=$key value=${value.toString()}")
    // }

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

    try {
        streams.start()
        latch.await()
    } catch (e: Throwable) {
        exitProcess(1)
    }
    exitProcess(0)
}

class AdapterDatumTransformer : Transformer<String, AdapterDatum, KeyValue<String, AdapterDatumSer>> {
    private lateinit var context: ProcessorContext

    override fun init(context: ProcessorContext) {
        this.context = context
    }

    override fun transform(key: String, value: AdapterDatum): KeyValue<String, AdapterDatumSer> {
        val offset = context.offset()
        val timestamp = context.timestamp()
        val partition = context.partition()
        return KeyValue(key, convertAdapterDatum(key, value, offset, timestamp, partition))
    }

    override fun close() {
    }
}

fun convertAdapterDatum(machineID: String, datum: AdapterDatum, offset: Long, timestamp: Long, partition: Int): AdapterDatumSer {
    val out = AdapterDatumSer.newBuilder()
    out.setMachineId(machineID)
    out.setOffset(offset)
    out.setTimestamp(timestamp)
    out.setPartition(partition)
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
    if (!_in.getAxes().isNullOrEmpty()) {
        out.axes = gson.toJson(_in.getAxes())
    }
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

    out.metaPartCount = _meta.getOrDefault("part_count", -1)
    out.metaStatus = _meta.getOrDefault("status", -1)
    out.metaTool = _meta.getOrDefault("tool", -1)
    out.metaDynamic = _meta.getOrDefault("dynamic", -1)
    out.metaMessage = _meta.getOrDefault("message", -1)
    out.metaBlock = _meta.getOrDefault("block", -1)
    out.metaTotal = _meta.getOrDefault("total", -1)
    return out.build()
}

data class ConnectSchemaField(
    val type: String,
    val optional: Boolean,
    val field: String
)

data class ConnectSchemaAndPayload(
    val schema: ConnectSchema,
    val payload: Any
)

data class ConnectSchema(
    val type: String,
    val fields: List<ConnectSchemaField>,
    val optional: Boolean,
    val name: String
)

/*
fun buildStreamsTopology(raw: KStream<String, String>) {
    // val splitSampleSerde = AvroMessageSerde(SplitSample.getEncoder(), SplitSample.getDecoder())
    val sampleKeySerde = AvroMessageSerde(SampleKey.getEncoder(), SampleKey.getDecoder())
    val sampleValueSerde = AvroMessageSerde(SampleValue.getEncoder(), SampleValue.getDecoder())

    val (tabledStream, streamed) = splitSamples.branch(
        Predicate { _, sample ->
            sample.getKey() in setOf(
                "execution",
                "part_count",
                "avail",
                "estop",
                "mode",
                "active_axis",
                "tool_id",
                "program",
                "program_comment",
                "line",
                "block" /* ehhhh */,
                "Fovr",
                "message",
                "servo",
                "comms",
                "logic",
                "motion",
                "system",
                "Xtravel",
                "Xoverheat",
                "Xservo",
                "Ztravel",
                "Zoverheat",
                "Zservo",
                "Ctravel",
                "Coverheat",
                "Cservo",
                "S1servo",
                "S2servo"
            )
        },
        Predicate { _, sample ->
            sample.getKey() in setOf(
                "Xload",
                "Zload",
                "Cload",
                "S1load",
                "S2load",
                "Zact",
                "Xact",
                "Cact",
                "S2speed",
                "S1speed",
                "path_position",
                "path_feedrate"
            )
        }
    )

    val tabled = tabledStream
        .map { key, value ->
            KeyValue(
                SampleKey.newBuilder().setMachineID(key).setProperty(value.getKey()).build(),
                SampleValue.newBuilder().setValue(value.getValue()).setOffset(value.getOffset())
                    .setTimestamp(value.getTimestamp()).build()
            )
        }
        .toTable(Materialized.with(sampleKeySerde, sampleValueSerde))

    /*
    val (executionState, partCountState, programCommentState) = tabled.toStream().branch(
            Predicate { key, _ -> key.getProperty() == "execution" },
            Predicate { key, _ -> key.getProperty() == "part_count" },
            Predicate { key, _ -> key.getProperty() == "program_comment"}
    )
     */


    // program comment regex
    // val pat0 = Pattern.compile("^(%\\s{0,2})?(?<num>O[0-9]{3,6})\\s*(?<notes>(\\([\\s\\w\\- \\.\\/]+\\)\\s{0,3})+)")
    // val pat1 = Pattern.compile("\\([\\s\\w\\-\\.\\/]+\\)")

    tabled.toStream().map { k, v ->
        KeyValue(
            gson.toJson(
                ConnectSchemaAndPayload(
                    ConnectSchema(
                        "struct",
                        listOf(
                            ConnectSchemaField(
                                "string",
                                false,
                                "machine_id"
                            ),
                            ConnectSchemaField(
                                "string",
                                false,
                                "property"
                            ),
                            ConnectSchemaField(
                                "int64",
                                false,
                                "offset"
                            )
                        ),
                        false,
                        "machine_state_key"
                    ),
                    mapOf("machine_id" to k.getMachineID(), "property" to k.getProperty(), "offset" to v.getOffset())
                )
            ),
            gson.toJson(
                ConnectSchemaAndPayload(
                    ConnectSchema(
                        "struct",
                        listOf(
                            ConnectSchemaField(
                                "string",
                                true,
                                "value"
                            ),
                            ConnectSchemaField(
                                "int64",
                                true,
                                "timestamp"
                            )
                        ),
                        false,
                        "machine_state_value"
                    ),
                    mapOf("value" to v.getValue(), "timestamp" to v.getTimestamp())
                )
            )
        )
    }.to("machine_state", Produced.with(Serdes.String(), Serdes.String()))

    streamed.map { machineId, record ->
        KeyValue(
            "$machineId-${record.getKey()}", gson.toJson(
                ConnectSchemaAndPayload(
                    ConnectSchema(
                        "struct",
                        listOf(
                            ConnectSchemaField(
                                "string",
                                false,
                                "machine_id"
                            ),
                            ConnectSchemaField(
                                "string",
                                false,
                                "property"
                            ),
                            ConnectSchemaField(
                                "int64",
                                false,
                                "timestamp"
                            ),
                            ConnectSchemaField(
                                "string",
                                false,
                                "value"
                            ),
                            ConnectSchemaField(
                                "int64",
                                false,
                                "offset"
                            )
                        ),
                        false,
                        "machine_values_value"
                    ),
                    mapOf(
                        "machine_id" to machineId,
                        "property" to record.getKey(),
                        "timestamp" to record.getTimestamp(),
                        "value" to record.getValue(),
                        "offset" to record.getOffset()
                    )
                )
            )
        )
    }.to("machine_values", Produced.with(Serdes.String(), Serdes.String()))
}

*/
