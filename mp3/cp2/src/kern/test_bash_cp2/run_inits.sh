#/bin/bash
echo "Running init0 - "
cd ..
rm -rf kfs.raw
cd ../user
make clean && make
cd ../util
make clean && make
rm -rf run_me
cp  ../user/bin/init0 run_me
./mkfs ../kern/kfs.raw ../user/bin/init0 ../user/bin/init1 ../user/bin/init2 ../user/bin/trek \
        ../user/bin/io_test mt2 run_me
cd ../kern
make run-kernel
cd ../..
./clean_all.sh
cd src/kern/test_bash_cp2
echo "no compliation warnings, i throw away the output for future runs "
echo "Running init1 - "
cd ..
rm -rf kfs.raw
cd ../user
make clean && make > /dev/null
cd ../util
make clean && make > /dev/null
rm -rf run_me
cp -f ../user/bin/init1 run_me
./mkfs ../kern/kfs.raw ../user/bin/init0 ../user/bin/init1 ../user/bin/init2 \
        ../user/bin/trek ../user/bin/io_test mt2 run_me > /dev/null
cd ../kern
make run-kernel > /dev/null
cd ../..
./clean_all.sh
cd src/kern/test_bash_cp2

echo "Running init2 - "
cd ..
rm -rf kfs.raw
cd ../user
make clean && make > /dev/null
cd ../util
make clean && make > /dev/null
rm -rf run_me
cp -f ../user/bin/init2 run_me
./mkfs ../kern/kfs.raw ../user/bin/init0 ../user/bin/init1 ../user/bin/init2 \
        ../user/bin/trek ../user/bin/io_test mt2 run_me > /dev/null
rm -rf run_me
cd ../kern
make run-kernel > /dev/null
cd ../..
./clean_all.sh
cd src/kern/test_bash_cp2