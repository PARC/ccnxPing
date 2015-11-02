ccnx_perf
============

CCNx Performance Tool

To build it:
  > autoreconf
  > ./configure --prefix=<where to install> \
         --with-longbow=<path_to_longbow> \
         --with-libparc=<path_to_libparc> \
         --with-libccnx=<path_to_libccnx> \
         --enable-debug
  > make 
  > make install

