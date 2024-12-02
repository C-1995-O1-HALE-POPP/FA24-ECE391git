#/bin/bash
cd ..
touch kfs.raw
rm -rf companion.o
./mkcomp.sh ../util/mt2
make run-io-lit-test
rm -rf companion.o kfs.raw
make clean