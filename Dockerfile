from debian:buster-slim as base
arg TARGETPLATFORM
arg BUILDPLATFORM

copy setup.sh external/fwlib/*.so.1.0.5 /tmp/
run /tmp/setup.sh && ldconfig /lib

from base as builder
run apt-get update && apt-get install -y \
  build-essential \
  cmake

workdir /usr/src/app

copy . .

run mkdir build && \
  cd build && \
  cmake .. && \
  make && \
  make install

from base
copy --from=builder /usr/src/app/build/fanuc_driver /usr/local/bin
cmd ["fanuc_driver"]
