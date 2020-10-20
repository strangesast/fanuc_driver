from strangesast/fwlib as base

run apt-get update && apt-get install -y librdkafka-dev:i386

from base as builder

copy ./driver/external/fwlib/build-deps.sh /tmp/
run /tmp/build-deps.sh

workdir /usr/src/app

copy driver/ .

run mkdir build && \
  cd build && \
  cmake .. && \
  make && \
  make install

from base

copy --from=builder /usr/src/app/build/fanuc_driver /usr/local/bin
cmd ["fanuc_driver"]
