mount -t myext2 -o loop ./fs.new /mnt
cd /mnt
mknod myfifo p
dmesg | tail
