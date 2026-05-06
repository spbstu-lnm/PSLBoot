/*
 * PSLBoot. Stage 2.5.
 *
 * Minimal ext2 + Linux bzImage loader for a QEMU IDE disk.
 */

#include <stdint.h>

#define SECTOR_SIZE             512u
#define MAX_BLOCK_SIZE          4096u
#define EXT2_SUPER_LBA          2u
#define EXT2_MAGIC              0xEF53u
#define EXT2_ROOT_INO           2u
#define EXT2_NDIR_BLOCKS        12u
#define EXT2_IND_BLOCK          EXT2_NDIR_BLOCKS
#define EXT2_DIND_BLOCK         (EXT2_IND_BLOCK + 1u)
#define EXT2_N_BLOCKS           15u
#define EXT2_S_IFMT             0xF000u
#define EXT2_S_IFLNK            0xA000u
#define EXT2_S_IFREG            0x8000u
#define EXT2_S_IFDIR            0x4000u
#define EXT2_FEATURE_INCOMPAT_EXTENTS 0x0040u

#define MBR_PART_TABLE          446u
#define MBR_PART_SIZE           16u
#define MBR_PART_TYPE_LINUX     0x83u

#define KERNEL_LOAD_ADDR        0x100000u
#define INITRD_LOAD_ADDR        0x4000000u
#define BOOT_PARAMS_ADDR        0x90000u
#define CMDLINE_ADDR            0x91000u

#define ZP_SETUP_SECTS          0x1F1u
#define ZP_HEADER               0x202u
#define ZP_VERSION              0x206u
#define ZP_TYPE_OF_LOADER       0x210u
#define ZP_LOADFLAGS            0x211u
#define ZP_CODE32_START         0x214u
#define ZP_RAMDISK_IMAGE        0x218u
#define ZP_RAMDISK_SIZE         0x21Cu
#define ZP_HEAP_END_PTR         0x224u
#define ZP_CMD_LINE_PTR         0x228u
#define ZP_INITRD_ADDR_MAX      0x22Cu

#define LOADED_HIGH             0x01u
#define CAN_USE_HEAP            0x80u

typedef struct {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t s_uuid[16];
    char s_volume_name[16];
    char s_last_mounted[64];
    uint32_t s_algorithm_usage_bitmap;
    uint8_t s_prealloc_blocks;
    uint8_t s_prealloc_dir_blocks;
    uint16_t s_padding1;
    uint8_t s_journal_uuid[16];
    uint32_t s_journal_inum;
    uint32_t s_journal_dev;
    uint32_t s_last_orphan;
    uint32_t s_hash_seed[4];
    uint8_t s_def_hash_version;
    uint8_t s_reserved_char_pad;
    uint16_t s_reserved_word_pad;
    uint32_t s_default_mount_opts;
    uint32_t s_first_meta_bg;
    uint32_t s_reserved[190];
} __attribute__((packed)) ext2_super_block_t;

typedef struct {
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;
    uint32_t bg_reserved[3];
} __attribute__((packed)) ext2_bgd_t;

typedef struct {
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks;
    uint32_t i_flags;
    uint32_t l_i_reserved1;
    uint32_t i_block[EXT2_N_BLOCKS];
    uint32_t i_generation;
    uint32_t i_file_acl;
    uint32_t i_dir_acl;
    uint32_t i_faddr;
    uint8_t l_i_frag;
    uint8_t l_i_fsize;
    uint16_t i_pad1;
    uint16_t l_i_uid_high;
    uint16_t l_i_gid_high;
    uint32_t l_i_reserved2;
} __attribute__((packed)) ext2_inode_t;

typedef struct {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t name_len;
    uint8_t file_type;
    char name[];
} __attribute__((packed)) ext2_dirent_t;

extern void jump_linux(uint32_t entry, uint32_t boot_params);

static ext2_super_block_t super;
static uint32_t partition_lba;
static uint32_t block_size;
static uint32_t inode_size;
static uint8_t block_buf[MAX_BLOCK_SIZE];
static uint8_t block_buf2[MAX_BLOCK_SIZE];
static uint8_t sector_buf[SECTOR_SIZE];

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void insw(uint16_t port, void *addr, uint32_t count) {
    __asm__ volatile ("cld; rep insw"
                      : "+D"(addr), "+c"(count)
                      : "d"(port)
                      : "memory");
}

static void *memcpy(void *dst, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t*)dst;
    const uint8_t *s = (const uint8_t*)src;
    while (n--) *d++ = *s++;
    return dst;
}

