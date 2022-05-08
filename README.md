# FuzzUSB #

USB gadget fuzzing framework for the Linux kernel.

Please see for the paper and working of the fuzzing:
([FuzzUSB: Hybrid Stateful Fuzzing of USB Gadget Stacks](https://github.com/purseclab/fuzzusb/blob/main/paper/fuzzusb.pdf)).

### Setup

#### initial setup
* https://github.com/google/syzkaller/blob/master/docs/linux/setup.md
* https://github.com/google/syzkaller/blob/master/docs/linux/external_fuzzing_usb.md

#### syzkaller setup
* apply syzkaller patch: `patch/syzkaller/README`
* syzkaller rebuild

#### Linux kernel setup
* apply kernel patch: `patch/kernel/README`
* kernel rebuild 

#### disk image setup
* initial setup within the image
```
$ apt install python3 usbutils alsa-utils net-tools rsync
```
* copy to the image: `state_mgr.py` 

### Run 

NOTE: We will be pushing more soon. 

