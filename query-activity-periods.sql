create materialized view if not exists samples as select machine_id, to_timestamp(timestamp / 1000.0) as ts from "input-avro";
create index on samples (ts);

drop materialized view if exists machine_counts;
create materialized view machine_counts as with ids as (select distinct on (machine_id) machine_id from "input-avro"),
series as (select a, coalesce(lead(a, 1) over (order by a), CURRENT_TIMESTAMP) as b from generate_series(
	(select date_trunc('hour', min(ts)) from tbl),
	(select max(ts) from tbl),
	interval '30 minutes') a)
select
	a.machine_id,
	a.a,
	a.b,
	count(*)
from (
	select
		machine_id,
		a,
		b
	from series
	cross join ids
) a
left join samples b on (b.machine_id = a.machine_id and a.a <= b.ts and b.ts < a.b)
group by a.a, a.b, a.machine_id
order by a.a, a.machine_id

select *, b - a as diff
from (
	select
		machine_id,
		min(a) as a,
		max(b) as b,
		cond
	from (
		select
			machine_id,
			a,
			b,
			count,
			cond,
			sum(r) over (partition by machine_id order by a) as r
		from (
			select
				*,
				case when pcond is null or cond != pcond then 1 else 0 end as r
			from (
				select
					machine_id,
					a,
					b,
					count,
					cond,
					lag(cond, 1) over (partition by machine_id order by a asc) as pcond
				from (
					select
						machine_id,
						a,
						b,
						count,
						count < 1000 as cond
					from machine_counts
				) t
			) t
		) t
	) t
	group by machine_id, r, cond
) t
order by machine_id, a asc

/* periods with sufficient samples */
select *, b - a as diff
from (
	select
		machine_id,
		min(a) as a,
		max(b) as b,
		cond
	from (
		select
			machine_id,
			a,
			b,
			count,
			cond,
			sum(r) over (partition by machine_id order by a) as r
		from (
			select
				*,
				case when pcond is null or cond != pcond then 1 else 0 end as r
			from (
				select
					machine_id,
					a,
					b,
					count,
					cond,
					lag(cond, 1) over (partition by machine_id order by a asc) as pcond
				from (
					select
						machine_id,
						a,
						b,
						count,
						count < 1000 as cond
					from machine_counts
				) t
			) t
		) t
	) t
	group by machine_id, r, cond
) t
where cond = false
order by a asc, machine_id
