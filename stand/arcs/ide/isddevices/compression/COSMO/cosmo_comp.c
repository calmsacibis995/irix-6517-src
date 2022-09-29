#include <sys/types.h>
#include "cosmo_hw.h"
#include "cosmo_drv.h"
#include "cosmo_ide.h"
#include "cosmo_cl560.h"
#include "cosmo_huff.h"
#include "cosmo_QArray.h"
#include <libsk.h>
#include <libsc.h>

extern unsigned fb_failed[FIELD_BUFFER_COUNT];
extern unsigned mem_error_cnt;

unsigned cosmo_comp_error = 0;
unsigned short CL560HPeriod,CL560HDelay,CL560HActive,CL560HSync;
unsigned short CL560ImageWidth,CL560ImageHeight;
unsigned short CL560VPeriod,CL560VDelay,CL560VActive,CL560VSync;
unsigned short MODE_PIXEL = _RGB_422_;
unsigned short MODE_DECOMPRESS = 0;
int havegoodsPixPtr = 0;
unsigned short *sPixPtr;
unsigned long *CL560CompressedDataPtr = NULL;
unsigned long CL560CodeSize;
Ptr CompressedDataPtr = NULL, ImageDataPtr = NULL;
#ifndef notdef
int PIXELS = 640, LINES = 248;
#else /* notdef */
int PIXELS = 320, LINES = 124;
#endif /* notdef */
int   vfc32bitmode = 1;		/* RBG is 32 bit mode */

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
unsigned long C_InitRegValues[_NumInitReg][_NumPixelMode] =

/*            444,   RGB_444
/* Name          BYPASS,  422,   1T_444, 2T_444, 3T_444,  4444,  RGB_422,444_422 */

/*Init_1      */{
	0x0002L,0x0042L,0x0040L,0x0040L,0x0040L,0x0042L,0x003CL,0x0042L,
	/*Init_2      */ 0x0001L,0x0001L,0x0000L,0x0000L,0x0000L,0x0001L,0x003DL,0x0001L,
	/*Init_3      */ 0x0141L,0x0081L,0x007FL,0x007FL,0x007FL,0x0081L,0x007BL,0x0081L,
	/*Init_4      */ 0x00F7L,0x00F7L,0x00F5L,0x00F5L,0x00F5L,0x00F7L,0x00F1L,0x00F7L,
	/*Init_5      */ 0x0000L,0x0000L,0x000EL,0x000EL,0x000EL,0x0000L,0x000AL,0x0000L,
	/*Init_6      */ 0x0000L,0x0000L,0x0000L,0x0000L,0x0000L,0x0000L,0x0000L,0x0000L,
	/*Init_7      */ 0x005DL,0x001DL,0x001BL,0x001BL,0x001BL,0x001DL,0x0017L,0x001DL,
	/*QuantSync   */ 0x0406L,0x0406L,0x0404L,0x0404L,0x0404L,0x0406L,0x0400L,0x0406L,       
	/*QuantYC     */ 0x2000L,0x2099L,0x1800L,0x20CCL,0x2044L,0x2055L,0x2099L,0x2099L,       
	/*QuantAB     */ 0x0000L,0x0000L,0x0000L,0x0000L,0x0088L,0x0099L,0x0000L,0x0000L,      
	/*VideoLatency*/ 0x00BFL,0x017FL,0x0181L,0x0181L,0x0181L,0x017FL,0x0185L,0x017FL};

/******************************************************************************/
/*                                                                            */
/* Config register values - ConfigRegValues[reg name][mode]                   */
/*   The Configuration registers have the same values for comp and decomp or, */
/*   as in the case of DecLength, CodeOrder, and DecMarker, are not used by   */
/*   both comp/decomp. These values are also assigned in the C_LoadRegister   */
/*   and D_LoadRegister routines.                                             */
/*                                                                            */
/******************************************************************************/

unsigned long ConfigRegValues[_RegName][_NumPixelMode] =

/* 444    RGB_444    */
/* Name          BYPASS,  422,   1T_444, 2T_444, 3T_444, 4444,   RGB_422,444_422 */

/*Config      */{
	0x0080L,0x0010L,0x0040L,0x0040L,0x0040L,0x0060L,0x0030L,0x0020L,
	/*CodecSeq    */ 0x0000L,0x00CCL,0x0000L,0x0036L,0x0000L,0x0000L,0x00CCL,0x00CCL,       
	/*CodecCompH  */ 0x0000L,0x0088L,0x0024L,0x0024L,0x0024L,0x00CCL,0x0088L,0x0088L,
	/*CodecCompL  */ 0x0000L,0x0044L,0x0012L,0x0012L,0x0012L,0x00AAL,0x0044L,0x0044L,
	/*CoderAttr   */ 0x0001L,0x0004L,0x0003L,0x0003L,0x0003L,0x0004L,0x0004L,0x0004L,
	/*CodingIntH  */ 0x0000L,0x0000L,0x0000L,0x0000L,0x0000L,0x0000L,0x0000L,0x0000L,
	/*CodingIntL  */ 0x0001L,0x0001L,0x0001L,0x0001L,0x0001L,0x0001L,0x0008L,0x0001L,
	/*DecLength   */ 0x0008L,0x0008L,0x0006L,0x0006L,0x0006L,0x0008L,0x0008L,0x0008L,
	/*CodeOrder   */ 0x0001L,0x0001L,0x0001L,0x0001L,0x0001L,0x0001L,0x0001L,0x0001L,
	/*QuantSelect */ 0x0001L,0x0001L,0x0001L,0x0001L,0x0001L,0x0001L,0x0001L,0x0001L,
	/*DCT         */ 0x0000L,0x0000L,0x000EL,0x000EL,0x000EL,0x0000L,0x000AL,0x0000L,
	/*CoderSync  */  0x0100L,0x01C0L,0x01C2L,0x01C2L,0x01C2L,0x01C0L,0x01C6L,0x01C0L};

