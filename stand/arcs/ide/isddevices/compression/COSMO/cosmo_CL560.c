#include "cosmo_hw.h"
#include "cosmo_ide.h"
#include "cosmo_misc.h"
#include "cosmo_cl560.h"
#include <libsc.h>

struct cl560_reg_table {
	char	reg_comment[32];
	long	reg_index;
	unsigned reg_num_bits;
	long	reg_bits;
};

struct cl560_reg_table CL560regtab[] = {
	"IRQ1 Mask Register", _IRQ1_Mask, 16, 0xffff,
	"IRQ2 Mask Register", _IRQ2_Mask, 16, 0xffff,
	"", -1, 0, 0
};

/******************************************************************************/
/*                                                                            */
/*  CL560Register[reg number] is an array that supplies the name, address     */
/*   (from base address), the read/write restrictions and the number of bits  */
/*   for each of the registers used, not only by the chip but also by the     */
/*   board.                                                                   */
/*                                                                            */
/******************************************************************************/

JPEG_reg_struct CL560Register[] =
{
/*                              Permission                                   */
/* Name              Address           Width             Also referenced by  */

{"HPeriod ",        0x00008000L,    RW, 14},
{"HSync   ",           0x00008004L,    RW, 14},
{"HDelay  ",        0x00008008L,    RW, 14},
{"HActive ",         0x0000800CL,    RW, 12},
{"VPeriod ",         0x00008010L,    RW, 14},
{"VSync   ",           0x00008014L,    RW, 11},
{"VDelay  ",        0x00008018L,    RW, 14},
{"VActive ",         0x0000801CL,    RW, 11},

{"DCT",             0x00008800L,    WW, 16},

{"Config  ",        0x00009000L,    RW, 9 },
{"Huff_Enable",     0x00009004L,    WW, 1 },    /* Huffman table load enable  */
{"S_reset",         0x00009008L,    WW, 1 },
{"Start   ",          0x0000900CL,    RW, 1 },    /* WW -> RW */
{"HV_Enable",       0x00009010L,    RW, 1 },    /* WW -> RW */

{"Flags   ",          0x00009014L,    RW, 16},    /* 14 -> 16 */

{"NMRQ_Enb",     0x00009018L,    RW, 16},    /* WW -> RW */
                                                /* Interupt mask              */
{"DRQ_Enable",      0x0000901CL,    RW, 16},    /* WW -> RW */
                                                /* DMA request interupt mask  */
{"StartFrame",      0x00009020L,    WW, 1 },
{"Version ",         0x00009024L,    RR, 3 },

{"Init_1",          0x00009800L,    WW, 7 },    /* ZigZag1, Zigzag sync 1     */
{"Init_2",          0x00009804L,    WW, 7 },    /* ZigZag2, Zigzag sync 2     */

{"CodecSeq",        0x0000A000L,    WW, 10},    /* Huffman table sequence     */
{"CodecCompH",      0x0000A004L,    WW, 10},    /* DPCM register sequence HI  */
{"CodecCompL",      0x0000A008L,    WW, 10},    /* DPCM register sequence LO  */
{"CoderAttr ",      0x0000A00CL,    WW, 7 },
{"CodingIntH",      0x0000A010L,    WW, 8 },    /* Coder coding interval HI   */
{"CodingIntL",      0x0000A014L,    WW, 8 },    /* Coder coding interval LO   */

{"DecLength",       0x0000A80CL,    RW, 4 },    /* Decoder sequence length    */
{"DecMarker",       0x0000A810L,    RR, 8 },
{"DecResume",       0x0000A814L,    WW, 1 },    /* Decoder's resume flag      */
{"DecDPCM",         0x0000A818L,    WW, 1 },    /* Decoder DPCM register reset*/
{"CodeOrder",       0x0000A81CL,    RW, 1 },

{"Init_3  ",          0x0000B600L,    RW, 11},    /* WW -> RW */
                                                /* Zero Packer/Unpacker sync  */

{"Quant   ",           0x0000B800L,    RW, 16},
{"QuantSelect",     0x0000BC00L,    WW, 1 },
{"QuantSync",       0x0000BE00L,    WW, 14},    /* Video sync                 */
{"QuantYC",         0x0000BE08L,    WW, 14},
{"QuantAB",         0x0000BE0CL,    WW, 10},

    /* Color Transformation Matrix  0xC000L - 0xC020L */

{"R2Y",             0x0000C000L,    WW, 12},
{"G2Y",             0x0000C004L,    WW, 12},
{"B2Y",             0x0000C008L,    WW, 12},
{"R2U",             0x0000C00CL,    WW, 12},
{"G2U",             0x0000C010L,    WW, 12},
{"B2U",             0x0000C014L,    WW, 12},
{"R2V",             0x0000C018L,    WW, 12},
{"G2V",             0x0000C01CL,    WW, 12},
{"B2V",             0x0000C020L,    WW, 12},

{"VidLatency",    0x0000C030L,    RW, 14},    /* 11 -> 14 */
{"HControl",        0x0000C034L,    RW, 14},    /* Horizontal count, ZVA reset*/
                                                /*    Horiz. count            */
{"VControl",        0x0000C038L,    RW, 14},    /* 13 -> 14 */
                                                /* Vertical count, ZVA reset  */
                                                /*    Vert. count             */
{"VertLnCnt",   0x0000C03CL,    RR, 14},    /* 13 -> 14 */
                                            
{"Init_4",          0x0000CF00L,    WW, 10},    /* Block storage sync         */

{"Init_7",          0x0000D400L,    WW, 16},    /* DCT memory                 */

{"FIFO    ",        0x0000D800L,    RW, 13},


{"HuffYAC ",         0x0000E000L,    RW, 9 },    /* 560 only, e000-e5fc (384 words) */
{"HuffYDC ",         0x0000E600L,    RW, 9 },    /* 560 only, e600-e65c (23 words) */
{"HuffCAC ",         0x0000E800L,    RW, 9 },    /* 560 only, e800-edfc (384 words) */
{"HuffCDC ",         0x0000EE00L,    RW, 9 },    /* 560 only, ee00-ee5c (23 words) */
{"Codec",           0x00000000L,    DF, 32},
{"Block   ",        0x0000C800L,    RR, 16},    /* Block and BlockAddress are */
{"BlkAddr ",         0x0000CE00L,    RR, 8 },    /* used solely for testing    */
                                                /* purposes.                  */

{"Init_5",          0x00008820L,    WW, 16},    /* RW -> WW */
                                                /* DCT core sync              */
{"Init_6",          0x00008824L,    WW, 16},    /* WW -> RW */
                                                /* DCT control, Ctest, Dtest  */
{"IRQ1    ",       0x00009018L,    RW, 16},
{"IRQ2    ",       0x00009028L,    RW, 16},
{"FRMEND_Enable    ",       0x0000902CL,    RW, 16},
{"CoderSync",       0x0000A020L,    WW, 10},    /* Coder Sync register */
{"CWrdCntHigh",          0x0000A024L,    RW, 16},    /* RW -> RW */
{"CWrdCntLow",          0x0000A028L,    RW, 16},    /* RW -> RW */
{"CoderRateAct",          0x0000A02CL,    RW, 1},    /* RW -> RW */
{"CoderRateEnb",          0x0000A030L,    RW, 5},    /* RW -> RW */
{"CoderRobAct",          0x0000A034L,    RW, 1},    /* RW -> RW */
{"CoderRSTPad",          0x0000A038L,    RW, 16},    /* RW -> RW */
{"DecoderStrt",          0x0000A820L,    WW, 1},    /* RW -> RW */
{"DecoderMis",          0x0000A824L,    RW, 1},
{"DecoderMisErr", 0x0000A828L,    RR, 16},
{"FIFOLevel",       0x0000DA04L,    RR, 16}
};