static void *memset(void *dst, int value, uint32_t n) {
    uint8_t *d = (uint8_t*)dst;
    while (n--) *d++ = (uint8_t)value;
    return dst;
}

static uint32_t strlen(const char *s) {
    uint32_t n = 0;
    while (s[n]) n++;
    return n;
}

static int streqn(const char *a, const char *b, uint32_t n) {
    uint32_t i;
    for (i = 0; i < n; i++) {
        if (a[i] != b[i] || b[i] == 0) return 0;
    }
    return b[n] == 0;
}

static int startswith(const char *name, uint32_t name_len, const char *prefix) {
    uint32_t i = 0;
    while (prefix[i]) {
        if (i >= name_len || name[i] != prefix[i]) return 0;
        i++;
    }
    return 1;
}

static void puts(const char *s) {
    volatile uint16_t *vga = (volatile uint16_t*)0xB8000;
    static uint32_t pos = 0;
    while (*s) {
        while ((inb(0x3F8 + 5) & 0x20u) == 0) {}
        outb(0x3F8, (uint8_t)*s);
        if (*s == '\n') {
            pos = (pos / 80u + 1u) * 80u;
        } else {
            vga[pos++] = (uint16_t)(0x0700u | (uint8_t)*s);
        }
        if (pos >= 80u * 25u) pos = 0;
        s++;
    }
}

static void die(const char *msg) {
    puts("PSLBoot: ");
    puts(msg);
    puts("\n");
    for (;;) __asm__ volatile ("hlt");
}

static void ata_wait_ready(void) {
    uint8_t status;
    do {
        status = inb(0x1F7);
    } while (status & 0x80u);
}

static void ata_wait_drq(void) {
    uint8_t status;
    do {
        status = inb(0x1F7);
        if (status & 0x01u) die("ATA read error");
    } while ((status & 0x08u) == 0);
}

static void disk_read(uint32_t lba, uint32_t count, void *buf) {
    uint8_t *dst = (uint8_t*)buf;

    while (count) {
        uint8_t chunk = count > 255u ? 255u : (uint8_t)count;
        uint32_t i;

        ata_wait_ready();
        outb(0x1F6, (uint8_t)(0xE0u | ((lba >> 24) & 0x0Fu)));
        outb(0x1F2, chunk);
        outb(0x1F3, (uint8_t)lba);
        outb(0x1F4, (uint8_t)(lba >> 8));
        outb(0x1F5, (uint8_t)(lba >> 16));
        outb(0x1F7, 0x20);

        for (i = 0; i < chunk; i++) {
            ata_wait_drq();
            insw(0x1F0, dst, SECTOR_SIZE / 2u);
            dst += SECTOR_SIZE;
        }

        lba += chunk;
        count -= chunk;
    }
}

