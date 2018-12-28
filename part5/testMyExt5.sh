mount -t myext2 -o loop ./fs.new /mnt/
cd /mnt/
echo "1234567" > test.txt
cat test.txt
cd ~
cat /mnt/test.txt
