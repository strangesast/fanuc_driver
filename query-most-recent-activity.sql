select distinct on (machine_id) machine_id, start, duration
from (
	select *
	from (
		select
			*,
			"end" - "start" as duration
		from (
			select
				machine_id,
				execution,
				"start",
				lead("start", 1) over (partition by machine_id order by "start" asc) as "end"
			from (
				select *,
					to_timestamp(timestamp / 1000) at time zone 'America/New_York' as "start"
				from execution
			) t
		) t
		where "end" is not null
	) t
	where execution = 'ACTIVE' and duration > '40 second'
	order by start desc
) t
order by machine_id, start desc
