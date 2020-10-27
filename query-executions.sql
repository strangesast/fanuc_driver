select
	machine_id,
	"start",
	"end",
	coalesce("end", CURRENT_TIMESTAMP) - "start" as duration,
	execution
from (
	select
		machine_id,
		timestamp as "start",
		lead(timestamp, 1) over(partition by machine_id order by timestamp asc) as "end",
		execution
	from (
		select
			machine_id,
			timestamp,
			execution,
			p
		from (
			select
				machine_id,
				timestamp,
				execution,
				lag(execution, 1) over (partition by machine_id order by timestamp asc) as p
			from (
				select
					machine_id,
					to_timestamp(timestamp / 1000.0) as timestamp,
					execution
				from "input-avro"
				where execution is not null
			) t
		) t
		where p is null or execution != p
	) t
) t
order by "start" asc;
