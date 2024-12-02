#/bin/bash

##### gdb instructions #####
# (gdb) target remote localhost:25000
# (gdb) b process_exec - kernel mapping before elf load
# (gdb) b thread_jump_to_user - kernel mapping after elf load


cd ..
touch kfs.raw
ehco 'test illegal inst'
make test-illegal-inst
ehco 'test illegal read'
make test-illegal-read
ehco 'test illegal write'
make test-illegal-write
make clean
ehco 'test user/kernel mapping'
cd ../user
make clean && make
cd ../util
make clean && make
rm -rf run_me
cp ../user/bin/paging run_me
./mkfs ../kern/kfs.raw run_me
cd ../kern
make debug-kernel
cd ../..
./clean_all.sh