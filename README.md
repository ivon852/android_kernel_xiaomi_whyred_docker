# Docker Kernel for Xiaomi Whyred
A Docker compatible Android kernel for Xiaomi Redmi Note 5 Pro (whyred).

可透過Termux原生執行docker的紅米Note 5內核。

This kernel is based on [RAD kernel for whyred](https://github.com/radcolor/android_kernel_xiaomi_whyred), adds support for running docker on Android.
![](https://i.postimg.cc/MHbSYmhq/Screenshot-20211228-232245-Termux.png)

## How to use
You need root access and a custom ROM before flashing this kernel.

This kernel is only tested on LineageOS 18. 

1. Download `boot.img` from Releases.
2. Flash it via TWRP or fastboot.
3. After booting, run this command in Termux:
```bash
sudo mount -t tmpfs -o uid=0,gid=0,mode=0755 cgroup /sys/fs/cgroup
```
4. Run docker daemon
```
sudo dockerd --iptables=false
```

## Compilation Guide
1. Clone this repo, expor CROSS_COMPILE & CROSS_COMPILE_ARM32 path (in this case I use Eva GCC) and ARCH type. 
2. Kernel configs are alreay in `.config`. Launch menuconfig by `make menuconfig`.
3. After tweaking, start compiling by `make`.
4. Use Android Image Kitchen to repack generated dts file to `boot.img`.

## References
If you want to compile kernel for other devices, you may refer to [FreddieOliveira's Tutorial](https://gist.github.com/FreddieOliveira/efe850df7ff3951cb62d74bd770dce27).
