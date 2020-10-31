with tbl as (
	select
		machine_id,
		to_timestamp(timestamp / 1000) as timestamp,
		program,
		row_number() over (partition by machine_id order by timestamp asc) as r
	from "input-avro"
	where program is not null
)
select
	a.machine_id,
	a.timestamp,
	a.part_count,
	b.timestamp as program_timestamp,
	regexp_replace("program", '[\n\r]+', ' ', 'g') as "program"
from (
	select
		machine_id,
		timestamp,
		part_count
	from (
		select
			machine_id,
			to_timestamp(timestamp / 1000) as timestamp,
			part_count,
			lag(part_count, 1) over (partition by machine_id order by timestamp asc) as p
		from "input-avro"
		where part_count is not null and part_count > 0
	) t
	where part_count > p
) a
join tbl b on a.machine_id = b.machine_id and b.r = (
	select max(r) from tbl
	where tbl.machine_id = a.machine_id and tbl.timestamp < a.timestamp
)
order by a.machine_id, a.timestamp desc
