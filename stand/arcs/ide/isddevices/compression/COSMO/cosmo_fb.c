#include "cosmo_hw.h"
#include "cosmo_ide.h"
#include <ide.h>

#define MAX_MEM_ERROR 20
unsigned mem_error_cnt = 0;

static int fb_mem_16_pio_test1(unsigned mem_data);
static int fb_mem_16_pio_walking(unsigned walking1);
static int fb_mem_32_pio_test1(unsigned mem_data);
static int fb_mem_32_dma_test1(unsigned mem_data);
static int fb_mem_32_pio_walking(unsigned walking1);
static void fb_mem_32_dma_walking(unsigned walking1);

int
cosmo_fb_mem_pio(void)
{
	char *self = "cosmo_fb_mem_pio";

	msg_printf(DBG, "%s: Reportlevel is 0x%x\n", self, Reportlevel);
	cosmo_err_count = 0;
	fb_mem_16_pio_test1( 0 );	/* zero as data */
	fb_mem_16_pio_test1( 0xffff );	/* 0xffff as data */
	fb_mem_16_pio_walking( 1 );	/* walking 1's */
	fb_mem_16_pio_walking( 0 );	/* walking 0's */
	fb_mem_16_pio_test1( 0xa5a5 );	/* 0xa5a5 as data */
	fb_mem_16_pio_test1( 0x5a5a );	/* 0x5a5a as data */
	fb_mem_32_pio_test1( 0 );		/* zero as data */
	fb_mem_32_pio_test1( 0xffffffff );	/* 0xffff as data */
	fb_mem_32_pio_walking( 1 );		/* walking 1's */
	fb_mem_32_pio_walking( 0 );		/* walking 0's */
	fb_mem_32_pio_test1( 0xa5a5a5a5 );	/* 0xa5a5 as data */
	fb_mem_32_pio_test1( 0x5a5a5a5a );	/* 0x5a5a as data */
	return(cosmo_err_count);
}

int
cosmo_fb_mem_dma(void)
{
	char *self = "cosmo_fb_mem_dma";

	msg_printf(DBG, "%s: Reportlevel is 0x%x\n", self, Reportlevel);
	cosmo_err_count = 0;
	cosmo_board_reset();
	cosmo_dma_reset(ALL_DMA_RESETS);
	fb_mem_32_dma_test1( 0xa5a5a5a5 );	/* 0xa5a5 as data */
	cosmo_board_reset();
	cosmo_dma_reset(ALL_DMA_RESETS);
	fb_mem_32_dma_test1( 0 );		/* zero as data */
	cosmo_board_reset();
	cosmo_dma_reset(ALL_DMA_RESETS);
	fb_mem_32_dma_test1( 0xffffffff );	/* 0xffff as data */
	cosmo_board_reset();
	cosmo_dma_reset(ALL_DMA_RESETS);
	fb_mem_32_dma_walking( 1 );		/* walking 1's */
	cosmo_board_reset();
	cosmo_dma_reset(ALL_DMA_RESETS);
	fb_mem_32_dma_walking( 0 );		/* walking 0's */
	cosmo_board_reset();
	cosmo_dma_reset(ALL_DMA_RESETS);
	fb_mem_32_dma_test1( 0xa5a5a5a5 );	/* 0xa5a5 as data */
	cosmo_board_reset();
	cosmo_dma_reset(ALL_DMA_RESETS);
	fb_mem_32_dma_test1( 0x5a5a5a5a );	/* 0x5a5a as data */
	return(cosmo_err_count);
}

int
cosmo_fb_pio(int argc, char **argv)
{
	char *self = "cosmo_fb_pio";

	if ( cosmo_probe_for_it() == -1 ) {
		return(-1);
	}
	cosmo_board_reset();
	cosmo_dma_reset(ALL_DMA_RESETS);
	if ( cosmo_fb_mem_pio() ) {
		msg_printf(SUM, "%s: test failed\n", self);
		return(cosmo_err_count);
	} else {
		msg_printf(SUM, "%s: test passed\n", self);
		return(0);
	}
}

