dd if=/dev/zero of=myfs bs=1M count=1
./mkfs.myext2 myfs
sudo mount -t myext2  -o loop ./myfs  /mnt
mount
