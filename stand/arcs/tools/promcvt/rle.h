

typedef struct prom_rle_hdr {
	__uint32_t	rle_magic;
	__uint32_t	rle_real_length;
	__uint32_t	rle_real_chksum;
	__uint32_t	rle_pad;
} prom_rle_hdr_t;


#define RLE_MAGIC	0x5f524c45		/* '_RLE' */

int compress_block(char *old_addr, char *new_addr, long old_length);
int uncompress_block(unsigned char *comp_buf, unsigned char *uncomp_buf,
                            int flags, long blk_size, long *new_size);