unsigned long D_InitRegValues[_NumInitReg][_NumPixelMode] =

/* 444                     RGB_444
/* Name          BYPASS,  422,   1T_444, 2T_444, 3T_444,  4444,  RGB_422,444_422    */

/*Init_1      */{
	0x003EL,0x003EL,0x003EL,0x003EL,0x003EL,0x003EL,0x003EL,0x003EL,
	/*Init_2      */ 0x0037L,0x0037L,0x0037L,0x0037L,0x0037L,0x0037L,0x0037L,0x0037L,
	/*Init_3      */ 0x01FFL,0x01FFL,0x01FFL,0x01FFL,0x01FFL,0x01FFL,0x01FFL,0x01FFL,
	/*Init_4      */ 0x0049L,0x0049L,0x0049L,0x0049L,0x0049L,0x0049L,0x0049L,0x0049L,
	/*Init_5      */ 0x0005L,0x0005L,0x0005L,0x0005L,0x0005L,0x0005L,0x0005L,0x0005L,
	/*Init_6      */ 0x0000L,0x0000L,0x0000L,0x0000L,0x0000L,0x0000L,0x0000L,0x0000L,
	/*Init_7      */ 0x0022L,0x0022L,0x0022L,0x0022L,0x0022L,0x0022L,0x0022L,0x0022L,
	/*QuantSync   */ 0x043EL,0x043EL,0x043EL,0x043EL,0x043EL,0x043EL,0x043EL,0x043EL,       
	/*QuantYC     */ 0x2000L,0x2033L,0x1800L,0x2099L,0x2088L,0x20AAL,0x2033L,0x2033L,   
	/*QuantAB     */ 0x0000L,0x0000L,0x0000L,0x0000L,0x0011L,0x0033L,0x0000L,0x0000L,      
	/*VideoLatency*/ 0x00BFL,0x017FL,0x017FL,0x017FL,0x017FL,0x017FL,0x017FL,0x017FL};

/******************************************************************************/
/*                                                                            */
/*  regRead(Reg,Data,Offset) first reads from the shadow memory, if the board */
/*    is not in use then control is returned to the calling routine. This     */
/*    routine makes the decisions weather to byte swap or not to byte swap.   */
/*    The registers that do not get swapped are the board registers and the   */
/*    CODEC register.                                                         */
/******************************************************************************/

void
regRead(short Reg,unsigned long *Data, short Offset)
{
	char *self = "regRead";
	unsigned long *pointer;

	pointer = (unsigned long *)((unsigned long)cosmo_regs.jpeg_cl560 +
		  	(unsigned long)CL560Register[Reg].address + 
	    		(((unsigned long)(Offset)) << 2));

	*Data = *pointer;
	msg_printf(VDBG,"%s: (0x%x,0x%x,0x%x,0x%x) %4x Data:%4x (%d)\n",
	    self, cosmo_regs.jpeg_cl560, CL560Register[Reg].address,
	    Reg, Offset, pointer, *Data&0xffff, *Data);
	return;                           /* then do not byte swap.           */
}

void
regWrite(short Reg,unsigned long Data, short Offset)
{
	char *self = "regWrite";
	unsigned long *pointer;

	pointer = (unsigned long *)((unsigned long)cosmo_regs.jpeg_cl560 +
	    (unsigned long)CL560Register[Reg].address + 
	    (((unsigned long)(Offset)) << 2));
	msg_printf(VDBG,"%s: (0x%x,0x%x,0x%x,0x%x) %4x Data:%4x (%d)\n",
	    self, cosmo_regs.jpeg_cl560, CL560Register[Reg].address,
	    Reg, Offset, pointer, Data&0xffff, Data);
	*pointer = Data;
	return; 
}

__psint_t save_Reportlevel = -1;
unsigned use_Reportlevel = 0;

void
do_noReportlevel(void)
{
	char *self = "do_noReportlevel";

	msg_printf(VDBG, "%s (0x%x,0x%x)\n",
			self, save_Reportlevel, Reportlevel);
	if ( ++use_Reportlevel == 1 ) {
		save_Reportlevel = *Reportlevel;
		Reportlevel = 0;
	}
}

void
do_Reportlevel(void)
{
	char *self = "do_Reportlevel";

	if ( --use_Reportlevel == 0 ) {
		*Reportlevel = save_Reportlevel;
		save_Reportlevel = -1;
		msg_printf(VDBG, "%s (0x%x,0x%x)\n",
			self, save_Reportlevel, *Reportlevel);
	}
}

/*****************************************************************************/
/*                                                                           */
/*   Subroutine DrainFIFO() drains the FIFO until _Flags_FIFO_QFull is no */
/*   longer true.                                                            */
/*                                                                           */
/*****************************************************************************/
short FIFO_QFull=0;

