## 这是16年的时候折腾的笔记，现在附加到github，凑合着看吧。

hi3798m_debian 安装install笔记<https://github.com/yoyoliyang/hi3798m_debian/blob/master/hi3798m_install_docker>
### hi3798m机顶盒研究初始
有个网友送了台hi3798m的box板子。带usb3.0。
他给我的板子，已经把emmc上的引导刷坏，故只能usb引导。但是用我原来的板子（这里成usb2.0）上的fastboot.bin无法引导，老板子已经被我卖了。
通过咸鱼，有个朋友给我发送了一个fastboot.bin文件，是usb3.0盒子用的，能成功引导。

![image](https://github.com/yoyoliyang/hi3798m_debian/blob/master/20161108-065937.png)

这个板子的配置很高。usb3.0，而且自带pcie接口的wifi模块（rtl8192eu），板载天线。
发现emmc的分区是通过bootloader来传导的，只需修改内核配置文件的引导参数便可以修改。用emmc跑rootfs会快很多。4g的大小，也够用了。

### hi3798m机顶盒继续研究
先提供两个打包和解包initramfs的脚本：
```
  root@osboxes:~/hi3798m# cat 1.sh
  #!/bin/bash
  cd initramfs
  rm ../initramfs.cpio.gz
  #find . | cpio -H newc -o > ../initramfs.cpio.gz
  #find . | cpio -H newc -o | gzip -9 > ../initramfs.cpio.gz
  find . | cpio -H newc -o | xz -9 --format=lzma > ../initramfs.cpio.gz #超高压缩

  root@osboxes:~/hi3798m# cat 2.sh
  #!/bin/bash
  rm boot.img.debian
  ../mkbootimg/mkbootimg --base 0x03000000 --kernel ./boot_img_dir/boot.img-zImage --ramdisk initramfs.cpio.gz -o boot.img.debian
  mv boot.img.debian recovery.img
  curl -u root:pswd -T recovery.img ftp:192.168.0.30/
```
安卓系统搞定了，但是依旧想折腾下，顺便学习linux基础知识。

目前用buildroot来做了一个ramdisk，直接跑在内存中。
注意：创建好后，需要手动制作/init文件，如下：
```
  #!/bin/sh
  insmod /ko/ath6kl_usb.ko
  insmod /ko/cfg80211_ath6k.ko
  #insmod /ko/hi_vfmw_avsp.bin
  insmod /ko/tntfs_hisilicon.ko
  insmod /ko/bcmdhd.ko
  insmod /ko/cfg80211.ko
  #insmod /ko/hi_vfmw_h264.bin
  insmod /ko/xhci-hcd.ko
  insmod /ko/btusb.ko
  insmod /ko/ehci-hcd.ko
  insmod /ko/ohci-hcd.ko
  insmod /ko/rtl8188eu.ko

  echo -e "Waiting 5 seconds for removable devices."
  i=0
  while [ $i -lt 5 ]; do
  sleep 1
  echo -n “.”
  i=\$1)
  done
  echo
  exec /sbin/init
```
另外需要修改/etc/inittab文件，加载tty输出：
  console::respawn:/sbin/getty -L ttyAMA0 115200 vt100
注意编译buildroot时需要加入libnss库，否则即便有resolv.conf也无法解析域名等。

目前打算的方法是：
利用init直接挂载/dev/sda1为opt目录
加载/opt/init.hi3798m脚本
该脚本保存了若干配置文件，比如resolv.conf，网络配置等。这样既可无需修改ramdisk也可以保存数据，原理跟tinycore差不多。
另外软件包可以直接套用optware。

发现一个问题，ramdisk压缩后大小不能超过8M，否则无法启动。

另外注意事项，buildroot的system configuration中要用sysvinit，而不是busybox，否则ifupdown无法启动，另外也不会生成/etc/init.d目录和其内脚本。

现在无论如何也无法解决switch_root后ttl无输出的问题了， 目前只能把buildroot编译好的rootfs作为ramdisk，然后交给opt脚本来处理。
得查看下tinycore的原理。

现在debian wheezy跑起来了。因为内核不支持devtmpfs，所以无法使用debian jessie。

打算告一段落了。折腾了一个多星期。没有海思官方的sdk真心寸步难行啊！

修改了内核的部分代码，编译出来了内核。不过跑不起来。不知道缘故。

内核编译ok了。因为之前用了错误的github分支导致。目前一切正常，devtmpfs也正常了。
问题1：登录后tty服务错误，暂时没解决，无输出。
问题2：网络配置，无法ping，提示socks权限错误，因为kernel打开了CONFIG_ANDROID_PARANOID_NETWORK，请关闭该宏。
![image](https://github.com/yoyoliyang/hi3798m_debian/blob/master/20160912-085825.png)
又解决一个大问题：
Timed out waiting for device dev-ttyAMA0.device的提示后，无法出现登录，是内核配置缘故：
![image](https://github.com/yoyoliyang/hi3798m_debian/blob/master/20160913-100530.png)

### hi3798m后续笔记
最近总算在hi3798m的机顶盒上面告一段落，做一下总结：
1、hi3798m主板上必有一个usb引导的短接口，只需要焊住，以后开启一定会从u盘引导。
2、u盘的文件格式必须为fat16格式。只需要三个引导文件fastboot.bin，bootargs.bin，recovery.img文件。因为bootargs中定义了引导文件为recovery.img，我这边没有fastboot的源码，也就无法刷入自己定义文件名称。而其中recovery.img文件是mkbootimg生成的，包含了uImage和initramfs的打包。推荐initramfs打包格式为xz。
```
find . | cpio -H newc -o | xz -9 --format=lzma > ../initramfs.cpio.gz
```
3、关于hi3798m的内核源码，我是从这里找的：https://github.com/Spitzbube/hisilicon-kernel/tree/iconbit_hi3798mx ，配置文件在arch/arm/configs目录下。编译环境为：
![imaeg](https://github.com/yoyoliyang/hi3798m_debian/blob/master/20160914-035144.png)
内核配置有几个注意：

需要打开devtmpfs，否则无法跑debian8

需要关闭CONFIG_ANDROID_PARANOID_NETWORK

需要打开open by fhandle syscalls

需要打开cgroup功能

4、运行debian的话，需要用initramfs来switch root，配置文件等详见github。

我从淘宝收购了一个烽火的hg680-j的机顶盒。

cpu是海思hi3798m的四核a7的cpu，内存为1G。

从github找到了内核源码，并编译好了适合跑debian的uImage。

这里提供的是我编译的所有工具以及配置文件，方便自己也方便他人或许以后有用。

你可以直接用buildroot编译initramfs，或者用busybox做initramfs来switch_root到debian（这是我目前的做法）。

如此，一个高性能的四核arm服务器就做好了，省电且非常便宜。


