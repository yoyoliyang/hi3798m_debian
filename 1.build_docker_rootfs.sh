BRTG=/root/buildroot-2016.08/output
echo "export WG=wget ftp://root:pswd@192.168.0.30/" >> $BRTG/target/etc/profile
echo "export CL=curl -u root:pswd ftp://192.168.137.1 -T" >> $BRTG/target/etc/profile
export CL=curl -u root:pswd ftp://192.168.137.1 -T
echo del dir.
rm -rv $BRTG/target/dev
rm -rv $BRTG/target/etc/hosts
rm -rv $BRTG/target/etc/hostname
rm -rv $BRTG/target/etc/resolv.conf
rm -rv $BRTG/target/opt
rm -rv $BRTG/target/media
rm -rv $BRTG/target/proc
rm -rv $BRTG/target/run
rm -rv $BRTG/target/sys
cd $BRTG && tar cf target.tar target/ && cd /root
$CL $BRTG/target.tar
rm $BRTG/target.tar
ls $BRTG/target