void DrainFIFO(void)
{
	char *self = "DrainFIFO";
	unsigned int d0,d1,d2,d3;
	unsigned long Data, Data2;
	unsigned long pointer;
	int i;
	volatile unsigned long *codecAddress;

	pointer = (unsigned long)cosmo_regs.jpeg_cl560 +
	    (unsigned long)CL560Register[_Codec].address;
	codecAddress = (unsigned long *)(pointer);
	msg_printf(DBG, "%s: called (0x%x)\n", self, codecAddress);

	FIFO_QFull = 1;      /* 1.1a Modification: FIFO_QFull flag reports   */
	/*      that the FIFO has reached quarter full  */
	/*      since that chip won't start until       */
	/*      quarter full is reached once.           */
	do_noReportlevel();
	do {
		regRead(_Flags,&Data,0);
		regRead(_Flags,&Data2,0);
	} while((Data&0xffff) != (Data2&0xffff));
	do_Reportlevel();

	while(Data & _Flags_FIFO_QFull) {
		if(Data & _Flags_FIFO_3QFull) {
			CL560CodeSize += 92*4;
			for(i=0; i<23; i++) {
				d0 = *codecAddress;
				d1 = *codecAddress;
				d2 = *codecAddress;
				d3 = *codecAddress;
				CL560CompressedDataPtr[0] = d0;
				CL560CompressedDataPtr[1] = d1;
				CL560CompressedDataPtr[2] = d2;
				CL560CompressedDataPtr[3] = d3;
				CL560CompressedDataPtr+= 4;
			}
		} else if(Data & _Flags_FIFO_HalfFull) {
			CL560CodeSize += 60*4;
			for(i=0; i<15; i++) {
				d0 = *codecAddress;
				d1 = *codecAddress;
				d2 = *codecAddress;
				d3 = *codecAddress;
				CL560CompressedDataPtr[0] = d0;
				CL560CompressedDataPtr[1] = d1;
				CL560CompressedDataPtr[2] = d2;
				CL560CompressedDataPtr[3] = d3;
				CL560CompressedDataPtr+= 4;
			}
		} else {
			CL560CodeSize += 28*4;
			for(i=0; i<7; i++) {
				d0 = *codecAddress;
				d1 = *codecAddress;
				d2 = *codecAddress;
				d3 = *codecAddress;
				CL560CompressedDataPtr[0] = d0;
				CL560CompressedDataPtr[1] = d1;
				CL560CompressedDataPtr[2] = d2;
				CL560CompressedDataPtr[3] = d3;
				CL560CompressedDataPtr+= 4;
			}
		}
		do_noReportlevel();
		do {
			regRead(_Flags,&Data,0);
			regRead(_Flags,&Data2,0);
		} while((Data&0xffff) != (Data2&0xffff));
		do_Reportlevel();
	}
	msg_printf(DBG, "%s: exitting (0x%x,0x%x,0x%x)\n", self,
				Data, Data2,CL560CodeSize);
}

void LastDrainFIFO(void)
{
	char *self = "LastDrainFIFO";
	unsigned long Data, Data2;
	int i;
	unsigned long pointer;
	volatile unsigned long *codecAddress;


	/* Note: FIFO cannot be drained unless 1/4 full has been reached */
	pointer = (unsigned long)cosmo_regs.jpeg_cl560 +
	    (unsigned long)CL560Register[_Codec].address;
	codecAddress = (unsigned long *)(pointer);

	do_noReportlevel();
	regRead(_Flags,&Data,0);
	do_Reportlevel();
	msg_printf(DBG, "%s: called (0x%x,0x%x)\n", self, codecAddress, Data);
	if(!((Data & _Flags_FIFO_QFull) || FIFO_QFull))
	{
		printf("        NOT QUARTER FULL YET!\n");
		return;
	}


	do_noReportlevel();
	do {
		regRead(_Flags,&Data,0);
		regRead(_Flags,&Data2,0);
	} while((Data&0xffff) != (Data2&0xffff));
	do_Reportlevel();
	while(Data & _Flags_FIFO_NotEmpty)
	{
		Data = *codecAddress;
		CL560CodeSize += 4;
		*CL560CompressedDataPtr++ = Data;
		if(Data==0xFFFFFFFF)            /* A value of FFFFFFFF means we   */
			return;                     /*  have emptied the          */
		do_noReportlevel();
		do {
			regRead(_Flags,&Data,0);
			regRead(_Flags,&Data2,0);
		} while((Data&0xffff) != (Data2&0xffff));
		do_Reportlevel();
	}
	msg_printf(DBG, "  Flags:%.8lx (0x%x)\n",Data, CL560CodeSize);
}
/******************************************************************************/
/*                                                                            */
/*  C_LoadRegisters() loads the appropriate values into the chip registers for*/
/*    compression.                                                            */
/*                                                                            */
/******************************************************************************/

