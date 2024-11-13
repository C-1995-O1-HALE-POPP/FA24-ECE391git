#/bin/bash
cd ..
rm -rf companion.o
cd ../user
make clean && make
cd ../kern
./mkcomp.sh ../user/bin/rogue
make run-elf-test-debug
rm -f companion.o
make clean