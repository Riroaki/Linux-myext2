cat /proc/filesystems |grep myext2
dd if=/dev/zero of=myfs bs=1M count=1
/sbin/mkfs.ext2 myfs
mount -t myext2 -o loop ./myfs /mnt
mount
umount /mnt
mount -t ext2 -o loop ./myfs /mnt
mount
umount /mnt
rmmod myext2