void C_LoadRegisters(void)
{
	unsigned long  LeftBlanking, RightBlanking;
	unsigned long  TopBlanking, BottomBlanking;

	MODE_DECOMPRESS = 0;
	CL560ImageWidth = PIXELS;
	CL560ImageHeight = LINES;
	LeftBlanking   = 0;             /* arbitrary */
	/* pixels */
	RightBlanking  = 1;             /* arbitrary */
	/* pixels */
	TopBlanking    = 1;             /* minimum 9 */
	/* lines */
	BottomBlanking = 9;             /* minimum 9 */
	/* lines */

/* Initialize some image size dependent variables                         */
/* These formulas are simply examples. Refer to the databook for          */
/*    restrictions                                                        */

	CL560HPeriod  = 8 + CL560ImageWidth + LeftBlanking + RightBlanking;
	CL560HDelay   = LeftBlanking;                            /* pixels */
	CL560HSync   = 2;
	CL560HActive = (CL560ImageWidth / 8);                   /* blocks */

	CL560VPeriod = CL560ImageHeight + TopBlanking + BottomBlanking;
	CL560VDelay  = TopBlanking;                             /* lines */
	CL560VSync   = 4;      /* arbitrary */
	/* lines */
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
	regWrite(_VDelay, calculateVDelay(CL560VDelay,
					CL560HActive,CL560HPeriod),0);
	regWrite(_VActive,calculateVActive(CL560VActive),0);

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

	regWrite(_R2Y,0x00000133L,0);
	regWrite(_G2Y,0x00000259L,0);
	regWrite(_B2Y,0x00000074L,0);
	regWrite(_R2U,0x00000F54L,0);
	regWrite(_G2U,0x00000EADL,0);
	regWrite(_B2U,0x000001FFL,0);
	regWrite(_R2V,0x000001FFL,0);
	regWrite(_G2V,0x00000E53L,0);
	regWrite(_B2V,0x00000FAEL,0);

	/* Load Configuration Registers */

	regWrite(_Config,calculateConfig(),0);

	regWrite(_IRQ2_Mask,0L,0);

	regWrite(_Init_1,C_InitRegValues[Init_1][MODE_PIXEL],0);
	regWrite(_Init_2,C_InitRegValues[Init_2][MODE_PIXEL],0);
	regWrite(_Init_3,C_InitRegValues[Init_3][MODE_PIXEL],0);
	regWrite(_Init_4,C_InitRegValues[Init_4][MODE_PIXEL],0);
	regWrite(_Init_5,C_InitRegValues[Init_5][MODE_PIXEL],0);
	regWrite(_Init_6,C_InitRegValues[Init_6][MODE_PIXEL],0);
	regWrite(_Init_7,C_InitRegValues[Init_7][MODE_PIXEL],0);
	regWrite(_QuantSync,C_InitRegValues[QuantSync][MODE_PIXEL],0);
	regWrite(_QuantYC,C_InitRegValues[QuantYC][MODE_PIXEL],0);
	regWrite(_QuantAB,C_InitRegValues[QuantAB][MODE_PIXEL],0);
	regWrite(_VideoLatency,C_InitRegValues[VideoLatency][MODE_PIXEL],0);

	regWrite(_CodecSeq,ConfigRegValues[CodecSeq][MODE_PIXEL],0);
	regWrite(_CodecCompH,ConfigRegValues[CodecCompH][MODE_PIXEL],0);
	regWrite(_CodecCompL,ConfigRegValues[CodecCompL][MODE_PIXEL],0);
	regWrite(_CoderAttr,ConfigRegValues[CoderAttr][MODE_PIXEL],0);
	regWrite(_CodingIntH,ConfigRegValues[CodingIntH][MODE_PIXEL],0);
	regWrite(_CodingIntL,ConfigRegValues[CodingIntL][MODE_PIXEL],0);
	regWrite(_CoderSync,ConfigRegValues[cl560CoderSync][MODE_PIXEL],0);
	regWrite(_DecLength,ConfigRegValues[DecLength][MODE_PIXEL],0);
	regWrite(_CodeOrder,ConfigRegValues[CodeOrder][MODE_PIXEL],0);
	regWrite(_QuantSelect,ConfigRegValues[QuantSelect][MODE_PIXEL],0);

#ifdef notdef
	regWrite(_CoderRCEnable,16,0);	/*full*/
	regWrite(_CoderRCEnable,8,0);	/*7/8full*/
	regWrite(_CoderRCEnable,4,0);	/*3/4full*/
	regWrite(_CoderRCEnable,2,0);	/*1/2full*/
	regWrite(_CoderRCEnable,0,0);	/*Noratecontrol*/
#endif /* notdef */
}

/******************************************************************************/
/*                                                                            */
/*  ZeroHuffman() initializes the Huffman tables to zeros.                    */
/*                                                                            */
/******************************************************************************/

void ZeroHuffman(void)
{
	short i,temp;
	char *self = "ZeroHuffman";


	msg_printf(DBG, "%s: zero huffman table\n", self);
	regWrite(_Huff_Enable,-1L,0);

	do_noReportlevel();
	for(i=0;i<32;i++)
		regWrite(_HuffYDC,0L,i);
	for(i=0;i<512;i++)
		regWrite(_HuffYAC,0L,i);
	for(i=0;i<32;i++)
		regWrite(_HuffCDC,0L,i);
	for(i=0;i<512;i++)
		regWrite(_HuffCAC,0L,i);
	do_Reportlevel();

	regWrite(_Huff_Enable,0L,0);
}

/******************************************************************************/
/*                                                                            */
/*  LoadHuffTabFromArray() loads a Huffman table array onto the chip.         */
/*    The tables may be variable in length. The Huffman tables are loaded     */
/*    into memory and into a shadow memory ( same offsets with a different    */
/*    baseaddress ), the entries are entered into the locations as unsigned   */
/*    long integers.                                                          */
/*                                                                            */
/******************************************************************************/

void LoadHuffTabFromArray(unsigned short HuffArray[])
{
	char *self = "LoadHuffTabFromArray";
	unsigned short HuffTable;
	unsigned long code;
	unsigned long data;
	short num,i=0,j, huffCount;

/******************************************************************************/
/*                                                                            */
/*  Huffman tables MUST be initialized to zero before being loaded.           */
/*                                                                            */
/******************************************************************************/

	msg_printf(DBG, "%s: called (0x%x)\n", self, &HuffArray);
	ZeroHuffman();

	regWrite(_Huff_Enable,-1L,0);


	for (j=0;j<4;j++)
	{
		switch(j)
		{
		case 0: 
			HuffTable=_HuffYDC;
			msg_printf(DBG, "Loading YDC Huffman Table from array\n");
			break;
		case 1: 
			HuffTable=_HuffCDC;
			msg_printf(DBG, "Loading CDC Huffman Table from array\n");
			break;
		case 2: 
			HuffTable=_HuffYAC;
			msg_printf(DBG, "Loading YAC Huffman Table from array\n");
			break;
		case 3: 
			HuffTable=_HuffCAC;
			msg_printf(DBG, "Loading CAC Huffman Table from array\n");
			break;
		}

		do_noReportlevel();
		num=0;
		do
		{
			code = HuffArray[i];

			if(code != 0xFFF)                       /* the FFF  delimits */
			{                                       /* the 4 tables */
				regWrite(HuffTable,code,num);
#ifdef CHECKING
				regRead(HuffTable,&data,num);
				if(code != (data&0xffff)) printf("wrote a %x, read a %x at offset: %d\n", code, data, num);
#endif
				num++;
				i++;
			}
		}        while(code != 0xFFF);

		i++;
		huffCount += num;
		do_Reportlevel();
	}
	regWrite(_Huff_Enable,0L,0);

}

