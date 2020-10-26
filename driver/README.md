# fwlib FOCAS layer

```
loop:
  on mprogram changes
    get whole program
  on cprogram changes
    get header
  on either changes
    get program_path

  on part_count changes
    get cycle_time

```
