
/*****************************************************************************
 *
 * Copyright 1993, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 *
 *****************************************************************************/

#ifndef __COSMO_IDE_H_
#define __COSMO_IDE_H_
#include <uif.h>

extern cosmo_probe_for_it(void);
void cosmo_board_reset(void);
extern cosmo_dma_reset(unsigned);
extern cosmo_set_vfc_mode(unsigned);
extern cosmo_carob_reset(unsigned);
int ucc_dma(unsigned *dma_buffer, unsigned dma_size);
int cosmo_test_bits(volatile unsigned *, unsigned, unsigned);
void regRead(short Reg,unsigned long *Data, short Offset);
void regWrite(short Reg,unsigned long Data, short Offset);
int cosmo_init_7186(void);
void cosmo_reset_560(void);
void cosmo_init_field_buffers(void);
int cosmo_fill_image(void);
void do_decomp_compare(void);
int do_comp_compare(void);
void cosmo_7186_set_rgbmode(int);
void cosmo_comp_print_err(char *,unsigned,unsigned,unsigned);
void LoadHuffTabFromArray(unsigned short *);
void LoadQTabFromArray(unsigned short *);

void do_Reportlevel(void);
void do_noReportlevel(void);

extern JPEG_Regs cosmo_regs;
extern unsigned cosmo_err_count;

#define UCC_TCR_SIZE	0x800
#define NUM_UCC_TCRS	100
#endif /* __COSMO_IDE_H_ */