/******************************************************************************/
/*                                                                            */
/*  LoadQTabFromArray() loads 4 Q tables from array into the chip.         */
/*                                                                            */
/*    Depending on the pixel mode, fewer tables may be loaded. Tables may     */
/*    also be identical. The Q tables entries are calculated and loaded into  */
/*    two arrays (compQTable[] and decompQtable[]) in the MakeQTables routine.*/
/*    Each of these arrays contain the data for the two tables, if four are   */
/*    requested the code loads the same data as the first and second tables   */
/*    into the third and fourth tables.                                       */
/*                                                                            */
/*    The tables are both 128 entries in size, therefore there is no need for */
/*    delimitters in the array.                                               */
/*                                                                            */
/******************************************************************************/

void LoadQTabFromArray(unsigned short QArrayName[])
{
	unsigned long code;
	unsigned short num;

	msg_printf(DBG, "Loading Q Tables into CL560 from array\n");

	regWrite(_QuantSelect,0x00000000L,0); /* select RAM B to load RAM A */
	/* (the selected RAM cannot be loaded) */

	do_noReportlevel();
	for(num=0;num<64;num++)               /* load luminance quantizer table */
	{
		code = QArrayName[num];
		regWrite(_Quant,code,num);
	}

	for(num=64;num<128;num++)             /* load chrominance quantizer table */
	{
		code = QArrayName[num];
		regWrite(_Quant,code,num);
	}

	/* we duplicate tables in 3rd and  */
	/*     4th tables                  */

	for(num=0;num<64;num++)                /* load 3rd quantizer table        */
	{
		code = QArrayName[num];
		regWrite(_Quant,code,128+num);
	}

	for(num=64;num<128;num++)               /* load 4th quantizer table       */
	{
		code = QArrayName[num];
		regWrite(_Quant,code,128+num);
	}
	do_Reportlevel();

	regWrite(_QuantSelect,0x00000001L,0);   /* select RAM A                   */

}

cosmo_comp_init(void)
{
	LoadHuffTabFromArray(CandD560_HuffArray);
	LoadQTabFromArray(DefaultCQ50Array1);
}

cosmo_fill_fb(unsigned mem_data)
{
	char *self = "cosmo_fill_fb";
	volatile VFC_REGS *vfc_regs = cosmo_regs.jpeg_vfc;
	volatile long *uic_pio = cosmo_regs.jpeg_uic_pio;
	int i, j;
	unsigned dummy;

	cosmo_set_vfc_mode((VFC_SRC_GIO|vfc32bitmode));
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
	cosmo_set_vfc_mode(0);
	vfc_regs->vfc_write = 0;
	vfc_regs->vfc_write_ctl = 0;
	vfc_regs->vfc_read = 8;
	vfc_regs->vfc_read_ctl = 0;
	cosmo_set_vfc_mode((VFC_DEST_CL560|VFC_SRC_GIO|vfc32bitmode));

}
/******************************************************************************/
/*                                                                            */
/*  C_DoImage() performs the compression of the image. Written for the        */
/*      CL550 development board.                                              */
/*                                                                            */
/******************************************************************************/


