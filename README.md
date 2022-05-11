# FuzzUSB #

USB gadget fuzzing framework for the Linux kernel.

Please see for the paper and working of the fuzzing:
([FuzzUSB: Hybrid Stateful Fuzzing of USB Gadget Stacks](https://github.com/purseclab/fuzzusb/blob/main/paper/fuzzusb.pdf)).

### Setup

#### initial setup
* https://github.com/google/syzkaller/blob/master/docs/linux/setup.md
* https://github.com/google/syzkaller/blob/master/docs/linux/external_fuzzing_usb.md

#### syzkaller setup
* syzkaller checkout: `d5696d51924aeb6957c19b616c888f58fe9a3740`
* apply syzkaller patch: `patch/syzkaller/README`
* syzkaller rebuild
```
$ ./scripts/build_syz.sh
```

#### Linux kernel setup
* kernel symlink setup
```
$ cd kernel
$ ln -s [target_kernel_dir] target
```
* apply kernel patch: `patch/kernel/README`
* kernel build with gadget enabled config (e.g., `kernel/config`)
```
$ ./scripts/build_kern.sh
```

#### disk image setup
* disk image symlink setup
```
$ ln -s [target_disk_image] disk/disk.img
```
* additional setup within the image
```
$ apt install python3 usbutils alsa-utils net-tools rsync
```
* copy files to the image: `to_disk/state_mgr.py` 

### Run 
```
$ ./run.sh 
```

NOTE: We will be pushing more soon. 

