rm target.tar target -r
wget ftp://root:pswd@192.168.137.1/busybox-armv7l -O /busybox && chmod 755 /busybox
wget ftp://root:pswd@192.168.137.1/target.tar && tar xf target.tar
/busybox rm -r /bin /sbin /usr
/busybox mv -n /target/* /
/busybox cp -af /target/etc/* /etc/
/busybox sync
rm /busybox
rm target.tar target -r
echo done
