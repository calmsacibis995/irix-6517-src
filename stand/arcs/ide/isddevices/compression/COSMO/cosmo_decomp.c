#include <sys/types.h>
#include "cosmo_hw.h"
#include "cosmo_drv.h"
#include "cosmo_ide.h"
#include "cosmo_cl560.h"
#ifdef notdef
#include "cosmo_huff.h"
#include "cosmo_QArray.h"
#endif /* notdef */
#include <libsc.h>
#include <libsk.h>

extern unsigned fb_failed[FIELD_BUFFER_COUNT];
extern unsigned mem_error_cnt;
extern unsigned long *CL560CompressedDataPtr;
extern int comp_good_data[];
extern unsigned long CL560CodeSize;
extern int vfc32bitmode;

unsigned cosmo_decomp_error = 0;
unsigned long CL560CodeCount;
int prefilling;
int Finished, numimages;
unsigned long calculateHPeriod(short localHPeriod);
unsigned long calculateHPeriod(short);
unsigned long calculateHDelay(short);
unsigned long calculateHSync(short, short );
unsigned long calculateHActive(short);
unsigned long calculateVPeriod(short);
unsigned long calculateVDelay(short, short, short);
unsigned long calculateVSync(short);
unsigned long calculateVActive(short);
unsigned long calculateConfig(void);
unsigned long calculateHControl(short);
unsigned long calculateVControl(short, short, short);

extern JPEG_reg_struct CL560Register[];
extern Ptr CompressedDataPtr, ImageDataPtr;
extern unsigned short CandD560_HuffArray[];
extern unsigned short DefaultDQ50Array1[];
extern unsigned long D_InitRegValues[_NumInitReg][_NumPixelMode];
extern unsigned long ConfigRegValues[_RegName][_NumPixelMode];
extern short MODE_PIXEL,MODE_DECOMPRESS;
extern int PIXELS, LINES;

int
cosmo_fill_image(void)
{
	char *self = "cosmo_fill_image";
	int i, j;
	unsigned *dptr;

	if ((CompressedDataPtr == NULL) && 
		((CompressedDataPtr = (Ptr) malloc((size_t)_C_Data_size))
							== NULL)) {
		msg_printf(ERR, "no compressed data memory\n");
		cosmo_decomp_error++;
		return(-1);
	}
	if ((ImageDataPtr == NULL) &&
		((ImageDataPtr = (Ptr) malloc((size_t)_I_Data_size))
							== NULL)) {
		msg_printf(ERR, "no image pointer memory\n");
		cosmo_decomp_error++;
		return(-1);
	}
	dptr = (unsigned *)CompressedDataPtr;
	for ( i=0, j=0; i<_C_Data_size; i+=4, j++) {
		*dptr++ = comp_good_data[j%5];
	}
	msg_printf(DBG, "%s: filled from 0x%x to 0x%x\n", self,
			CompressedDataPtr, dptr);

	return 0;
}

/******************************************************************************/
/*                                                                            */
/*  Subroutine __FillFIFO() fills the FIFO to 3/4 full.                       */
/*                                                                            */
/******************************************************************************/

unsigned fillfifocount = 0;
unsigned lastsaved = 0;
unsigned print_count = 0;
int	printtheping = 0;

