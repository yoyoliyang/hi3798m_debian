cp /root/hisilicon-kernel-iconbit_hi3798mx/arch/arm/boot/uImage /root/recovery.dir/kernel -vf
mv recovery.img recovery.img.bak
/root/mkbootimg/mkboot recovery.dir recovery.img
$CL recovery.img
