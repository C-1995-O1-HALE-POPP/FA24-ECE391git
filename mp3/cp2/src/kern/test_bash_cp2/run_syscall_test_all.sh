#/bin/bash

cd ..
rm -rf kfs.raw
cd ../user
make clean && make
cd ../util
make clean && make
cp -f ../user/bin/io_test run_me
./mkfs ../kern/kfs.raw ../user/bin/init0 ../user/bin/init1 ../user/bin/init2 \
        ../user/bin/trek ../user/bin/io_test mt2 run_me
cd ../kern
make run-kernel
cd ../..
./clean_all.sh