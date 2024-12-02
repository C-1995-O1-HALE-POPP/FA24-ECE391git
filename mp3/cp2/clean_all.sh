#bin bash
cd src/kern
make clean
rm -rf *.o *.raw *.elf test_src_cp1/*.o test_src_cp2/*.o
cd ../user
make clean
cd ../util
make clean