int
cosmo_fb_dma(int argc, char **argv)
{
	char *self = "cosmo_fb_dma";

	if ( cosmo_probe_for_it() == -1 ) {
		return(-1);
	}
	cosmo_board_reset();
	cosmo_dma_reset(ALL_DMA_RESETS);
	if ( cosmo_fb_mem_dma() ) {
		msg_printf(SUM, "%s: test failed\n", self);
		return(cosmo_err_count);
	} else {
		msg_printf(SUM, "%s: test passed\n", self);
		return(0);
	}
}

int
fb_print_mem_error(char *cself, unsigned fbnum, unsigned offset,
		   unsigned cexpected, unsigned cgot)
{
	if ( ++mem_error_cnt < MAX_MEM_ERROR ) {
		msg_printf(ERR,
		  "%s: FB %d: offset 0x%x(0x%x),expected==0x%x,got 0x%x\n",
		  cself, fbnum, offset, (offset*4), cexpected, cgot);
	}
#ifdef notdef
	debug(0);
#endif /* notdef */
}

/*
 *	fb_mem_16_pio_test - simple memory test that write a value into
 *	each field buffer FIFO location in 16 bit mode and then read
 *	and compares for correct data.
 *	
 *	Error counts are left in fb_failed[n] for each of the 4 field
 *	buffer FIFO's.
 */
unsigned fb_failed[FIELD_BUFFER_COUNT];

int
fb_mem_16_pio_test(unsigned mem_data)
{
	char *self = "fb_mem_16_pio_test";
	volatile VFC_REGS *vfc_regs = cosmo_regs.jpeg_vfc;
	volatile long *uic_pio = cosmo_regs.jpeg_uic_pio;
	int i, j;
	unsigned dummy;

	cosmo_carob_reset(CRB_REV_VFCRESET);
	cosmo_set_vfc_mode(VFC_SRC_GIO);
	mem_error_cnt = 0;
	for ( i=FIELD_BUFFER_COUNT-1; i>=0; i-- ) {
		fb_failed[i] = 0;
		vfc_regs->vfc_write = (1<<i);
		dummy = vfc_regs->vfc_write;
		vfc_regs->vfc_write_ctl = W_RST;
		for ( j=0; j<FIELD_BUFFER_SIZE; j++ ) {
			dummy = mem_data & 0xffff;
			if ( (i==2) || (i==3) )
				dummy <<= 16;
			*uic_pio = dummy;
			if ( !(j&0x1ffff) ) {
				busy(1);
			}
		}
		dummy = vfc_regs->vfc_write;
		vfc_regs->vfc_write_ctl = W_RST;
	}
	/* initialize read port for each buffer */
	cosmo_set_vfc_mode(VFC_DEST_GIO);
	for ( i=FIELD_BUFFER_COUNT-1; i>=0; i-- ) {
		vfc_regs->vfc_read = (1<<i);
		dummy = vfc_regs->vfc_read;
		vfc_regs->vfc_read_ctl = R_RST;
		dummy = *uic_pio;	/* Tim's dummy read */
		for ( j=0; j<FIELD_BUFFER_SIZE; j++ ) {
			if ( (i==2) || (i==3) ) {
				dummy = ((*uic_pio)>>16) & 0xffff;
			} else {
				dummy = *uic_pio & 0xffff;
			}
			if ( dummy != (mem_data&0xffff) ) {
				msg_printf(DBG, "%s: mem_data==0x%x\n", 
							self, mem_data);
				fb_print_mem_error(self, i, j,
						mem_data&0xffff, dummy);
				fb_failed[i]++;
				cosmo_err_count++;
			}
			if ( !(j&0x1ffff) ) {
				busy(1);
			}
		}
		dummy = vfc_regs->vfc_read;
	}
	for ( i=0; i<FIELD_BUFFER_COUNT; i++ ) {
		msg_printf(DBG, "%s: fb_failed[%d]==0x%x\n",
						self, i, fb_failed[i]);
	}
	busy(0);
	return(0);
}