void
writecodec(void)
{
	char *self = "writecodec";
	volatile unsigned *codecAddress = (unsigned *)
		((unsigned)cosmo_regs.jpeg_cl560 + CL560Register[_Codec].address);
	int i;
  
	*codecAddress = 0xffffffff;
	*codecAddress = 0;
	for(i=0; i<30; i++) {
		*codecAddress = 1<<i;
	}
	*codecAddress = 0xdeadbeef;
	*codecAddress = 0xcafebabe;
}

void
emptycodec(void)
{
	char *self = "emptycodec";
	volatile unsigned *codecAddress = (unsigned *)
		((unsigned)cosmo_regs.jpeg_cl560 + CL560Register[_Codec].address);
	volatile unsigned *flags =
		(unsigned *)((unsigned)cosmo_regs.jpeg_cl560 +
					CL560Register[_Flags].address);
	unsigned actual;
	unsigned flags_value;
  
	while(!(flags_value = *flags & _Flags_FIFO_Empty)) {
		actual = *codecAddress;
	}
	*flags = 1;
}

read1codec(unsigned expected)
{
	char *self = "read1codec";
	volatile unsigned *codecAddress = (unsigned *)
		((unsigned)cosmo_regs.jpeg_cl560 + CL560Register[_Codec].address);
	volatile unsigned *flags =
		(unsigned *)((unsigned)cosmo_regs.jpeg_cl560 +
					CL560Register[_Flags].address);
	unsigned actual;
  
	if(*flags & _Flags_FIFO_Empty) {
		printf("actual: EMPTY,\texpected: 0x%x\n", expected);
		return 1;
	}
	actual = *codecAddress;
	if(actual != expected) {
		printf("actual: 0x%x,\texpected: 0x%x\n", actual, expected);
		return 1;
	} else return 0;
}

