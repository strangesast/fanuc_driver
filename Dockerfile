arg IMAGE=debian
from ${IMAGE} as builder
arg TARGETPLATFORM
run apt-get update && apt-get install -y build-essential

copy fwlib/${TARGETPLATFORM}/libfwlib32.so.1.0.5 /lib/

workdir /usr/src/app/
copy . .
run ln -s /lib/libfwlib32.so.1.0.5 /lib/libfwlib32.so && \
  ldconfig && \
  gcc -Wall -o hello main.c -lfwlib32 -pthread  && \
  touch focas.log && \
  mkdir /deps && \
  ldd hello | grep "=> /" | awk '{print substr($3, 1, length($3)-2)}' | xargs -I % bash -c "cp %* /deps/" && \
  cp /lib/ld-linux* /deps/

from scratch

copy --from=builder /deps /lib
copy --from=builder /usr/src/app/focas.log /usr/src/app/hello ./

cmd ["./hello"]