static int
fb_mem_16_pio_test1(unsigned mem_data)
{
	char *self = "fb_mem_16_pio_test1";
	int i;
	
	msg_printf(INFO, "%s: Write/Read/Compare 0x%x\n", self, mem_data);
	fb_mem_16_pio_test(mem_data);
	for ( i=0; i<FIELD_BUFFER_COUNT; i++ ) {
		if(fb_failed[i] > 0) {
			msg_printf(VRB, "%s: Field buffer %d (%d) had 0x%x errors\n", 
				self, i, (1<<i), fb_failed[i]);
		} else {
			msg_printf(VRB,"%s: Field buffer %d (%d) passed\n", 
				self, i, (1<<i));
		}
	}
	return(0);
}

static int
fb_mem_16_pio_walking(unsigned walking1)
{
	char *self = "fb_mem_16_pio_walking";
	unsigned i, k, mem_data;
	unsigned fb_bad[FIELD_BUFFER_COUNT] = { 0, 0, 0, 0 };
	
	if ( walking1 ) {
		msg_printf(INFO, "%s: Walking One's Test\n", self);
	} else {
		msg_printf(INFO, "%s: Walking Zero's Test\n", self);
	}
	for( k=0, mem_data=1; k<16; mem_data <<= 1, k++ ) {
		fb_mem_16_pio_test(mem_data);
		for ( i=0; i<FIELD_BUFFER_COUNT; i++ ) {
			if(fb_failed[i] > 0) {
				fb_bad[i]++;
				msg_printf(VRB, "%s: Field buffer %d 0x%x errors\n", 
					self, i, fb_failed[i]);
			}
		}
	}
	for ( i=0; i<FIELD_BUFFER_COUNT; i++ ) {
		if(fb_bad[i] > 0) {
			msg_printf(VRB, "%s: Field buffer %d (%d) had 0x%x errors\n", 
				self, i, (1<<i), fb_bad[i]);
		} else {
			msg_printf(VRB,"%s: Field buffer %d (%d) passed\n", 
				self, i, (1<<i));
		}
	}
	
}

/*
 *	fb_mem_32_pio_test - simple memory test that pio writes a value into
 *	each field buffer FIFO location in 32 bit mode and then pio reads
 *	and compares for correct data.
 *	
 *	Error counts are left in fb_failed[0,2] for each of the 2 32 bit field
 *	buffer FIFO's.
 */

int
fb_mem_32_pio_test(unsigned mem_data)
{
	char *self = "fb_mem_32_pio_test";
	volatile VFC_REGS *vfc_regs = cosmo_regs.jpeg_vfc;
	volatile long *uic_pio = cosmo_regs.jpeg_uic_pio;
	int i, j;
	unsigned dummy;

	cosmo_carob_reset(CRB_REV_VFCRESET);
	cosmo_set_vfc_mode((VFC_SRC_GIO|MODE_SIZE_32));
	mem_error_cnt = 0;
	for ( i=FIELD_BUFFER_COUNT-2; i>=0;  i -= 2 ) {
		msg_printf(DBG, "%s: writing FB %d\n", self, i );
		fb_failed[i] = 0;
		vfc_regs->vfc_write = (1<<i);
		dummy = vfc_regs->vfc_write;
		vfc_regs->vfc_write_ctl = W_RST;
		for ( j=0; j<FIELD_BUFFER_SIZE; j++ ) {
			*uic_pio = mem_data;
		}
		dummy = vfc_regs->vfc_write;
		vfc_regs->vfc_write_ctl = W_RST;
	}
	/* initialize read port for each buffer */
	cosmo_set_vfc_mode(VFC_DEST_GIO|MODE_SIZE_32);
	for ( i=FIELD_BUFFER_COUNT-2; i>=0; i -= 2 ) {
		msg_printf(DBG, "%s: reading FB %d\n", self, i );
		vfc_regs->vfc_read = (1<<i);
		dummy = vfc_regs->vfc_read;
		vfc_regs->vfc_read_ctl = R_RST;
		dummy = *uic_pio;	/* Tim's dummy read */
		for ( j=0; j<FIELD_BUFFER_SIZE; j++ ) {
			dummy = *uic_pio;
			if ( dummy != mem_data ) {
				if ( ++cosmo_err_count < 15 ) {
					fb_print_mem_error(self, i, j,
						mem_data, dummy);
				}
				fb_failed[i]++;
			}
			if ( !(j&0x1ffff) ) {
				busy(1);
			}
		}
		dummy = vfc_regs->vfc_read;
	}
	for ( i=0; i<FIELD_BUFFER_COUNT; i++ ) {
		msg_printf(DBG, "%s: fb_failed[%d]==0x%x\n",
						self, i, fb_failed[i]);
	}
	return(0);
}

