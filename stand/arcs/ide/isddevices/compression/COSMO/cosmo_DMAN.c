#include "cosmo_hw.h"
#include "cosmo_ide.h"
#define _DMAN_Reset	4
#define _DMAN_Int	8
#define _DMAN_Int_Mask	0xc
#define _DMAN_Short_Burst	0x10
#define _DMAN_CC_Control	0x14
#define _DMAN_UCC_Control	0x18
#define _DMAN_Breq_Holdoff	0x1c

struct DMAN_reg_table {
	char	DMAN_comment[48];
	long	DMAN_offset;
	unsigned DMAN_num_bits;
	long	DMAN_bits;
};

struct DMAN_reg_table DMANregtab[] = {
	"Reset Register", _DMAN_Reset, 4, 0xf,
	"Interrupt Mask Register", _DMAN_Int_Mask, 7, 0x7f,
	"Short Burst Length Register", _DMAN_Short_Burst, 8, 0xff,
	"Compressed Channel Control Register", _DMAN_CC_Control, 2, 0x2,
	"Uncompressed Channel Control Register", _DMAN_UCC_Control, 2, 0x2,
	"BReq Holdoff Time Register", _DMAN_Breq_Holdoff, 10, 0x3ff,
	"", -1, 0, 0
};
cosmo_DMAN_reg(int argc, char **argv)
{
	char *self = "cosmo_DMAN_reg";
	unsigned reg_addr, tindx = 0;
	long	numerrors = 0;

	if ( cosmo_probe_for_it() == -1 ) {
		return(-1);
	}
	cosmo_board_reset();
	cosmo_dma_reset(ALL_DMA_RESETS);
	while ( DMANregtab[tindx].DMAN_offset != -1 ) {
		msg_printf(INFO, "%s: testing %s\n", self,
				DMANregtab[tindx].DMAN_comment);
		reg_addr = ((unsigned)(cosmo_regs.jpeg_dma_ctrl) - _DMAN_Reset) +
					DMANregtab[tindx].DMAN_offset;
		numerrors += cosmo_test_bits((unsigned *)reg_addr, DMANregtab[tindx].DMAN_num_bits,
						DMANregtab[tindx].DMAN_bits);
		tindx++;
	}
	if ( numerrors ) {
		msg_printf(SUM, "%s: DMAN Register test failed\n", self);
		return(-1);
	} else {
		msg_printf(SUM, "%s: DMAN Register test passed\n", self);
		return(0);
	}
}
