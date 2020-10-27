select
	machine_id,
	b,
	sum(case when execution = 'ACTIVE' then sum else interval '0' end) active,
	sum(case when execution = 'STOPPED' then sum else interval '0' end) stopped,
	sum(case when execution = 'INTERRUPTED' then sum else interval '0' end) interrupted,
	sum(case when execution = 'READY' then sum else interval '0' end) ready
from execution_distribution_view
group by machine_id, b
order by b

create or replace function dist(
	dist interval
)
	returns table (
		machine_id text,
		b timestamp with time zone,
		active interval,
		stopped interval,
		interrupted interval,
		ready interval
	)
	language plpgsql
as $$
begin
	return query with ed as (
		select
			t.machine_id,
			t.execution,
			t.b,
			sum(d)
		from (
			select
				a."machine_id",
				a."execution",
				b.b,
				least(b.b + dist, "end") - greatest(b.b, "start") as d
			from generate_series(
					(select date_trunc('hour', min("start")) from executions),
					(select max("end") from executions),
					dist) b
			inner join executions a on (
				(a."end" > b.b and a."start" < b.b) or
				(a."start" > b.b and a."end" < b.b + dist) or
				(a."start" > b.b and a."start" < b.b + dist))
		) t
		group by t.b, t.machine_id, t.execution
		order by t.b asc, t.machine_id, t.execution
	)
	select
		ed.machine_id,
		ed.b,
		sum(case when ed.execution = 'ACTIVE' then sum else interval '0' end) active,
		sum(case when ed.execution = 'STOPPED' then sum else interval '0' end) stopped,
		sum(case when ed.execution = 'INTERRUPTED' then sum else interval '0' end) interrupted,
		sum(case when ed.execution = 'READY' then sum else interval '0' end) ready
	from ed
	group by ed.machine_id, ed.b
	order by ed.b;
end;$$