static int
fb_mem_32_pio_test1(unsigned mem_data)
{
	char *self = "fb_mem_32_pio_test1";
	int i;
	
	msg_printf(INFO, "%s: Write/Read/Compare 0x%x\n", self, mem_data);
	fb_mem_32_pio_test(mem_data);
	for ( i=0; i<FIELD_BUFFER_COUNT; i++ ) {
		if(fb_failed[i] > 0) {
			msg_printf(VRB, "%s: Field buffer %d (%d) had 0x%x errors\n", 
				self, i, (1<<i), fb_failed[i]);
		} else {
			msg_printf(VRB,"%s: Field buffer %d (%d) passed\n", 
				self, i, (1<<i));
		}
	}
	return(0);
}

static int
fb_mem_32_pio_walking(unsigned walking1)
{
	char *self = "fb_mem_32_pio_walking";
	unsigned i, k, mem_data;
	unsigned fb_bad[FIELD_BUFFER_COUNT] = { 0, 0, 0, 0 };
	
	if ( walking1 ) {
		msg_printf(INFO, "%s: Walking One's Test\n", self);
	} else {
		msg_printf(INFO, "%s: Walking Zero's Test\n", self);
	}
	for( k=0, mem_data=1; k<32; mem_data <<= 1, k++ ) {
		fb_mem_32_pio_test(mem_data);
		for ( i=0; i<FIELD_BUFFER_COUNT; i++ ) {
			if(fb_failed[i] > 0) {
				fb_bad[i]++;
				msg_printf(VRB, "%s: Field buffer %d 0x%x errors\n", 
					self, i, fb_failed[i]);
			}
		}
	}
	for ( i=0; i<FIELD_BUFFER_COUNT; i++ ) {
		if(fb_bad[i] > 0) {
			msg_printf(VRB, "%s: Field buffer %d (%d) had 0x%x errors\n", 
				self, i, (1<<i), fb_bad[i]);
		} else {
			msg_printf(VRB,"%s: Field buffer %d (%d) passed\n", 
				self, i, (1<<i));
		}
	}
	
}

/*#define DMA_BUFFER_SIZE 0x4000*/
#define DMA_BUFFER_SIZE 0x200
unsigned fb_dma_bufferx[DMA_BUFFER_SIZE+0x2000];
unsigned *fb_dma_buffer = 0;

int
fb_write_dma_data(unsigned mem_data, unsigned mem_size)
{
	char *self = "fb_write_dma_data";
	int i;
	unsigned dma_size = 0;

	msg_printf(DBG,"%s: data==0x%x, size==0x%x\n", 
					self, mem_data, mem_size);
	for ( i = 0; i< DMA_BUFFER_SIZE; i++ ) {
		fb_dma_buffer[i] = mem_data;
	}
	for ( i = 0; i < mem_size; i+= dma_size ) {
		dma_size = min(DMA_BUFFER_SIZE,(mem_size-i));
		if ( ucc_dma(fb_dma_buffer,dma_size) == -1 ) {
			return(-1);
		}
		if ( !(i&0x1ffff) ) {
			busy(1);
		}
	}
	return(0);
}

