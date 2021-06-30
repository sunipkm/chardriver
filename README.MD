# Character Driver example with Circular Buffer
1. To install kernel sources in Ubuntu:
    a. Make sure to enable source URLs in `/etc/apt/sources.list`
    b. `apt-get update`
    c. `apt-get source linux-image-$(uname -r)`
    d. `apt-get build-dep linux-image-$(uname -r)`

2. To build kernel module and test program execute `make`
3. To load kernel module, `insmod char_buf.ko`
4. To unload kernel module, `rmmod char_buf`
5. To create `/dev/` node: Check `dmesg` after loading the kernel module and follow the commands specified.
6. To monitor kernel module messages, run `dmesg -w`.
7. To test, run `readbuf.out` to read, and do `echo -n Hello world > /dev/charDev`.