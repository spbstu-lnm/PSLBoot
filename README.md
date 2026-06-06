# PSLBoot
Polytech Simple Linux Bootloader **(WIP)**
___
# What works

PSLBoot can load a Linux bzImage and initrd from the first Linux/ext2
partition of a raw QEMU IDE disk. Stage 2 is stored in the post-MBR gap
starting at LBA 1, so create the first partition at the usual 1 MiB offset
or later.

# Build

```sh
make
```

This creates `build/bootloader.img`, mostly useful as a build artifact sanity
check. For a real Debian disk image, install the loader into an existing raw
image:

```sh
make install DISK=debian-ext2.img
```

The install target preserves the partition table, writes MBR boot code, writes
the boot signature, and writes Stage 2 at LBA 1.

# Debian/ext2 disk expectations

- QEMU disk must be attached as the primary IDE disk.
- The first Linux partition must contain an ext2 filesystem, not ext4-with-extents.
- Kernel/initrd are searched as `/boot/vmlinuz`, `/boot/initrd.img`, then by
  `vmlinuz*` and `initrd.img*` inside `/boot`.
- Kernel command line is currently built in as:
  `root=/dev/sda1 ro console=tty0 console=ttyS0,115200n8`.

Example run after installing into a prepared disk:

```sh
make run
```

or directly:

```sh
"C:/Program Files/qemu/qemu-system-i386.exe" -m 256M -drive format=raw,file=debian-ext2.img -serial stdio
```
