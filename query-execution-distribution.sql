select
	machine_id,
	execution,
	b,
	sum(d)
from (
	select
		a."machine_id",
		a."execution",
		b,
		least(b + interval '5 minutes', "end") - greatest(b, "start") as d
	from generate_series(
			(select date_trunc('hour', min("start")) from executions),
			(select max("end") from executions),
			interval '5 minutes') b
	inner join executions a on (
		(a."end" > b and a."start" < b) or
		(a."start" > b and a."end" < b + interval '5 minutes') or
		(a."start" > b and a."start" < b + interval '5 minutes'))
) t
group by b, machine_id, execution
order by b asc, machine_id, execution;
