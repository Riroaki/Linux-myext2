dd if=/dev/zero of=myfs bs=1M count=1
/sbin/mkfs.ext2 myfs
mount -t myext2 -o loop ./myfs /mnt
cd /mnt
echo "1234567" > test.txt
cat test.txt
cd
umount /mnt
mount -t ext2 -o loop ./myfs /mnt
cd /mnt
cat test.txt