void __FillFIFO(void)
{
	char *self = "__FillFIFO";
	volatile unsigned *codecAddress = (unsigned *)
	  ((unsigned)cosmo_regs.jpeg_cl560 + CL560Register[_Codec].address);
	unsigned long Data, Data2;
	unsigned long *dptr;
	unsigned int d0,d1,d2,d3;
	int i, j, wordstowrite= 0, wordchunks;
	unsigned status;
	volatile VFC_REGS *vfc_regs = cosmo_regs.jpeg_vfc;
	static int nummcus = 0, numextrawords = 4, targetnummcus = 25;

	regRead(_Flags,&Data,0);
	msg_printf(DBG, "%s: flags==0x%x (pre==0x%x)\n",
				self, Data&0xffff, prefilling);
	if (!prefilling) {
		if(Data & _Flags_FIFO_Late) {
			msg_printf(DBG, "\nL:0x%x \n",Data);
			regWrite(_Flags,-1L,0); /*       clear late bit   */
			regWrite(_Flags,0L,0);  /*       clear late bit   */
			us_delay(0x20);
			regRead(_Flags,&Data,0);
		}
	}
	while(Data & _Flags_FIFO_Not3QFull)
	{
		if(CL560CodeCount<=0) {
			numimages--;
			if(numimages > 0) {
				*codecAddress = 0xffffffd0;
				CL560CompressedDataPtr =
				    (unsigned long *)CompressedDataPtr;
				CL560CodeCount = (CL560CodeSize+3)/4;
			} else {
				/* let chip stop after the image */
				msg_printf(DBG,"%s: chip stop\n", self);
				regWrite(_HV_Enable,1L,0);
				regWrite(_Start,0L,0);
				regWrite(_IRQ2_Mask, _Flags_FIFO_QFull,0);
				Finished=1;
				goto fillfifoout;
			}
		}
		if(Data & _Flags_FIFO_NotQFull) {
			if(CL560CodeCount >= 92) {
				msg_printf(DBG,"%s: doing 0x%x bytes\n",
						self, 92);
				CL560CodeCount -= 92;
				for(i=0; i<23; i++) {
					d0 = CL560CompressedDataPtr[0];
					d1 = CL560CompressedDataPtr[1];
					d2 = CL560CompressedDataPtr[2];
					d3 = CL560CompressedDataPtr[3];
					*codecAddress = d0;
					*codecAddress = d1;
					*codecAddress = d2;
					*codecAddress = d3;
					fillfifocount += 16;
					CL560CompressedDataPtr+= 4;
				}
				wordstowrite = 92;
				regRead(_Flags,&Data,0);
				continue;
			} else wordstowrite = CL560CodeCount;
		} else if(Data & _Flags_FIFO_NotHalfFull) {
			if(CL560CodeCount >= 60) {
				CL560CodeCount -= 60;
				msg_printf(DBG,"%s: doing 0x%x bytes\n",
						self, 60);
				for(i=0; i<15; i++) {
					d0 = CL560CompressedDataPtr[0];
					d1 = CL560CompressedDataPtr[1];
					d2 = CL560CompressedDataPtr[2];
					d3 = CL560CompressedDataPtr[3];
					*codecAddress = d0;
					*codecAddress = d1;
					*codecAddress = d2;
					*codecAddress = d3;
					fillfifocount += 16;
					CL560CompressedDataPtr+= 4;
				}
				wordstowrite = 60;
				regRead(_Flags,&Data,0);
				continue;
			} else wordstowrite = CL560CodeCount;
		} else {
			if(CL560CodeCount >= 28) {
				CL560CodeCount -= 28;
				msg_printf(DBG,"%s: doing 0x%x bytes\n",
						self, 28);
				for(i=0; i<7; i++) {
					d0 = CL560CompressedDataPtr[0];
					d1 = CL560CompressedDataPtr[1];
					d2 = CL560CompressedDataPtr[2];
					d3 = CL560CompressedDataPtr[3];
					*codecAddress = d0;
					*codecAddress = d1;
					*codecAddress = d2;
					*codecAddress = d3;
					fillfifocount += 16;
					CL560CompressedDataPtr+= 4;
				}
				wordstowrite = 28;
				regRead(_Flags,&Data,0);
				continue;
			} else wordstowrite = CL560CodeCount;
		}
		CL560CodeCount -= wordstowrite;

		wordchunks = wordstowrite/4;
		for(i=0; i<wordchunks; i++) {
			d0 = CL560CompressedDataPtr[0];
			d1 = CL560CompressedDataPtr[1];
			d2 = CL560CompressedDataPtr[2];
			d3 = CL560CompressedDataPtr[3];
			*codecAddress = d0;
			*codecAddress = d1;
			*codecAddress = d2;
			*codecAddress = d3;
			fillfifocount += 16;
			CL560CompressedDataPtr+= 4;
		}
		for(i=0; i<wordstowrite-wordchunks*4; i++) {
			d0 = CL560CompressedDataPtr[0];
			d1 = CL560CompressedDataPtr[1];
			*codecAddress = *CL560CompressedDataPtr++;
			fillfifocount += 4;
		}

		regRead(_Flags,&Data,0);
	}
	if(Data&_Flags_MarkerCode) {
		regRead(_DecMarker,&Data2,0); /* check if marker codes have */
		/* been detected                 */
		msg_printf(ERR, "marker code bit high. flags: 0x%x, DecMarker: 0x%x\n",
		    Data, Data2);
		msg_printf(ERR, "CL560CodeCount: %d\n", CL560CodeCount);
		msg_printf(ERR, "Codec contents:\n\n");
		regWrite(_Config,0x38,0);

		for(i=0; i<32; i++) {
			regRead(_Flags, &Data, 0);
			if((Data&0x4000) == 0) break;
			regRead(_Codec, &Data, 0);
			msg_printf(ERR, "0x%x ", Data);
		}
		msg_printf(ERR, "\n");
		if((Data2&0xffff) != 0)
		{
			msg_printf(ERR, "_DecMarker = %.8lx\n",Data2);
			goto fillfifoout;
		}
	}
fillfifoout:

	if ( fillfifocount > lastsaved ) {
		print_count = 0;
	}
	lastsaved = fillfifocount;
	if ( !(++print_count % 50000) ) {
		lastsaved--;
		goto fillfifoout;
	}
	msg_printf(DBG, "%s: exiting (0x%x, 0x%x)\n", self,
			CL560CodeCount, vfc_regs->vfc_intr&0xffff);
}

