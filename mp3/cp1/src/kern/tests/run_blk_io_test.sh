#/bin/bash
cd ..
rm -rf companion.o kfs_out.raw
cd ../user
make clean && make
cd ../util
make clean && make
cp mt2 ../kern/kfs.raw
cd ../kern
make run-io-blk-test
cp kfs.raw kfs_out.raw
cd ../util
rm -f ../kern/kfs.raw
./mkfs ../kern/kfs.raw ../user/bin/trek \
        ../user/bin/rogue ../user/bin/zork ../user/bin/hello\
        empty i_am_long read_me write_me mt2
cd ../kern
make run-shell

rm -f io_test.raw companion.o
make clean