static uint16_t rd16(const void *p) {
    const uint8_t *b = (const uint8_t*)p;
    return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

static uint32_t rd32(const void *p) {
    const uint8_t *b = (const uint8_t*)p;
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static void wr16(void *p, uint16_t v) {
    uint8_t *b = (uint8_t*)p;
    b[0] = (uint8_t)v;
    b[1] = (uint8_t)(v >> 8);
}

static void wr32(void *p, uint32_t v) {
    uint8_t *b = (uint8_t*)p;
    b[0] = (uint8_t)v;
    b[1] = (uint8_t)(v >> 8);
    b[2] = (uint8_t)(v >> 16);
    b[3] = (uint8_t)(v >> 24);
}

static void detect_partition(void) {
    uint32_t i;
    disk_read(0, 1, sector_buf);
    partition_lba = 0;

    for (i = 0; i < 4; i++) {
        uint8_t *p = sector_buf + MBR_PART_TABLE + i * MBR_PART_SIZE;
        uint8_t type = p[4];
        if (type == MBR_PART_TYPE_LINUX || type == 0x00u) {
            uint32_t start = rd32(p + 8);
            if (start != 0) {
                partition_lba = start;
                return;
            }
        }
    }
}

static void read_block(uint32_t block_num, void *buf) {
    uint32_t lba = partition_lba + (block_num * block_size) / SECTOR_SIZE;
    disk_read(lba, block_size / SECTOR_SIZE, buf);
}

static void read_inode(uint32_t inode_num, ext2_inode_t *inode) {
    uint32_t group = (inode_num - 1u) / super.s_inodes_per_group;
    uint32_t index = (inode_num - 1u) % super.s_inodes_per_group;
    uint32_t desc_block = super.s_first_data_block + 1u;
    uint32_t desc_offset = group * sizeof(ext2_bgd_t);
    ext2_bgd_t *bgd;
    uint32_t inode_offset;

    read_block(desc_block + desc_offset / block_size, block_buf);
    bgd = (ext2_bgd_t*)(block_buf + (desc_offset % block_size));

    inode_offset = index * inode_size;
    read_block(bgd->bg_inode_table + inode_offset / block_size, block_buf);
    memcpy(inode, block_buf + (inode_offset % block_size), sizeof(ext2_inode_t));
}

static uint32_t inode_block_at(const ext2_inode_t *inode, uint32_t file_block) {
    uint32_t ptrs = block_size / 4u;
    uint32_t *table;

    if (file_block < EXT2_NDIR_BLOCKS) return inode->i_block[file_block];
    file_block -= EXT2_NDIR_BLOCKS;

    if (file_block < ptrs) {
        if (!inode->i_block[EXT2_IND_BLOCK]) return 0;
        read_block(inode->i_block[EXT2_IND_BLOCK], block_buf2);
        table = (uint32_t*)block_buf2;
        return table[file_block];
    }
    file_block -= ptrs;

    if (file_block < ptrs * ptrs) {
        uint32_t first = file_block / ptrs;
        uint32_t second = file_block % ptrs;
        if (!inode->i_block[EXT2_DIND_BLOCK]) return 0;
        read_block(inode->i_block[EXT2_DIND_BLOCK], block_buf2);
        table = (uint32_t*)block_buf2;
        if (!table[first]) return 0;
        read_block(table[first], block_buf2);
        table = (uint32_t*)block_buf2;
        return table[second];
    }

    die("file uses triple-indirect blocks");
    return 0;
}

static void read_file_range(const ext2_inode_t *inode, uint32_t offset,
                            uint32_t size, void *dst) {
    uint8_t *out = (uint8_t*)dst;
    uint32_t left = size;

    while (left) {
        uint32_t file_block = offset / block_size;
        uint32_t block_off = offset % block_size;
        uint32_t chunk = block_size - block_off;
        uint32_t phys = inode_block_at(inode, file_block);

        if (chunk > left) chunk = left;
        if (phys == 0) {
            memset(out, 0, chunk);
        } else if (block_off == 0 && chunk == block_size) {
            read_block(phys, out);
        } else {
            read_block(phys, block_buf);
            memcpy(out, block_buf + block_off, chunk);
        }

        offset += chunk;
        out += chunk;
        left -= chunk;
    }
}

static uint32_t find_in_dir(const ext2_inode_t *dir, const char *name) {
    uint32_t off = 0;
    uint32_t name_len = strlen(name);

    while (off < dir->i_size) {
        uint32_t chunk = dir->i_size - off;
        uint32_t pos = 0;
        if (chunk > block_size) chunk = block_size;
        read_file_range(dir, off, chunk, block_buf);

        while (pos + 8u <= chunk) {
            ext2_dirent_t *de = (ext2_dirent_t*)(block_buf + pos);
            if (de->rec_len == 0) die("bad ext2 dirent");
            if (de->inode && de->name_len == name_len &&
                streqn(de->name, name, name_len)) {
                return de->inode;
            }
            pos += de->rec_len;
        }
        off += chunk;
    }

    return 0;
}

static uint32_t find_prefixed_in_dir(const ext2_inode_t *dir, const char *prefix) {
    uint32_t off = 0;

    while (off < dir->i_size) {
        uint32_t chunk = dir->i_size - off;
        uint32_t pos = 0;
        if (chunk > block_size) chunk = block_size;
        read_file_range(dir, off, chunk, block_buf);

        while (pos + 8u <= chunk) {
            ext2_dirent_t *de = (ext2_dirent_t*)(block_buf + pos);
            if (de->rec_len == 0) die("bad ext2 dirent");
            if (de->inode && startswith(de->name, de->name_len, prefix)) {
                return de->inode;
            }
            pos += de->rec_len;
        }
        off += chunk;
    }

    return 0;
}

static uint32_t path_lookup(const char *path) {
    ext2_inode_t dir;
    uint32_t ino = EXT2_ROOT_INO;
    const char *p = path;

    if (*p == '/') p++;
    read_inode(ino, &dir);

    while (*p) {
        char component[64];
        uint32_t n = 0;
        while (p[n] && p[n] != '/') {
            if (n + 1u >= sizeof(component)) die("path component too long");
            component[n] = p[n];
            n++;
        }
        component[n] = 0;

        ino = find_in_dir(&dir, component);
        if (!ino) return 0;
        read_inode(ino, &dir);

        p += n;
        if (*p == '/') p++;
    }

    return ino;
}

static uint32_t find_boot_file(const char *exact, const char *prefix) {
    uint32_t ino = path_lookup(exact);
    uint32_t boot_ino;
    ext2_inode_t boot_dir;
    ext2_inode_t exact_inode;

    if (ino) {
        read_inode(ino, &exact_inode);
        if ((exact_inode.i_mode & EXT2_S_IFMT) == EXT2_S_IFREG) return ino;
    }
    boot_ino = path_lookup("/boot");
    if (!boot_ino) return 0;
    read_inode(boot_ino, &boot_dir);
    if ((boot_dir.i_mode & EXT2_S_IFMT) != EXT2_S_IFDIR) return 0;
    return find_prefixed_in_dir(&boot_dir, prefix);
}

static void copy_cmdline(void) {
    static const char cmdline[] =
        "root=/dev/sda1 ro console=tty0 console=ttyS0,115200n8";
    memcpy((void*)CMDLINE_ADDR, cmdline, sizeof(cmdline));
}

void stage2_main(void) {
    ext2_inode_t kernel;
    ext2_inode_t initrd;
    uint32_t kernel_ino;
    uint32_t initrd_ino;
    uint8_t *bp = (uint8_t*)BOOT_PARAMS_ADDR;
    uint8_t setup_sects;
    uint32_t kernel_offset;
    uint32_t entry;

    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x01);
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);
    outb(0x3F8 + 2, 0xC7);
    outb(0x3F8 + 4, 0x0B);

    puts("PSLBoot\n");

    detect_partition();
    disk_read(partition_lba + EXT2_SUPER_LBA, 2, block_buf);
    memcpy(&super, block_buf, sizeof(super));

    if (super.s_magic != EXT2_MAGIC) die("ext2 superblock not found");
    if (super.s_feature_incompat & EXT2_FEATURE_INCOMPAT_EXTENTS) {
        die("extents are not supported; create a real ext2 filesystem");
    }

    block_size = 1024u << super.s_log_block_size;
    if (block_size > MAX_BLOCK_SIZE) die("ext2 block size > 4096");

    inode_size = super.s_inode_size ? super.s_inode_size : 128u;
    if (inode_size < sizeof(ext2_inode_t)) die("unsupported inode size");

    kernel_ino = find_boot_file("/boot/vmlinuz", "vmlinuz");
    if (!kernel_ino) die("kernel not found");
    read_inode(kernel_ino, &kernel);
    if ((kernel.i_mode & EXT2_S_IFMT) != EXT2_S_IFREG) die("kernel is not regular");

    memset(bp, 0, 4096);
    read_file_range(&kernel, 0, 4096, bp);

    if (rd16(bp + 0x1FE) != 0xAA55u || rd32(bp + ZP_HEADER) != 0x53726448u) {
        die("bad Linux kernel header");
    }
    if (rd16(bp + ZP_VERSION) < 0x0200u) die("Linux boot protocol too old");

    setup_sects = bp[ZP_SETUP_SECTS] ? bp[ZP_SETUP_SECTS] : 4u;
    kernel_offset = ((uint32_t)setup_sects + 1u) * SECTOR_SIZE;
    if (kernel_offset >= kernel.i_size) die("bad kernel size");

    puts("Loading kernel\n");
    read_file_range(&kernel, kernel_offset, kernel.i_size - kernel_offset,
                    (void*)KERNEL_LOAD_ADDR);

    initrd_ino = find_boot_file("/boot/initrd.img", "initrd.img");
    if (initrd_ino) {
        read_inode(initrd_ino, &initrd);
        if ((initrd.i_mode & EXT2_S_IFMT) == EXT2_S_IFREG) {
            puts("Loading initrd\n");
            read_file_range(&initrd, 0, initrd.i_size, (void*)INITRD_LOAD_ADDR);
            wr32(bp + ZP_RAMDISK_IMAGE, INITRD_LOAD_ADDR);
            wr32(bp + ZP_RAMDISK_SIZE, initrd.i_size);
        }
    }

    copy_cmdline();
    bp[ZP_TYPE_OF_LOADER] = 0xFFu;
    bp[ZP_LOADFLAGS] |= LOADED_HIGH | CAN_USE_HEAP;
    wr16(bp + ZP_HEAP_END_PTR, 0xFE00u);
    wr32(bp + ZP_CMD_LINE_PTR, CMDLINE_ADDR);
    wr32(bp + ZP_INITRD_ADDR_MAX, 0x7FFFFFFFu);

    entry = rd32(bp + ZP_CODE32_START);
    if (!entry) entry = KERNEL_LOAD_ADDR;

    puts("Booting Linux\n");
    jump_linux(entry, BOOT_PARAMS_ADDR);
}