/******************************************************************************/
/*                                                                            */
/*  D_LoadRegisters() loads the appropriate values into the chip registers for*/
/*    decompression                                                           */
/*                                                                            */
/******************************************************************************/

void D_LoadRegisters(void)
{
	unsigned long Data, data;
	unsigned long  LeftBlanking, RightBlanking;
	unsigned long  TopBlanking, BottomBlanking;

	MODE_DECOMPRESS = 1;
	CL560ImageWidth = PIXELS;
	CL560ImageHeight = LINES;
	LeftBlanking   = 0;             /* arbitrary */
	/* pixels */
	RightBlanking  = 0;             /* arbitrary */
	/* pixels */
	TopBlanking    = 10;             /* minimum 9 */
	/* lines */
	BottomBlanking = 0;             /* minimum 9 */
	/* lines */

	msg_printf(DBG, "Loading the Decompression register values\n");

/* Initialize some image size dependent variables                         */
/* These formulas are simply examples. Refer to the databook for          */
/*    restrictions                                                        */

	CL560HPeriod  = CL560ImageWidth + LeftBlanking + RightBlanking;
	CL560HDelay   = LeftBlanking;                            /* pixels */
	CL560HSync   = CL560ImageWidth - 1;/* non arbitrary */
	/* pixels */
	CL560HActive = (CL560ImageWidth / 8);                   /* blocks */

	CL560VPeriod = CL560ImageHeight + TopBlanking + BottomBlanking;
	CL560VDelay  = TopBlanking;                     /* lines */
	CL560VSync   = 2;
	CL560VActive = (CL560ImageHeight / 8);                  /* lines */

	/* Load Window Registers */

	regWrite(_HPeriod,calculateHPeriod(CL560HPeriod),0);
	regWrite(_HSync,  calculateHSync(CL560HSync,CL560HPeriod),0);
	regWrite(_HDelay, calculateHDelay(CL560HDelay),0);
	regWrite(_HActive,calculateHActive(CL560HActive),0);
	if ( CL560HDelay == 0 ) {
		regWrite(_VSync, CL560VSync+1, 0);
	} else {
		regWrite(_VSync, CL560VSync, 0);
	}
	regWrite(_VPeriod,calculateVPeriod(CL560VPeriod),0);
	regWrite(_VDelay,
		calculateVDelay(CL560VDelay,CL560HActive,CL560HPeriod),0);
	regWrite(_VActive,calculateVActive(CL560VActive),0);

	regWrite(_HControl, calculateHControl(CL560HActive),0);
	regWrite(_VControl,
		calculateVControl(CL560HActive,CL560VActive, CL560HPeriod),0);

	/* Shut offffff DMA */
	regWrite(_DRQ_Enable,0xffff,0);
	regWrite(_DRQ_Enable,0,0);
	regWrite(_IRQ1_Mask,_Int_VInactive,0);

	/* Load DCT coefficients */

	regWrite(_DCT,0x00005A82L,0);
	regWrite(_DCT,0x00007FFFL,1);
	regWrite(_DCT,0x000030FCL,2);
	regWrite(_DCT,0x00007642L,3);
	regWrite(_DCT,0x00005A82L,4);
	regWrite(_DCT,0x00007FFFL,5);
	regWrite(_DCT,0x000030FCL,6);
	regWrite(_DCT,0x00007642L,7);

	/* Load RGB-YUV conversion matrix */

	regWrite(_R2Y,0x00000400L,0);
	regWrite(_R2U,0x00000400L,0);
	regWrite(_R2V,0x00000400L,0);
	regWrite(_G2Y,0x00000FFEL,0);
	regWrite(_G2U,0x00000EA2L,0);
	regWrite(_G2V,0x0000071BL,0);
	regWrite(_B2Y,0x0000059CL,0);
	regWrite(_B2U,0x00000D23L,0);
	regWrite(_B2V,0x00000FFDL,0);

	/* Load Configuration Registers */

	regWrite(_Config,calculateConfig(),0);

	regWrite(_IRQ2_Mask,0L,0);

	regWrite(_Init_1,       D_InitRegValues[Init_1][MODE_PIXEL],        0);
	regWrite(_Init_2,       D_InitRegValues[Init_2][MODE_PIXEL],        0);
	regWrite(_Init_3,       D_InitRegValues[Init_3][MODE_PIXEL],        0);
	regWrite(_Init_4,       D_InitRegValues[Init_4][MODE_PIXEL],        0);
	regWrite(_Init_5,       D_InitRegValues[Init_5][MODE_PIXEL],        0);
	regWrite(_Init_6,       D_InitRegValues[Init_6][MODE_PIXEL],        0);
	regWrite(_Init_7,       D_InitRegValues[Init_7][MODE_PIXEL],        0);
	regWrite(_QuantSync,    D_InitRegValues[QuantSync][MODE_PIXEL],     0);
	regWrite(_QuantYC,      D_InitRegValues[QuantYC][MODE_PIXEL],       0);
	regWrite(_QuantAB,      D_InitRegValues[QuantAB][MODE_PIXEL],       0);
	regWrite(_VideoLatency, D_InitRegValues[VideoLatency][MODE_PIXEL],  0);

	regWrite(_CodecSeq,     ConfigRegValues[CodecSeq][MODE_PIXEL],      0);
	regWrite(_CodecCompH,   ConfigRegValues[CodecCompH][MODE_PIXEL],    0);
	regWrite(_CodecCompL,   ConfigRegValues[CodecCompL][MODE_PIXEL],    0);
	regWrite(_CoderAttr,    ConfigRegValues[CoderAttr][MODE_PIXEL],     0);
	regWrite(_CodingIntH,   ConfigRegValues[CodingIntH][MODE_PIXEL],    0);
	regWrite(_CodingIntL,   ConfigRegValues[CodingIntL][MODE_PIXEL],    0);
	regWrite(_CoderSync,    ConfigRegValues[cl560CoderSync][MODE_PIXEL],0);
	regWrite(_DecLength,    ConfigRegValues[DecLength][MODE_PIXEL],     0);
	regWrite(_CodeOrder,    ConfigRegValues[CodeOrder][MODE_PIXEL],     0);
	regWrite(_QuantSelect,  ConfigRegValues[QuantSelect][MODE_PIXEL],   0);

	regWrite(_DecDPCM,  1,   0);

}