int
C_DoImage(int numfields)
{
	char * self = "C_DoImage";
	unsigned long Data;
	unsigned long *PixPtr;
	static unsigned short *sp, inited = 0;
	unsigned short *badptr;
	int i, j, k;
	unsigned short junk1, junk2;
	long PixelTotal;
	unsigned long count=0L;
	short done;
	unsigned int tstamp = 0;
	volatile long  *dma_ctrl = cosmo_regs.jpeg_dma_ctrl;
	volatile unsigned *fen_reg = (unsigned *)
			((unsigned )cosmo_regs.jpeg_cl560 +
			  (unsigned long)CL560Register[_FRMEND_Enable].address);
	volatile long *uic_pio = cosmo_regs.jpeg_uic_pio;
	volatile VFC_REGS *vfc_regs= cosmo_regs.jpeg_vfc;
	volatile long *ts_reg = cosmo_regs.jpeg_timestamp;
	unsigned no_560_activity = 0;
	unsigned last_code_count = 0;


	msg_printf(DBG, "%s: compressing\n", self);
	/* set up our variables */
	msg_printf(DBG, "%s: frame enable clear (0x%x)\n", self, fen_reg);
	*fen_reg = 1;		/* Enable Frame End INterrupt */
	msg_printf(DBG, "%s: CL560ImageWidth==0x%x,CL560ImageHeight==0x%x\n",
				self, CL560ImageWidth, CL560ImageHeight);
	if ((CompressedDataPtr == NULL) && 
		((CompressedDataPtr = (Ptr) malloc((size_t)_C_Data_size))
							== NULL)) {
		printf("no compressed data memory\n");
		cosmo_comp_error++;
		return(-1);
	}
	if ((ImageDataPtr == NULL) &&
		((ImageDataPtr = (Ptr) malloc((size_t)_I_Data_size))
							== NULL)) {
		printf("no image pointer memory\n");
		cosmo_comp_error++;
		return(-1);
	}
	msg_printf(DBG, "%s: CompressedDataPtr==0x%x (0x%x),", self,
				CompressedDataPtr, _C_Data_size);
	msg_printf( DBG, " ImageDataPtr==0x%x (0x%x)\n",
					ImageDataPtr, _I_Data_size);

	CL560CompressedDataPtr = (unsigned long *)CompressedDataPtr;
	for ( i=0; i<_C_Data_size; i+= 4 ) {
		*CL560CompressedDataPtr++ = 0x11111111;
	}
	CL560CompressedDataPtr = (unsigned long *)CompressedDataPtr;
	CL560CodeSize = 0L;
	PixPtr = (unsigned long *)(ImageDataPtr);
	if(!inited) {
		sPixPtr = (unsigned short *)malloc(640*248*sizeof(short));
		inited++;
	}
	PixelTotal = CL560HActive*8L*CL560VActive*8L;
	sp = sPixPtr;

/******************************* VLEADING ************************************/
/*                                                                           */
/*   The HSyncs signal is set until the chip is in the active region         */
/*   (Board_Status = Status_BLANK). The code waits for BLANK to negate the   */
/*   HSYNC, indicating that the chip is waiting for the first pixel.         */
/*                                                                           */
/*   Note: Could also use vinac here if external Status_BLANK not available  */
/*****************************************************************************/

	done=0;
	while(!done)
	{
		/* if we are in slave mode, we must toggle the HSYNC line */
		regRead(_Flags,&Data,0);
		msg_printf(DBG, "%s: flags==0x%x\n", self, Data);
		done=1;
	}

	msg_printf(DBG, "%s: Chip ready for 1st pixel\n", self);

	/* let the chip stop after the current image */

	regWrite(_HV_Enable,1L,0);
	regWrite(_Start,0L,0);
	msg_printf(DBG, "%s: CL560 stopped\n", self);

	vfc_regs->vfc_mode = VFC_SRC_D1;
	vfc_regs->vfc_read = 0;
	vfc_regs->vfc_write_ctl = W_ADV;
	us_delay(0x20);
	vfc_regs->vfc_read = 8;
	vfc_regs->vfc_write_ctl = 0;
	vfc_regs->vfc_write = 1;
	vfc_regs->vfc_mode = VFC_DEST_CL560 | VFC_SRC_GIO | vfc32bitmode;

	regWrite(_Flags,-1L,0);                   /* 1.0a Modification:     */

	regWrite(_Flags,0L,0);                    /*       clear late bit   */



	regWrite(_IRQ2_Mask,_Flags_FIFO_QFull,0);  /* 1.1a Modification: */
	/* let the chip start on the next VSYNC */
	regWrite(_HV_Enable,1L,0);
	regWrite(_Start,-1L,0);
	vfc_regs->vfc_write_ctl = W_ADV;


	regWrite(_Flags,-1L,0);            /*       clear late bit   */
	regWrite(_Flags,0L,0);            /*       clear late bit   */
	regWrite(_CoderRobustness,0,0);
	regRead(_Flags,&Data,0);
	msg_printf(DBG, "%s: CL560 Started (0x%x)\n", self, Data);
	for(i=0; i<numfields; i++) {
		regRead(_Flags,&Data,0);
		while(Data & _Flags_VInactive) { /* wait for 560 to start */
			DrainFIFO();
			regRead(_Flags,&Data,0);
			if(Data & _Flags_FIFO_Late) {
				msg_printf(DBG, "\nL:0x%x \n",Data);
				regWrite(_Flags,-1L,0);     /*       clear late bit   */
				regWrite(_Flags,0L,0);      /*       clear late bit   */
			}
			if ( last_code_count == CL560CodeSize ) {
				if ( ++no_560_activity >= 20 ) {
					msg_printf(ERR, "%s: no 560 activity, abort\n",
									self);
					cosmo_comp_error++;
					return 0;
				}
				us_delay(0x20);
			} else {
				last_code_count = CL560CodeSize;
				no_560_activity = 0;
			}
		}
		if(i == numfields-1)
			regWrite(_Start,0L,0);
		while(!(Data & _Flags_VInactive)) {
			DrainFIFO();
			regRead(_Flags,&Data,0);
			if(Data & _Flags_FIFO_Late) {
				msg_printf(DBG, "\nL:0x%x \n",Data);
				regWrite(_Flags,-1L,0);     /*       clear late bit   */
				regWrite(_Flags,0L,0);      /*       clear late bit   */
			}
			if ( last_code_count == CL560CodeSize ) {
				if ( ++no_560_activity >= 20 ) {
					msg_printf(ERR, "%s: no 560 activity, abort\n",
									self);
					cosmo_comp_error++;
					return 0;
				}
				us_delay(0x20);
			} else {
				last_code_count = CL560CodeSize;
				no_560_activity = 0;
			}
		}
		if (i==0) {
			cosmo_dma_reset(FIFO_RST_N|CL560_RST_N|IIC_RST_N);
			cosmo_dma_reset(TIMESTAMP_RST_N|FIFO_RST_N|
						CL560_RST_N|IIC_RST_N);
		}

		tstamp = *ts_reg;
		msg_printf(DBG, "Time stamp for Field %d: %d\n", i, tstamp);
	}
	LastDrainFIFO();
	regRead(_Flags,&Data,0);
	if(Data & _Flags_FIFO_Late) {
		msg_printf(DBG, "\nL:0x%x \n",Data);
		regWrite(_Flags,-1L,0);     /*       clear late bit   */
		regWrite(_Flags,0L,0);      /*       clear late bit   */
	}
	regRead(_CoderRobustness,&Data,0);
	if(Data & 1) {
		msg_printf(DBG, "Rate control mechanism activated!!\n");
		msg_printf(DBG, "Compressed data may be corrupted!!\n");
	}
	regWrite(_IRQ2_Mask,0L,0);    /* 1.1a Modification: Turning NMRQ off */

	return 0;
}

unsigned int comp_good_data[5] = { 0x28a0028a,
				   0x0028a002,
				   0x8a0028a0,
				   0x028a0028,
				   0xa0028a00 };

