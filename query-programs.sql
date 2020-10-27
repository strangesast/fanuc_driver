select distinct on ("program") program, length(program)
from (
	select
		machine_id,
		to_timestamp(timestamp / 1000) at time zone 'America/New_York',
		regexp_replace("program", '[\n\r]+', ' ', 'g') as "program"
	from "input-avro"
	where "program" is not null
	order by timestamp desc
) t
