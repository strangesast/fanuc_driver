select
	machine_id,
	timestamp at time zone 'America/New_York',
	execution,
	coalesce(n, CURRENT_TIMESTAMP) - timestamp as duration
from (
	select
		machine_id,
		timestamp,
		lead(timestamp, 1) over (partition by machine_id order by timestamp asc) as n,
		execution
	from (
		select
			machine_id,
			timestamp,
			execution,
			lead(execution, 1) over (partition by machine_id order by timestamp asc) as n
		from (
			select
				machine_id,
				to_timestamp(timestamp / 1000) as timestamp,
				execution
			from "input-avro"
			where execution is not null
		) t
	) t
	where n != execution
) t
order by timestamp desc
