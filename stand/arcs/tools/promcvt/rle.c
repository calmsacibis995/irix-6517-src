#include <sys/types.h> 
#include "rle.h"
#include "stdio.h"

int uncompress_block(unsigned char *comp_buf, unsigned char *uncomp_buf,
			    int flags, long blk_size, long *new_size)
{
    int i;
    unsigned char value;
    unsigned char count;
    unsigned char cur_byte;
    long read_index, write_index;
    register uint real_length;
    unsigned cksum = 0;
    prom_rle_hdr_t *rle = (prom_rle_hdr_t *)comp_buf;

    if (rle->rle_magic != RLE_MAGIC) {
	printf("Bad magic number on RLE segment.\n");
	return -1;
    }

    real_length = rle->rle_real_length;

    comp_buf += sizeof(prom_rle_hdr_t);

    read_index = write_index = 0;

    while(read_index < blk_size) {
	cur_byte = comp_buf[read_index++];
	if (cur_byte == 0) {
	    count = comp_buf[read_index++];
	    if (count == 0) {
		uncomp_buf[write_index++] = 0;
	    } else {
		value = comp_buf[read_index++];
		    for (i = 0; i <= count; i++) {
			uncomp_buf[write_index++] = value;
			cksum += value;
		    }
	    } /* If count == 0 */
	} else { /* if cur_byte == 0 */
		uncomp_buf[write_index++] = cur_byte;
		cksum += cur_byte;
	}

	if (write_index > real_length) {
		printf("Attempted to decompress beyond original size!\n");
		return -1;
	}

    } /* while read_index */

    *new_size = write_index + sizeof (prom_rle_hdr_t);

    if (write_index != real_length) {
	printf("Decompressed length != original length!\n");
	return -1;
    }

    if (cksum != rle->rle_real_chksum) {
	printf("Decompressed checksum is wrong!\n");
	printf("    stored == 0x%x, computed == 0x%x\n", rle->rle_real_chksum,
		cksum);
	return -1;
    }
    
    return 0;
}


int compress_block(char *old_addr, char *new_addr, long old_length)
{
    int read_index;
    int count = 0;
    unsigned char value = 0;
    unsigned char cur_byte = 0;
    int write_index;
    uint cksum = 0;
    prom_rle_hdr_t *rle = (prom_rle_hdr_t *)new_addr;

    new_addr += sizeof(prom_rle_hdr_t);

    write_index = read_index = 0;

    while ( read_index < old_length) {
	if (read_index == 0) {
	    cur_byte = value = old_addr[read_index];
	    cksum = cur_byte;		/* Initialize the checksum */
	    count = 0;
	} else {
	    if (count == 255) {
		if (write_index + 3 > old_length)
		    return old_length;
		new_addr[write_index++] = 0;
		new_addr[write_index++] = count;
		new_addr[write_index++] = value;
		value = cur_byte = old_addr[read_index];
		cksum += cur_byte;
		count = 0;
	    } else { 
		if ((cur_byte = old_addr[read_index]) == value) {
		    cksum += cur_byte;	/* Update the checksum */
		    count++;
		} else {
		    cksum += cur_byte;	/* Update the checksum */
		    if (count > 1) {
			if (write_index + 3 > old_length)
			    return old_length;
			new_addr[write_index++] = 0;
			new_addr[write_index++] = count;
			new_addr[write_index++] = value;
		    } else if (count == 1) {
			if (value == 0) {
			    if (write_index + 4 > old_length)
				return old_length;
			    new_addr[write_index++] = value;
			    new_addr[write_index++] = value;
			    new_addr[write_index++] = value;
			    new_addr[write_index++] = value;
			} else {
			    if (write_index + 2 > old_length)
				return old_length;
			    new_addr[write_index++] = value;
			    new_addr[write_index++] = value;
			}
		    } else { /* count == 0 */
			if (value == 0) {
			    if (write_index + 2 > old_length)
				return old_length;
			    new_addr[write_index++] = value;
			    new_addr[write_index++] = value;
			} else {
			    if (write_index + 1 > old_length)
				return old_length;
			    new_addr[write_index++] = value;
			}
		    } /* if count > 1 */
		    value = cur_byte;
		    count = 0;
		} /* if byte == value */
	    } /* if count == 255 */
	} /* if read_index == 0 */
	read_index++;
    }

    /* Drain the pipe */
    if (count > 1) {
	if (write_index + 3 > old_length)
	    return old_length;
	new_addr[write_index++] = 0;
	new_addr[write_index++] = count;
	new_addr[write_index++] = value;
    } else if (count == 1) {
	if (value == 0) {
	    if (write_index + 4 > old_length)
		return old_length;
	    new_addr[write_index++] = value;
	    new_addr[write_index++] = value;
	    new_addr[write_index++] = value;
	    new_addr[write_index++] = value;
	} else {
	    if (write_index + 2 > old_length)
		return old_length;
	    new_addr[write_index++] = value;
	    new_addr[write_index++] = value;
	}
    } else { /* count == 0 */
	if (value == 0) {
	    if (write_index + 2 > old_length)
		return old_length;
	    new_addr[write_index++] = value;
	    new_addr[write_index++] = value;
	} else {
	    if (write_index + 1 > old_length)
		return old_length;
	    new_addr[write_index++] = value;
	}
    } /* if count > 1 */
    value = cur_byte;
    count = 0;

    rle->rle_real_length = write_index;
    rle->rle_magic = RLE_MAGIC;
    rle->rle_real_chksum = cksum;

    return write_index + sizeof(prom_rle_hdr_t);
}

