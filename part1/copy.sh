cd linux-4.8/
cd fs/
cd fs
cp -R ext2 myext2
cd ~/linux-4.8/fs/myext2
mv ext2.h myext2.h 

cd /lib/modules/$(uname -r)/build/include/linux 
cp ext2_fs.h myext2_fs.h 
cd /lib/modules/$(uname -r)/build/include/asm-generic/bitops
cp ext2-atomic.h myext2-atomic.h
cp ext2-atomic-setbit.h myext2-atomic-setbit.h
