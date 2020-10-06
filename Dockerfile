arg IMAGE=debian
from ${IMAGE} as builder
arg TARGETPLATFORM
run apt-get update && apt-get install -y \
  build-essential \
  cmake \
  wget \
  pkg-config \
  libjansson-dev \
  libsnappy-dev \
  zlib1g-dev \
  liblzma-dev


workdir /tmp
run wget -O /tmp/avro-release-1.10.0.tar.gz https://github.com/apache/avro/archive/release-1.10.0.tar.gz && \
  tar -xf /tmp/avro-release-1.10.0.tar.gz && \
  cd avro-release-1.10.0/lang/c && \
  mkdir build && \
  cd build && \
  cmake .. && \
  make && \
  make test && \
  make install

copy fwlib/${TARGETPLATFORM}/libfwlib32.so.1.0.5 /lib/

workdir /usr/src/app/
copy . .
run ln -s /lib/libfwlib32.so.1.0.5 /lib/libfwlib32.so && \
  ldconfig && \
  gcc -Wall -o hello main.c -lfwlib32 -lavro -pthread -lm && \
  touch focas.log && \
  mkdir /deps && \
  ldd hello | grep "=> /" | awk '{print substr($3, 1, length($3)-2)}' | xargs -I % bash -c "cp %* /deps/" #  && \
  cp /lib/ld-linux* /deps/

#from scratch
#from ${IMAGE}

#copy --from=builder /deps /lib
#copy ./avsc ./avsc
#copy --from=builder  /usr/src/app/focas.log /usr/src/app/hello ./

cmd ["./hello"]
