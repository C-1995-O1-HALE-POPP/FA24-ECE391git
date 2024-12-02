#/bin/bash
cd ..
rm -rf companion.o
cd ../user
make clean && make
cd ../util
make clean && make
./mkfs ../kern/kfs.raw ../user/bin/trek \
        ../user/bin/rogue ../user/bin/zork ../user/bin/hello\
        empty i_am_long read_me write_me mt2
cd ../kern
./mkcomp.sh kfs.raw
make run-io-fs-test
make run-fs-test
rm -f kfs.raw companion.o
make clean