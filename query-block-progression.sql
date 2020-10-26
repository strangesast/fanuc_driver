select *
from (
	select
		machine_id,
		to_timestamp(timestamp / 1000) as timestamp,
		unnest(array['program_number', 'program_path_alt', 'mprogram', 'cprogram', 'sequence', 'block', 'block_num']) as "key",
		unnest(array["program_number"::text, "program_path_alt", "mprogram"::text, "cprogram"::text, "sequence"::text, "block", "block_num"::text]) as "value"
	from "input-avro"
	where machine_id = '3c7b5d01-bc91f2d0-5e31c389-7992c4c5'
) t
where "value" is not null
order by timestamp desc
limit 100
