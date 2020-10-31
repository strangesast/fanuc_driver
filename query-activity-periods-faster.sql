drop view activity_periods_alt;
create view activity_periods_alt as select
	machine_id,
	a,
	b,
	b - a as duration,
	row_number() over (partition by machine_id order by a asc) as r,
	cnt
from (
	select
		machine_id,
		min(a) as a,
		max(b) as b,
		sum(cnt) as cnt
	from (
		select
			machine_id,
			t as a,
			t + interval '5 minute' as b,
			sum(r) over (partition by machine_id order by t) as g,
			cnt
		from (
			select
				machine_id,
				t,
				case when pt is null or pt + interval '5 minute' < t then 1 else 0 end as r,
				cnt
			from (
				select
					machine_id,
					t,
					lag(t, 1) over (partition by machine_id order by t asc) as pt,
					cnt
				from (
					select
						machine_id,
						t0 + interval '5 minute' * t1 as t,
						cnt
					from (
						select
							machine_id,
							date_trunc('hour', timestamp) as t0,
							extract(minute from timestamp)::int / 5 as t1,
							count(*) as cnt
						from (
							select machine_id, to_timestamp(timestamp / 1000.0) as timestamp
							from "input-avro"
						) t
						group by machine_id, t0, t1
					) t
				) t
			) t
		) t
	) t
	group by machine_id, g
) t
order by a asc, machine_id
