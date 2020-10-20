#!/bin/bash
i=0
while true; do
  echo $@ $i
  i=$(($i + 1))
  val=$(($RANDOM % 10 + 1 | bc))
  if (( $val < 2 )); then
    exit 1
  fi
  sleep 1
done
