#bin bash
cd src/kern
make clean
rm -rf *.o *.raw
cd ../user
make clean
cd ../util
make clean