int
fb_read_dma_data(unsigned *mem_addr, unsigned mem_size)
{
	char *self = "fb_read_dma_data";
	volatile long *uic_pio = cosmo_regs.jpeg_uic_pio;
	int i;

	for ( i = 0; i< DMA_BUFFER_SIZE; i++ ) {
		fb_dma_buffer[i] = 0x11111111;
	}
	for ( i = 0; i < mem_size; i++ ) {
#ifndef notdef
		*mem_addr++ = *uic_pio;
#else /* notdef */
		*mem_addr = *uic_pio;
		if ( *mem_addr != 0xa5a5a5a5 ) {
			debug(0);
		}
		mem_addr++;
#endif /* notdef */
	}
}

/*
 *	fb_mem_32_dma_test - simple memory test that dma writes a value into
 *	each field buffer FIFO location in 32 bit mode and then dma reads
 *	and compares for correct data.
 *	
 *	Error counts are left in fb_failed[0,2] for each of the 2 32 bit field
 *	buffer FIFO's.
 */

int
fb_mem_32_dma_test(unsigned mem_data)
{
	char *self = "fb_mem_32_dma_test";
	volatile VFC_REGS *vfc_regs = cosmo_regs.jpeg_vfc;
	volatile long *uic_pio = cosmo_regs.jpeg_uic_pio;
	int i, j, k;
	unsigned dma_size = 0;
	unsigned dummy, *data_ptr;
	unsigned readptr, writeptr;

	cosmo_carob_reset(CRB_REV_VFCRESET);
	cosmo_set_vfc_mode((VFC_SRC_GIO|MODE_SIZE_32));
	mem_error_cnt = 0;
	for ( i=FIELD_BUFFER_COUNT-2; i>=0;  i -= 2 ) {
		msg_printf(DBG, "%s: writing FB %d\n", self, i );
		fb_failed[i] = 0;
		vfc_regs->vfc_read = 0;	/* get read pointer out of the way */
		vfc_regs->vfc_write = (1<<i);
		readptr = vfc_regs->vfc_read;
		writeptr = vfc_regs->vfc_write;
		msg_printf(DBG, "%s: readptr==0x%x writeptr==0x%x\n",
					self, readptr, writeptr);
		dummy = vfc_regs->vfc_write;
		vfc_regs->vfc_write_ctl = W_RST;
		if ( fb_write_dma_data(mem_data,FIELD_BUFFER_SIZE) == -1 ) {
			return(-1);
		}
		/*fb_write_dma_data(mem_data,0x4000);*/
		dummy = vfc_regs->vfc_write;
		vfc_regs->vfc_write_ctl = W_RST;
	}
	/* initialize read port for each buffer */
	cosmo_set_vfc_mode(VFC_DEST_GIO|MODE_SIZE_32);
	for ( i=FIELD_BUFFER_COUNT-2; i>=0; i -= 2 ) {
		msg_printf(DBG, "%s: reading FB %d\n", self, i );
		vfc_regs->vfc_read = (1<<i);
		dummy = vfc_regs->vfc_read;
		vfc_regs->vfc_read_ctl = R_RST;
		dummy = *uic_pio;	/* Tim's dummy read */
#ifndef notdef
		for ( j=0; j<FIELD_BUFFER_SIZE; j += dma_size ) {
			dma_size = min(DMA_BUFFER_SIZE,
						(FIELD_BUFFER_SIZE-j));
#else /* notdef */
		for ( j=0; j<0x4000; j += dma_size ) {
			dma_size = min(DMA_BUFFER_SIZE,
						(0x4000-j));
#endif /* notdef */
			fb_read_dma_data(fb_dma_buffer, dma_size);
			data_ptr = fb_dma_buffer;
			for (k=0; k<dma_size; k++ ) {
				dummy = *data_ptr++;
				if ( dummy != mem_data ) {
					if ( ++cosmo_err_count < 15 ) {
						fb_print_mem_error(self, i, k,
							mem_data, dummy);
					}
					fb_failed[i]++;
				}
			}
			if ( !(j&0xffff) ) {
				busy(1);
			}
		}
		dummy = vfc_regs->vfc_read;
	}
	busy(0);
	return(0);
}

static int
fb_mem_32_dma_test1(unsigned mem_data)
{
	char *self = "fb_mem_32_dma_test1";
	int i;
	
	msg_printf(INFO, "%s: Write/Read/Compare 0x%x\n", self, mem_data);
	fb_mem_32_dma_test(mem_data);
	for ( i=0; i<FIELD_BUFFER_COUNT; i+=2) {
		if(fb_failed[i] > 0) {
			msg_printf(VRB, "%s: Field buffer %d (%d) had 0x%x errors\n", 
				self, i, (1<<i), fb_failed[i]);
		} else {
			msg_printf(VRB, "%s: Field buffer %d (%d) passed\n", 
				self, i, (1<<i));
		}
	}
	return(0);
}

static void
fb_mem_32_dma_walking(unsigned walking1)
{
	char *self = "fb_mem_32_dma_walking";
	unsigned i, k, mem_data;
	unsigned fb_bad[FIELD_BUFFER_COUNT] = { 0, 0, 0, 0 };
	
	if ( walking1 ) {
		msg_printf(INFO, "%s: Walking One's Test\n", self);
	} else {
		msg_printf(INFO, "%s: Walking Zero's Test\n", self);
	}
	for( k=0, mem_data=1; k<32; mem_data <<= 1, k++ ) {
		fb_mem_32_dma_test(mem_data);
		for ( i=0; i<FIELD_BUFFER_COUNT; i+=2 ) {
			if(fb_failed[i] > 0) {
				fb_bad[i]++;
				msg_printf(VRB, "%s: Field buffer %d 0x%x errors\n", 
					self, i, fb_failed[i]);
			}
		}
	}
	for ( i=0; i<FIELD_BUFFER_COUNT; i+=2 ) {
		if(fb_bad[i] > 0) {
			msg_printf(VRB, "%s: Field buffer %d (0x%x) had 0x%x errors\n", 
				self, i, (1<<i), fb_bad[i]);
		} else {
			msg_printf(VRB, "%s: Field buffer %d (%d) passed\n", 
				self, i, (1<<i));
		}
	}
	
}

void
cosmo_init_field_buffers(void)
{
	char *self = "_cosmo_init_field_buffers";
	volatile VFC_REGS *vfc_regs = cosmo_regs.jpeg_vfc;
	volatile long *uic_pio = cosmo_regs.jpeg_uic_pio;
	int i,j,dummy;

	cosmo_carob_reset(CRB_REV_VFCRESET);
	/* Initialize write port of each buffer */
	cosmo_set_vfc_mode(VFC_SRC_GIO|VFC_DEST_GIO);
	for(i=0; i<FIELD_BUFFER_COUNT; i++)
	{
		vfc_regs->vfc_write = 1 << i ;
		vfc_regs->vfc_write_ctl |= W_RST;
		for ( j=0; j<FIELD_BUFFER_INIT_SIZE; j++ ) {
			if ( (i==2) || (i==3) ) {
				dummy = (j<<16);
			} else {
				dummy = j;
			}
			*uic_pio = j;
		}
		vfc_regs->vfc_write_ctl = W_RST;
	}

	/* Initialize read port of each buffer */
	for(i=0; i<FIELD_BUFFER_COUNT; i++)
	{
		vfc_regs->vfc_read = 1 << i ;
		vfc_regs->vfc_read_ctl = 0;
		vfc_regs->vfc_read_ctl |= R_RST;
		dummy = *uic_pio;
		dummy = *uic_pio;
		vfc_regs->vfc_read_ctl |= R_RST;
		for(j=0; j<FIELD_BUFFER_INIT_SIZE; j++)
		{
			dummy = *uic_pio;
		}
		vfc_regs->vfc_read_ctl |= R_RST;
		dummy = *uic_pio;
		dummy = *uic_pio;
		vfc_regs->vfc_read_ctl |= R_RST;
	}
}