int
readcodec(void)
{
	char *self = "readcodec";
	int numerrors = 0, i;
  
	numerrors += read1codec(0xffffffff);
	numerrors += read1codec(0);
	for(i=0; i<30; i++)
		numerrors += read1codec(1<<i);
	numerrors += read1codec(0xdeadbeef);
	numerrors += read1codec(0xcafebabe);
	return numerrors;
}

void
cosmo_reset_560(void)
{
	char *self = "cosmo_reset_560";

	regWrite(_S_reset,1L,0);
	msg_printf(DBG, "%s: CL560 Reset\n", self);
}

int
cosmo_CL560_reg(int argc, char **argv)
{
	char *self = "cosmo_CL560_reg";
	unsigned reg_offset, tindx = 0;
	long	numerrors = 0;

	if ( cosmo_probe_for_it() == -1 ) {
		return(-1);
	}
	cosmo_board_reset();
	cosmo_dma_reset(ALL_DMA_RESETS);
	cosmo_reset_560();
	while ( CL560regtab[tindx].reg_index != -1 ) {
		msg_printf(INFO, "%s: testing %s\n", self,
				CL560regtab[tindx].reg_comment);
		reg_offset =
		  (unsigned)CL560Register[CL560regtab[tindx].reg_index].address;
		numerrors += cosmo_test_bits((unsigned *)(cosmo_regs.jpeg_cl560)+reg_offset,
					CL560regtab[tindx].reg_num_bits,
					CL560regtab[tindx].reg_bits);
		tindx++;
	}
	if ( numerrors ) {
		msg_printf(SUM, "%s: CL560 Register test failed\n", self);
		return(-1);
	} else {
		msg_printf(SUM, "%s: CL560 Register test passed\n", self);
		return(0);
	}
}

cosmo_CL560_codec(int argc, char **argv)
{
	char *self = "cosmo_CL560_codec";
	volatile unsigned *config;
	volatile unsigned *IRQ2;
	volatile unsigned *DRQ;
	unsigned numerrors = 0;

	if ( cosmo_probe_for_it() == -1 ) {
		return(-1);
	}
	cosmo_board_reset();
	cosmo_dma_reset(ALL_DMA_RESETS);
	cosmo_carob_reset(CRB_REV_VFCRESET);
	cosmo_reset_560();
	config = (unsigned *)((unsigned)cosmo_regs.jpeg_cl560 +
				CL560Register[_Config].address);
	IRQ2 = (unsigned *)((unsigned)cosmo_regs.jpeg_cl560 +
				CL560Register[_IRQ2_Mask].address);
	DRQ = (unsigned *)((unsigned)cosmo_regs.jpeg_cl560 +
				CL560Register[_DRQ_Enable].address);
	*IRQ2 = _Flags_FIFO_Empty;
	*DRQ = _Int_FIFO_3QFull;
	*config = 0x38;
	emptycodec();
	*config = 0x138;
	writecodec();
	*config = 0x38;
	numerrors += readcodec();
	if ( numerrors ) {
		msg_printf(SUM, "%s: Codec test failed\n", self);
		return(-1);
	} else {
		msg_printf(SUM, "%s: Codec test passed\n", self);
		return(0);
	}
}
