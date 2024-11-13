#/bin/bash
cd ..
rm -rf companion.o
./mkcomp.sh ../util/mt2
make run-io-lit-test
rm -f companion.o
make clean