gcc -o changeMN changeMN.c
dd if=/dev/zero of=myfs bs=1M count=1/sbin/mkfs.ext2 myfs./changeMN myfs
mount -t myext2 -o loop ./fs.new /mnt
mount
sudo umount /mnt
sudo mount -t ext2 -o loop ./fs.new /mnt
rmmod myext2
