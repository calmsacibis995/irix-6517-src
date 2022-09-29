#include "cosmo_hw.h"
#include "cosmo_ide.h"

#define _CAROB_Mode	0
#define _CAROB_Int_Mask	0x20
#define _CAROB_D1_Config	0x24
#define _CAROB_Read_Pointer	4
#define _CAROB_Write_Pointer	0x10

struct CAROB_reg_table {
	char	CAROB_comment[48];
	long	CAROB_offset;
	unsigned CAROB_num_bits;
	long	CAROB_bits;
};

struct CAROB_reg_table CAROBregtab[] = {
	"Mode Register", _CAROB_Mode, 4, 0xf,
	"Interrupt Mask Register", _CAROB_Int_Mask, 5, 0x1f,
	"D1 Configuration Register", _CAROB_D1_Config, 10, 0x3ff,
	/*"Read Pointer Register", _CAROB_Read_Pointer, 4, 0xf,*/
	/*"Write Pointer Register", _CAROB_Write_Pointer, 4, 0xf,*/
	"", -1, 0, 0
};

cosmo_CAROB_reg(int argc, char **argv)
{
	char *self = "cosmo_CAROB_reg";

	unsigned reg_addr, tindx = 0;
	long	numerrors = 0;

	if ( cosmo_probe_for_it() == -1 ) {
		return(-1);
	}
	cosmo_board_reset();
	cosmo_dma_reset(ALL_DMA_RESETS);
	while ( CAROBregtab[tindx].CAROB_offset != -1 ) {
		msg_printf(INFO, "%s: testing %s\n", self,
				CAROBregtab[tindx].CAROB_comment);
		reg_addr = (unsigned)(cosmo_regs.jpeg_vfc) +
					CAROBregtab[tindx].CAROB_offset;
		numerrors += cosmo_test_bits((unsigned *)reg_addr, CAROBregtab[tindx].CAROB_num_bits,
						CAROBregtab[tindx].CAROB_bits);
		tindx++;
	}
	if ( numerrors ) {
		msg_printf(SUM, "%s: CAROB Register test failed\n", self);
		return(-1);
	} else {
		msg_printf(SUM, "%s: CAROB Register test passed\n", self);
		return(0);
	}
}
