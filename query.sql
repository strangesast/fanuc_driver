select *
from (
	select
		*,
		start at time zone 'America/New_York',
		stop - "start" as duration
	from (
		select
			machine_id,
			execution,
			to_timestamp(start / 1000) as start,
			to_timestamp(stop / 1000) as stop
		from (
			select
				machine_id,
				timestamp as start,
				execution,
				lead(timestamp, 1) over (partition by machine_id order by timestamp asc) as stop
			from "input-avro"
			where "execution" is not NULL
			order by "timestamp" asc
		) t
		where stop is not NULL
	) t
) t
where execution = 'ACTIVE' and duration > interval '20 second'
limit 100
