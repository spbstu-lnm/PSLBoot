/*
 *
 *      PSLBoot. Stage 2.5.
 *
 *      WIP
 *
 */


#include <stdint.h> // freestanding


// constants used in structs
#define	EXT2_NDIR_BLOCKS		12
#define	EXT2_IND_BLOCK			EXT2_NDIR_BLOCKS
#define	EXT2_DIND_BLOCK			(EXT2_IND_BLOCK + 1)
#define	EXT2_TIND_BLOCK			(EXT2_DIND_BLOCK + 1)
#define	EXT2_N_BLOCKS			(EXT2_TIND_BLOCK + 1)

#define KERNEL_LOAD_ADDR 0x100000   // 1 MB
#define INITRD_LOAD_ADDR  0x1000000 // 16 MB
#define SECTOR_SIZE       512
#define EXT2_SUPER_OFFSET 1024


// refer to https://wiki.osdev.org/Ext2
// https://github.com/torvalds/linux/blob/master/fs/ext2/ext2.h#L410
typedef struct {
    // BASE FIELDS

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
    
    // EXTENDED FIELDS

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

    uint8_t s_reserved_char_pad;    // 3 bytes total
    uint16_t s_reserved_word_pad;

    uint32_t s_default_mount_opts;
    uint32_t s_first_meta_bg;

    uint32_t s_reserved[190];   // 760 bytes total
} __attribute__((packed)) ext2_super_block_t; // ext2_super_block in kernel

// https://github.com/torvalds/linux/blob/master/fs/ext2/ext2.h#L191
typedef struct {
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;

    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;
    
    uint32_t bg_reserved[3];    // 12 bytes total
} __attribute__((packed)) ext2_bgd_t;   // ext2_group_desc in kernel

// https://github.com/torvalds/linux/blob/master/fs/ext2/ext2.h#L290
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

    uint32_t l_i_reserved1; // OS dependent; Linux assumed

    uint32_t i_block[EXT2_N_BLOCKS];
    
    uint32_t i_generation;
    uint32_t i_file_acl;
    uint32_t i_dir_acl;
    uint32_t i_faddr;

    // OS DEPENDENT 2 (LINUX)

    uint8_t l_i_frag;
    uint8_t l_i_fsize;
    
    uint16_t i_pad1;
    uint16_t l_i_uid_high;
    uint16_t l_i_gid_high;
    
    uint32_t l_i_reserved2;
} __attribute__((packed)) ext2_inode_t; // ext2_inode in kernel


// should be implemented in asm
// TODO: check asm implementations
extern void disk_read(uint32_t lba, uint32_t count, void* buf);
extern void* memcpy(void* dst, const void* src, uint32_t n);


static ext2_super_block_t super;
static uint32_t block_size;


void read_block(uint32_t block_num, void* buf) {
    uint32_t lba = (block_num * block_size) / SECTOR_SIZE;
    uint32_t sectors = block_size / SECTOR_SIZE;

    disk_read(lba, sectors, buf);
}


ext2_inode_t get_inode(uint32_t inode_num) {
    uint32_t inodes_per_group = super.s_inodes_count;
    uint32_t group = (inode_num - 1) / inodes_per_group;

    uint8_t bgd_buf[block_size];
    read_block(super.s_first_data_block + 1, bgd_buf);
    ext2_bgd_t* bgd = (ext2_bgd_t*)bgd_buf + group;

    // reading inode table
    uint8_t inode_buf[block_size];
    uint32_t inode_offset = ((inode_num - 1) % inodes_per_group) * 128;
    read_block(bgd->bg_inode_table + inode_offset / block_size, inode_buf);

    ext2_inode_t inode;
    memcpy(&inode, inode_buf + (inode_offset % block_size), sizeof(inode));
    
    return inode;
}


// boot parameters are passed via Linux Boot Protocol
// https://github.com/torvalds/linux/blob/master/arch/x86/include/uapi/asm/bootparam.h
typedef struct {
	uint8_t	setup_sects;

	uint16_t	root_flags;
	
    uint32_t	syssize;
	
    uint16_t	ram_size;
	uint16_t	vid_mode;
	uint16_t	root_dev;
	uint16_t	boot_flag;
	uint16_t	jump;
	
    uint32_t	header;
	
    uint16_t	version;
	
    uint32_t	realmode_swtch;
	
    uint16_t	start_sys_seg;
	uint16_t	kernel_version;
	
    uint8_t	type_of_loader;
	uint8_t	loadflags;
	
    uint16_t	setup_move_size;
	
    uint32_t	code32_start;
	uint32_t	ramdisk_image;
	uint32_t	ramdisk_size;
	uint32_t	bootsect_kludge;
	
    uint16_t	heap_end_ptr;
	
    uint8_t	ext_loader_ver;
	uint8_t	ext_loader_type;
	
    uint32_t	cmd_line_ptr;
	uint32_t	initrd_addr_max;
	uint32_t	kernel_alignment;
	
    uint8_t	relocatable_kernel;
	uint8_t	min_alignment;
	
    uint16_t	xloadflags;
	
    uint32_t	cmdline_size;
	uint32_t	hardware_subarch;
	
    uint64_t	hardware_subarch_data;
	
    uint32_t	payload_offset;
	uint32_t	payload_length;
	
    uint64_t	setup_data;
	uint64_t	pref_address;
	
    uint32_t	init_size;
	uint32_t	handover_offset;
	uint32_t	kernel_info_offset;
} __attribute__((packed)) boot_params_t;


void stage2_main(void) {
    // ==== reading ext2 superblock
    uint8_t buf[1024];
    disk_read(2, 2, buf);
    memcpy(&super, buf, sizeof(super));

    // infinite loop if magic is invalid
    if (super.s_magic != 0xEF53) while (1);

    block_size = 1024 << super.s_log_block_size;

    // TODO: look for vmlinuz
    // ... traverse ext2 dirs ...

    // TODO: Load kernel from KERNEL_LOAD_ADDR
    // ... read inode blocks ...

    // ==== filling boot_params and jumping to kernel
    boot_params_t *bp = (boot_params_t*)0x90000;
    bp->cmd_line_ptr = 0x91000;
    // write "root=/dev/sda1 ro quiet" to 0x91000

    // ==== jump to kernel entry point (usually KERNEL_LOAD_ADDR + 0x200)
    void (*kernel_entry)(void) = (void*)(KERNEL_LOAD_ADDR + 0x200);
    kernel_entry();
}
