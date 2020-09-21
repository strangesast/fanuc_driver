from i386/debian

run apt-get update && apt-get install -y \
  build-essential \
  cmake

workdir /usr/src/app

#run mkdir /fwlib
#copy external/fwlib/*.so.1.0.5 ./fwlib/
#env LD_LIBRARY_PATH="/fwlib:${LD_LIBRARY_PATH}"
#run ldconfig
copy external/fwlib/libfwlib32-linux-x86.so.1.0.5 /usr/local/lib/
run ln -s /usr/local/lib/libfwlib32-linux-x86.so.1.0.5 /usr/local/lib/libfwlib32.so && ldconfig

copy . .

run mkdir build && \
  cd build && \
  cmake .. && \
  make && \
  make install

cmd ["./build/fanuc_driver"]
