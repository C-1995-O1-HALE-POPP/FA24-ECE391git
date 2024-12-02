#/bin/bash
cd ..
rm -rf companion.o
cd ../user
make clean && make
cd ../kern
./mkcomp.sh ../user/bin/zork
make run-elf-test
rm -f companion.o
make clean