#define MAX_COMP_LOCAL_ERROR 20
int
do_comp_compare(void)
{
	char *self = "do_comp_compare";
	unsigned data1, data2, loc_error = 0;
	int i, j;

	msg_printf(DBG, "%s: comparing compressed image to known data\n",
						self);
	CL560CompressedDataPtr = (unsigned long *)CompressedDataPtr;
	for ( i=0, j=0; i<CL560CodeSize; i+=4) {
		data1 = *CL560CompressedDataPtr++;
		data2 = comp_good_data[j];
		if ( data1 != data2 ) {
			if ( ++loc_error < MAX_COMP_LOCAL_ERROR ) {
				cosmo_comp_print_err(self, 
					((unsigned)CL560CompressedDataPtr-
					 (unsigned)CompressedDataPtr),
								data2, data1);
			}
			cosmo_comp_error++;
		}
		j = (++j % 5);
	}
	msg_printf(DBG, "%s: got 0x%x errors (0x%x to 0x%x)\n", self,
		loc_error, CompressedDataPtr, CL560CompressedDataPtr);

	return loc_error;
}

cosmo_do_comp(void)
{
	char *self = "cosmo_do_comp";

	msg_printf(DBG, "%s: performing compression\n", self);
	C_DoImage(1);
	if ( !cosmo_comp_error ) {
		us_delay(0x2000);
		do_comp_compare();
		us_delay(0x2000);
	}
	return(cosmo_comp_error);
}

/******************************************************************************/
/*                                                                            */
/*  cosmo_comp_Reset() - does reset for compression                           */
/*                                                                            */
/******************************************************************************/

void cosmo_comp_Reset(void)
{
	volatile unsigned *i2c = (unsigned *)cosmo_regs.jpeg_8584_SAA7;

	cosmo_board_reset();
	cosmo_reset_560();
}

cosmo_comp(int argc, char **argv)
{
	char *self = "cosmo_comp";

	if ( cosmo_probe_for_it() == -1 ) {
		return(-1);
	}
	cosmo_comp_error = 0;
	cosmo_comp_Reset();
	C_LoadRegisters();
	regWrite(_IRQ2_Mask,0,0);
	cosmo_comp_init();
	cosmo_init_field_buffers();
	cosmo_fill_fb(0x80808080);	/* should use 0x80808080 */
	if ( cosmo_do_comp() ) {
		msg_printf(SUM, "%s: test failed\n", self);
		return(cosmo_err_count);
	} else {
		msg_printf(SUM, "%s: test passed\n", self);
		return(0);
	}
	return(0);
}

int
cosmo_comp_dma(int argc, char **argv)
{
	char *self = "cosmo_comp_dma";

	if ( cosmo_probe_for_it() == -1 ) {
		return(-1);
	}
	cosmo_board_reset();
	cosmo_dma_reset(ALL_DMA_RESETS);
	return(0);
}
/******************************************************************************/
/*                                                                            */
/*  The routines from this point on are all formulas for generating the       */
/*  register values, depending on the Pixel Mode and Master Mode.             */
/*                                                                            */
/******************************************************************************/

unsigned long calculateHPeriod(short localHPeriod)
{
	unsigned long CL550Hperiod;

	if(MODE_MSTR)
	{
		switch(MODE_PIXEL)
		{
		case _BYPASS_:
			CL550Hperiod = localHPeriod / 2 - 1;
			break;
		case _1T_444_:
		case _2T_444_:
		case _3T_444_:
		case _4444_:
			CL550Hperiod = localHPeriod * 2 - 1;
			break;
		case _RGB_422_:
		case _444_422_:
		case _422_:
			CL550Hperiod = localHPeriod - 1;
			break;
		}
	}
	else
		CL550Hperiod = (localHPeriod * 9) / 10;
	return(CL550Hperiod);
}

/******************************************************************************/

unsigned long calculateHSync(short localHSync,short localHPeriod)
{
	unsigned long CL550HSync;

	if(MODE_MSTR)
		CL550HSync = localHSync - 1;
	else
		CL550HSync = localHPeriod / 2;
	/*
    printf("CL550HSync=%d\n",CL550HSync);
*/
	return(CL550HSync);
}

/******************************************************************************/

unsigned long calculateHDelay(short localHDelay)
{
	unsigned long CL550HDelay;

	switch(MODE_PIXEL)
	{
	case _BYPASS_:
		CL550HDelay = localHDelay / 2;
		break;
	case _1T_444_:
	case _2T_444_:
	case _3T_444_:
	case _4444_:
		CL550HDelay = localHDelay * 2;
		break;
	case _RGB_422_:
	case _444_422_:
	case _422_:
		CL550HDelay = localHDelay;
		break;
	}

	/*
    printf("CL550HDelay=%d\n",CL550HDelay);
*/
	return(CL550HDelay);
}

/******************************************************************************/

unsigned long calculateHActive(short localHActive)
{
	unsigned long CL550HActive;

	switch(MODE_PIXEL)
	{
	case _BYPASS_:
		CL550HActive = localHActive - 1;
		break;
	case _1T_444_:
	case _2T_444_:
	case _3T_444_:
	case _4444_:
		CL550HActive = 4 * localHActive - 1;
		break;
	case _RGB_422_:
	case _444_422_:
	case _422_:
		CL550HActive = 2 * localHActive - 1;
		break;
	}
	/*
    printf("CL550HActive=%d\n",CL550HActive);
*/
	return(CL550HActive);
}

/******************************************************************************/

unsigned long calculateVPeriod(short localVPeriod)
{
	unsigned long CL550VPeriod;

	if(MODE_MSTR)
		CL550VPeriod = localVPeriod;
	else
		CL550VPeriod = 0;

	/*
    printf("CL550VPeriod=%d\n",CL550VPeriod);
*/
	return(CL550VPeriod);
}

/******************************************************************************/

unsigned long calculateVSync(short localVSync)
{
	unsigned long CL550VSync;

	if(MODE_MSTR)
		CL550VSync = localVSync + 1;
	else
		CL550VSync = 0;

/*
    printf("CL550VSync=%d\n",CL550VSync);
*/
return(CL550VSync);
}

/******************************************************************************/

