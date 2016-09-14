我从淘宝收购了一个烽火的hg680-j的机顶盒。
cpu是海思hi3798m的四核a7的cpu，内存为1G。
从github找到了内核源码，并编译好了适合跑debian的uImage。
这里提供的是我编译的所有工具以及配置文件，方便自己也方便他人或许以后有用。
你可以直接用buildroot编译initramfs，或者用busybox做initramfs来switch_root到debian（这是我目前的做法）。
如此，一个高性能的四核arm服务器就做好了，省电且非常便宜。

total 164552
-rwxr--r--  1 root root       558 Sep  8 21:57 1.debian.sh
-rwxr--r--  1 root root       661 Sep  8 21:10 1.sh
-rwxr--r--  1 root root       232 Sep  7 20:47 2.sh
drwxrwxr-x 16 root root      4096 Sep  8 17:35 buildroot-2016.08
-rw-r--r--  1 root root   5080974 Sep  1 17:38 buildroot-2016.08.tar.bz2
drwxr-xr-x 36 root root      4096 Sep  7 19:11 busybox-1.25.0
-rw-r--r--  1 root root   2108149 Sep  7 18:18 busybox-1.25.0.tar.bz2
drwxr-xr-x  2 root root      4096 Sep 14 09:07 configs 一些配置文件
drwxr-xr-x 20 root root      4096 Sep 13 16:40 debian (rootfs arm)
drwx------ 25 root root      4096 Sep 14 08:47 hisilicon-kernel-iconbit_hi3798mx 内核源码
-rw-r--r--  1 root root 141840817 Sep  7 18:05 hisilicon-kernel-iconbit_hi3798mx.zip
-rwxr--r--  1 root root       504 Sep  8 20:46 init
-rwxr--r--  1 root root       712 Sep 13 17:11 init_busybox
-rw-r--r--  1 root root       286 Sep  8 21:08 init_hi3798m.sh
drwxr-xr-x  6 root root      4096 Sep 13 17:11 initramfs
-rw-r--r--  1 root root   1943824 Sep 13 17:11 initramfs.cpio.gz
drwxr-xr-x  2 root root      4096 Sep 12 12:12 ko
-rw-r--r--  1 root root   1589302 Sep  7 19:07 ko.zip
drwxr-xr-x  5 root root      4096 Sep  7 18:22 mkbootimg
-rw-r--r--  1 root root       517 Sep 14 09:10 README.md
-rw-r--r--  1 root root   5836800 Sep 14 08:48 recovery.img 生成的引导镜像，利用mkbootimg制作
-rw-r--r--  1 root root   9957376 Sep  8 21:54 recovery.img.buildroot
