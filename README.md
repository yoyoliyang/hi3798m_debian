我从淘宝收购了一个烽火的hg680-j的机顶盒。

cpu是海思hi3798m的四核a7的cpu，内存为1G。

从github找到了内核源码，并编译好了适合跑debian的uImage。

这里提供的是我编译的所有工具以及配置文件，方便自己也方便他人或许以后有用。

你可以直接用buildroot编译initramfs，或者用busybox做initramfs来switch_root到debian（这是我目前的做法）。

如此，一个高性能的四核arm服务器就做好了，省电且非常便宜。