/******************************************************************************/
/*                                                                            */
/*  D_DoImage() performs the decompression of the image.Written for the CL560 */
/*     development board.                                                     */
/*                                                                            */
/******************************************************************************/

#define D_CODESIZE	0x2a0*8

void D_DoImage(int n)
{
	char *self = "D_DoImage";
	unsigned long Data;
	unsigned long *PixPtr;
	unsigned long PixelTotal;
	unsigned long count=0L;
	unsigned short *sPixPtr;
	short done,i;
	volatile VFC_REGS *vfc_regs = cosmo_regs.jpeg_vfc;
	unsigned status, start_560_timeout;
	int icount = 0;
	int linecount;
	short dev, val;
	unsigned no_560_activity = 0;
	unsigned last_code_count = 0;

	regRead(_DecoderMismatch,&Data,0);
	if ( Data & _Decoder_Mismatch_Occurred ) {
		regWrite(_DecoderMismatch,0,0);
	}

	/* set up our variables */

	CL560CompressedDataPtr = (unsigned long *)CompressedDataPtr;
	CL560CodeCount = (D_CODESIZE+3)/4;

	PixPtr = (unsigned long *)(ImageDataPtr);
	PixelTotal = CL560HActive*8L*CL560VActive*8L;

	/* Prefill the FIFO so the chip will have data to work on */

	prefilling = 1;
	__FillFIFO();
	prefilling = 0;

	regWrite(_Flags,-1L,0);            /* 1.0a Modification:     */
	regWrite(_Flags,0L,0);             /*       clear late bit   */


	regWrite(_IRQ2_Mask,_Int_FIFO_NotHalfFull,0);
	regWrite(_DRQ_Enable,_Flags_FIFO_Not3QFull,0);
	regRead(_Flags,&Data,0);


	vfc_regs->vfc_intr_mask = ~(DECMP_EOF|WRITE_ADV);
	vfc_regs->vfc_intr = 0;

	/* let the chip start on the next VSYNC */

	regWrite(_HV_Enable,1L,0);
	regWrite(_Start,-1L,0);

	regRead(_Flags,&Data,0);
	msg_printf(DBG, "%s: attempt to start CL560 (flags==0x%x)\n",
								self, Data);
	start_560_timeout = 0x30000;
	/* wait for 560 to start */
	if ( !(Data & _Flags_VSync) ) {
		do_noReportlevel();
		do {
			regRead(_Flags,&Data,0);
		} while ( !(Data & _Flags_VSync) && --start_560_timeout );
		do_Reportlevel();
		if ( start_560_timeout == 0 ) {
			msg_printf(DBG, "%s: 560 not starting ( 0x%x)\n",
								self, Data);
			cosmo_decomp_error++;
			return;
		}
	} else {
		msg_printf(DBG, "%s: 560 started OK\n", self);
	}
	msg_printf(DBG,"%s: delaying\n", self);
	us_delay(0x2000);

/************************************** VLEADING ******************************/
/*                                                                            */
/*  We keep HSyncs set until the chip is in the active region. The code waits */
/*  for BLANK to negate,indicating that the chip is ready with the first pixel*/
/*                                                                            */
/******************************************************************************/

	Finished=0;
	numimages = 1;
	last_code_count = CL560CodeCount;
	msg_printf(DBG, "%s: starting __FillFIFO loop\n", self);
	while(!Finished)
	{
		__FillFIFO();
		if( vfc_regs->vfc_intr & DECMP_EOF ) {
			/* let chip stop after the image */
			msg_printf(DBG,"%s: DECMP_EOF, let chip stop\n", self);
			regWrite(_HV_Enable,1L,0);
			regWrite(_Start,0L,0);
			regWrite(_IRQ2_Mask, _Flags_FIFO_QFull,0);
			Finished = 1;
			vfc_regs->vfc_intr = 0;
		}
		if ( last_code_count == CL560CodeCount ) {
		        if ( ++no_560_activity >= 20 ) {
				msg_printf(ERR, "%s: no 560 activity, abort\n",
								self);
				cosmo_decomp_error++;
				return;
			}
			us_delay(0x20);
		} else {
			last_code_count = CL560CodeCount;
			no_560_activity = 0;
		}
	}
	msg_printf(DBG, "%s: __FillFIFO loop ended\n", self);
}

