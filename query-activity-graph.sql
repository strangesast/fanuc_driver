select
  machine_id,
  extract(epoch from "start") * 1000 as "start",
  extract(epoch from "end") * 1000 as "end",
  extract(epoch from "duration") * 1000 as "duration",
  execution,
  r
from (
  select
    machine_id,
    "start",
    coalesce("end", "b") as "end",
    coalesce("end", "b") - "start" as duration,
    execution,
    r
  from (
    select
      machine_id,
      a,
      b,
      r,
      timestamp as "start",
      lead(timestamp, 1) over (partition by machine_id, r order by timestamp) as "end",
      execution
    from (
      select
        machine_id,
        a,
        b,
        r,
        timestamp,
        execution,
        lag(execution, 1) over (partition by machine_id, r order by timestamp asc) as p
      from (
        with executions as (
          select
            machine_id,
            to_timestamp(timestamp / 1000.0) as timestamp,
            execution
          from "input-avro"
          where execution is not null
          order by timestamp asc
        )
        select
          a.machine_id,
          a.a,
          a.b,
          a.r,
          b.timestamp,
          b.execution
        from activity_periods_alt a
        inner join executions b on (
          b.machine_id = a.machine_id and
          a.a <= b.timestamp and
          b.timestamp < a.b)
        ) t
      ) t
      where p is null or execution != p
  ) t
  order by "start" asc
) t
