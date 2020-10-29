package main

import org.apache.kafka.streams.KeyValue
import org.apache.kafka.streams.kstream.Transformer
import org.apache.kafka.streams.processor.ProcessorContext
import strangesast.AdapterDatum
import strangesast.AdapterDatumSer


class AdapterDatumTransformer : Transformer<String, AdapterDatum, KeyValue<String, AdapterDatumSer>> {
    private lateinit var context: ProcessorContext

    override fun init(context: ProcessorContext) {
        this.context = context
    }

    override fun transform(key: String, value: strangesast.AdapterDatum): KeyValue<String, AdapterDatumSer> {
        val offset = context.offset()
        val timestamp = context.timestamp()
        val partition = context.partition()
        return KeyValue(key, convertAdapterDatum(key, value, offset, timestamp, partition))
    }

    override fun close() {
    }
}