unsigned long calculateVDelay(short localVDelay,
short localHActive,
short localHPeriod)
{
	unsigned long CL550HActive,CL550HPeriod,CL550VDelay;

	if(MODE_DECOMPRESS)
	{
		CL550HActive=2 * localHActive * 8;           /* Number of clock cycles*/
		CL550HPeriod=2 * localHPeriod;               /* Number of clock cycles*/
		/* between HSyncs*/
		CL550VDelay = localVDelay - (D_InitRegValues[VideoLatency][MODE_PIXEL]+
		    CL550HActive)/CL550HPeriod - 9;
	}
	else
		CL550VDelay = localVDelay;

	if (CL550VDelay < 0) CL550VDelay = 0;

	/*
    printf("CL550VDelay=%d\n",CL550VDelay);
*/
	return(CL550VDelay);
}

/******************************************************************************/

unsigned long calculateVActive(short localVActive)
{
	unsigned long CL550VActive;

	CL550VActive = localVActive;

	/*
    printf("CL550VActive=%d\n",CL550VActive);
*/
	return(CL550VActive);
}

/******************************************************************************/

unsigned long calculateConfig(void)
{
	unsigned long CL550Config;

	CL550Config = ConfigRegValues[Config][MODE_PIXEL] +
		      (MODE_DECOMPRESS << 8) +
		      (MODE_MSTR << 3) +
		      (MODE_INTL << 2);
	if ( MODE_DECOMPRESS == 0 ) {
		CL550Config |=  (MODE_FEND << 1);
	}


	/* XXX 
CL550Config=0x138;
    printf("CL550Config=%#x\n",CL550Config);
*/
	return(CL550Config);
}

/******************************************************************************/

unsigned long calculateHControl(short localHActive)
/*  Bug fixed 4/1: changed long to short */
{
	unsigned long CL550HActive,CL550EndX,CL550VBIU,CL550HControl;

	if(MODE_DECOMPRESS)
	{
		switch(MODE_PIXEL)
		{
		case _BYPASS_:
		case _422_:
		case _4444_:
		case _444_422_:
		case _1T_444_:
		case _2T_444_:
		case _3T_444_:
			CL550VBIU = 9;
			break;
		case _RGB_422_:
			CL550VBIU = 12;
			break;
		}

		switch(MODE_PIXEL)
		{
		case _BYPASS_:
			CL550HActive = 1 * localHActive * 8;
			break;
		case _1T_444_:
		case _2T_444_:
		case _3T_444_:
		case _4444_:
			CL550HActive = 4 * localHActive * 8;
			break;
		case _RGB_422_:
		case _444_422_:
		case _422_:
			CL550HActive = 2 * localHActive * 8;
			break;
		}
		/* Version 2.0a Modification: Add one  */
		/* to the HControl calculation        */
		CL550EndX=CL550HActive;
		CL550HControl=CL550EndX-(D_InitRegValues[VideoLatency][MODE_PIXEL] + 1 +
		    CL550VBIU)%CL550HActive;
		CL550HControl += 3;
	}
	else
		CL550HControl = 0L;
#ifdef NOTDEF
	CL550HControl = 0x3fffL;    /* Version 2.1 Modification: Since HControl  */
#endif
	/*   is not relevant for still image (video */
	/*   only) compression, ignore changes in   */
	/*   HControl and overwrite with 0x3FFF     */

	/*
    printf("CL550HControl=0x%x\n",CL550HControl);
*/
	return(CL550HControl);
}

/******************************************************************************/

unsigned long calculateVControl(short localHActive,
short localVActive,
short localHPeriod)
{
	unsigned long CL550HActive,CL550VActive,CL550HPeriod;
	unsigned long CL550EndY,CL550VBIU,CL550VControl;

	if(MODE_DECOMPRESS)
	{
		switch(MODE_PIXEL)
		{
		case _BYPASS_:
		case _422_:
		case _4444_:
		case _444_422_:
		case _1T_444_:
		case _2T_444_:
		case _3T_444_:
			CL550VBIU = 9;
			break;
		case _RGB_422_:
			CL550VBIU = 12;
			break;
		}

		switch(MODE_PIXEL)
		{
		case _BYPASS_:
			CL550HActive = 1 * localHActive * 8;
			break;
		case _1T_444_:
		case _2T_444_:
		case _3T_444_:
		case _4444_:
			CL550HActive = 4 * localHActive * 8;
			break;
		case _RGB_422_:
		case _444_422_:
		case _422_:
			CL550HActive = 2 * localHActive * 8;
			break;
		}

		switch(MODE_PIXEL)
		{
		case _BYPASS_:
			CL550HPeriod = 1 * localHPeriod;
			break;
		case _1T_444_:
		case _2T_444_:
		case _3T_444_:
		case _4444_:
			CL550HPeriod = 4 * localHPeriod;
			break;
		case _RGB_422_:
		case _444_422_:
		case _422_:
			CL550HPeriod = 2 * localHPeriod;
			break;
		}

		CL550VActive=localVActive * 8;               /* Number of lines       */


		/* Version 2.0a Modification: Add one      */
		/* to the CL550EndY and VControl          */
		/* calculation where VideoLatency is used.*/

		CL550EndY=CL550VActive + (D_InitRegValues[VideoLatency][MODE_PIXEL] + 1 +
		    CL550HActive)/CL550HPeriod - 1;
		CL550VControl=CL550EndY-(D_InitRegValues[VideoLatency][MODE_PIXEL] + 1 +
		    CL550VBIU)/CL550HActive;
	}
	else
		CL550VControl = 0L;
#ifdef NOTDEF
	CL550VControl = 0x3fffL;    /* Version 2.1 Modification: Since VControl */
#endif
	/*   is not relevant for still image (video */
	/*   only) compression, ignore changes in   */
	/*   VControl and overwrite with 0x3FFF     */

	/*
    printf("CL550VControl=0x%x\n",CL550VControl);
*/
	return(CL550VControl);
}