cosmo_decomp_init(void)
{
	LoadHuffTabFromArray(CandD560_HuffArray);
	LoadQTabFromArray(DefaultDQ50Array1);
}

#define MAX_DECOMP_LOCAL_ERROR 20

void
do_decomp_compare(void)
{
	char *self = "do_decomp_compare";
	unsigned data1, data2, loc_error = 0;
	int i, j;
	unsigned dummy;
	volatile VFC_REGS *vfc_regs = cosmo_regs.jpeg_vfc;
	volatile long *uic_pio = cosmo_regs.jpeg_uic_pio;

	msg_printf(DBG, "%s: comparing decompressed image to known data\n",
						self);
	cosmo_set_vfc_mode((VFC_DEST_GIO|vfc32bitmode));
	vfc_regs->vfc_write = 0;
	vfc_regs->vfc_read = 1;
	vfc_regs->vfc_read_ctl = R_RST;
	dummy = *uic_pio;
	dummy = *uic_pio;
	vfc_regs->vfc_read_ctl = R_RST;
	for ( i=0; i<0x40000; i+=4) {
		data1 = (*uic_pio & 0xffffff);
		if ( data1 != 0x808080 ) {
			if ( ++loc_error < MAX_DECOMP_LOCAL_ERROR ) {
				cosmo_comp_print_err(self,i, 0x808080, data1);
			}
			cosmo_decomp_error++;
		}
	}
	msg_printf(DBG, "%s: got 0x%x errors\n", self, loc_error);
}

cosmo_do_decomp(void)
{
	char *self = "cosmo_do_comp";

	msg_printf(DBG, "%s: performing decompression\n", self);
	D_DoImage(1);
	if ( !cosmo_decomp_error ) {
		us_delay(0x2000);
		do_decomp_compare();
		us_delay(0x2000);
	}
	return(cosmo_decomp_error);
}

cosmo_decomp_fbsetup(void)
{
	char *self = "cosmo_decomp_fbsetup";
	volatile VFC_REGS *vfc_regs = cosmo_regs.jpeg_vfc;
	volatile long *uic_pio = cosmo_regs.jpeg_uic_pio;
	unsigned dummy;

	cosmo_set_vfc_mode(0);
	vfc_regs->vfc_pix_width = CL560ImageWidth;
	vfc_regs->vfc_pix_height = ((CL560ImageHeight+1) & 0x3ff);
	msg_printf(DBG, "%s: width==0x%x, height==0x%x\n", self,
			vfc_regs->vfc_pix_width & 0xffff,
			vfc_regs->vfc_pix_height & 0xffff);
	vfc_regs->vfc_write = 1;
	vfc_regs->vfc_write_ctl = W_RST;
	vfc_regs->vfc_write_ctl = 0;
	vfc_regs->vfc_read = 4;
	vfc_regs->vfc_read_ctl = 0;
	vfc_regs->vfc_read_ctl = R_RST;
	dummy = *uic_pio;
	dummy = *uic_pio;
	vfc_regs->vfc_read_ctl = R_RST;
	dummy = *uic_pio;
	vfc_regs->vfc_read_ctl = R_RST;
	cosmo_7186_set_rgbmode(1);
	vfc_regs->vfc_d1_config = 0;
	cosmo_set_vfc_mode((VFC_DEST_GIO|VFC_SRC_CL560|vfc32bitmode));

}
/******************************************************************************/
/*                                                                            */
/*  cosmo_decomp_Reset() - does reset for decompression                           */
/*                                                                            */
/******************************************************************************/

void cosmo_decomp_Reset(void)
{
	volatile unsigned *i2c = (unsigned *)cosmo_regs.jpeg_8584_SAA7;

	cosmo_board_reset();
#ifndef SCALED
	cosmo_init_7186();
#endif /* SCALED */
	cosmo_reset_560();
}

int
cosmo_decomp(int argc, char **argv)
{
	char *self = "cosmo_decomp";

	if ( cosmo_probe_for_it() == -1 ) {
		return(-1);
	}
	cosmo_decomp_error = 0;
	cosmo_decomp_Reset();
	D_LoadRegisters();
	regWrite(_IRQ2_Mask,0,0);
	cosmo_decomp_init();
	cosmo_init_field_buffers();
	cosmo_fill_image();
	cosmo_decomp_fbsetup();
	if ( cosmo_do_decomp() ) {
		msg_printf(SUM, "%s: test failed\n", self);
		return(cosmo_err_count);
	} else {
		msg_printf(SUM, "%s: test passed\n", self);
		return(0);
	}
	return(0);
}
