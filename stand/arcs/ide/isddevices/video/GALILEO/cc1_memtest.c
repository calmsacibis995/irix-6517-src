#include "sys/types.h"
#include "sys/sbd.h"
#include "sys/sema.h"
#include "sys/gfx.h"
#include "sys/gr2.h"
#include "sys/gr2hw.h"
#include "sys/gr2_if.h"
#include "sys/ng1hw.h"
#include "ide_msg.h"
#include "sys/param.h"
#include "regio.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"

extern struct gr2_hw  *base ;
extern ev1_initialize();

static unsigned int WalkOne[] = {
	0xFFFE0001, 0xFFFD0002, 0xFFFB0004, 0xFFF70008,
	0xFFEF0010, 0xFFDF0020, 0xFFBF0040, 0xFF7F0080,
	0xFEFF0100, 0xFDFF0200, 0xFBFF0400, 0xF7FF0800,
	0xEFFF1000, 0xDFFF2000, 0xBFFF4000, 0x7FFF8000,
};


static unsigned int CC1_PatArray[] = {
	0x55555555, 0xAAAAAAAA, 0x00000000, 
	0x55555555, 0xAAAAAAAA, 0x00000000
};

#define RESULT_SUMMARY(errs, str) \
{\
        if (errs)\
                sum_error(str);\
	else\
		okydoky();\
}

#define WAIT_FOR_TRANSFER_READY \
{\
	static int tries=0; \
	while ((CC_R1 (base, fbctl) & 0x80) == 0)  {\
		++tries; \
		if (tries > 10000) {\
			msg_printf(ERR, "WAIT_FOR_TRANSFER_READY Time out transfer ~ready 0x%0X \n",base->cc1.fbctl); \
			tries = 0; \
			break;\
		}\
	}\
} 

#define WRITE_CC1_FRAME_BUFFER_SETUP(fld_frm_sel,startline) \
{\
	int fbsel;\
	fbsel = (int)fld_frm_sel >> 1;\
        CC_W1 (base, indrct_addreg, 56 );\
        CC_W1 (base, indrct_datareg, 0x30);\
	CC_W1 (base, fbsel, (unsigned char)(fbsel<<5) ); \
	CC_W1 (base, fbsel, (unsigned char)(CC_R1(base, fbsel) | fld_frm_sel & 0x1)); \
        CC_W1 (base, indrct_addreg, 49);\
        CC_W1 (base, indrct_datareg, (unsigned char)((startline >> 1)  & 0xFF)); \
        CC_W1 (base, indrct_addreg, 48);\
        CC_W1 (base, indrct_datareg, 0x2);\
	CC_W1 (base, fbctl, (unsigned char)(((startline & 0x1) << 5) | 0xC0));\
	CC_W1 (base, fbctl, 0);\
}


#define	READ_CC1_FRAME_BUFFER_SETUP(fld_frm_sel,startline) \
{\
	int fbsel;\
	fbsel = (int)fld_frm_sel >> 1;\
	CC_W1 (base, indrct_addreg, 56);\
	CC_W1 (base, indrct_datareg, 0x0);\
	CC_W1 (base, fbsel, (unsigned char)(fbsel<<5)); \
	CC_W1 (base, fbsel, (unsigned char)(CC_R1(base, fbsel) | fld_frm_sel & 0x1)); \
	CC_W1 (base, indrct_addreg, 49);\
        CC_W1 (base, indrct_datareg, (unsigned char)((startline >> 1)  & 0xFF)); \
	CC_W1 (base, indrct_addreg, 48);\
	CC_W1 (base, indrct_datareg, 0x6);\
	CC_W1 (base, fbctl, (unsigned char)(((startline & 0x1) << 5) | 0xC0));\
	CC_W1 (base, fbctl, 0);\
}


#define PIXELS_PER_LINE			1280/4
#define LINE_COUNT			320

#ifdef PAD
#define PIXELS_PER_LINE			1248/4
#define LINE_COUNT			30
#endif

unsigned int pad_buf [4] = {0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF};

static int CC1_Dram_AddrBus_Test(void);
static int CC1_Dram_DataBus_Test(void);
static int CC1_Dram_Data_Test(void);

static int
CC1_Dram_AddrBus_Test(void)
{
	unsigned int errCnt;
	unsigned int pixels, dataval;
	unsigned int exp, rcv;
	unsigned int ffsel, field_frames_sel;
	int line;

	errCnt=0;
	dataval=0;
	msg_printf(VRB, "CC1_Dram_AddrBus_Test......\n");
	CC_W1 (base, sysctl, 0x48);



	/* CC1 direct reg4, bits[6:4]  field buffer select for host read/write*/
	/* Bit[4]  :	Odd/Even Field Select */
	/* Bits[6:5]: Frame Buffer Select */
	for(ffsel=0x00; ffsel<0x7;ffsel++) {
		field_frames_sel = (ffsel<<4) & 0x70;

		msg_printf(DBG, "CC1_Dram_AddrBus_Test - Writing\n");
		for(line=0; line < LINE_COUNT; line++) {
			if ((line & 127) == 0) busy (1);
 			WRITE_CC1_FRAME_BUFFER_SETUP(field_frames_sel, line);
			for(pixels=0; pixels< PIXELS_PER_LINE; pixels++){
				dataval = (((line<<16) & 0xFFFF0000) | (pixels & 0xFFFF));
				msg_printf(DBG, "CC1_Dram_AddrBus_Test line= 0x%04X pixels= 0x%04X, field_frame= 0x%04X Writing 0x%08X......\n", line, pixels, field_frames_sel,dataval);
				CC_W4 (base, fbdata, (int)dataval);
			}

			/* CC Dram Writes  must  be  multiple of 48 (bytes) */
			/* We have to pad additional 16 bytes */
			CC_W4 (base, fbdata, pad_buf[0]);
			CC_W4 (base, fbdata, pad_buf[1]);
			CC_W4 (base, fbdata, pad_buf[2]);
			CC_W4 (base, fbdata, pad_buf[3]);
		}
		
		busy(1);
		/* Read Back and Verify */
		/* Read do not need to worry about the padding */
		/* Dummy Setup needed for WRITE to READ transition */


		READ_CC1_FRAME_BUFFER_SETUP(0, 0);
		WAIT_FOR_TRANSFER_READY;

		msg_printf(DBG, "CC1_Dram_AddrBus_Test - Read Back & Verify\n");
		for(line=0; line<LINE_COUNT; line++) {
			if ((line & 127) == 0) busy (1);
			READ_CC1_FRAME_BUFFER_SETUP(field_frames_sel, line);
			WAIT_FOR_TRANSFER_READY;
                        for(pixels=0; pixels< PIXELS_PER_LINE; pixels++){
			       exp = (((line<<16) & 0xFFFF0000) | (pixels & 0xFFFF));
                               rcv = CC_R4 (base, fbdata);
msg_printf(DBG,"AddrBus_Test : line=0x%04X, pixel=0x%04X, field_frame= 0x%04X, exp=0x%08X rcv=0x%08X\n" ,line, field_frames_sel, pixels, exp, rcv);
                               if (rcv  != exp)  {
					msg_printf(ERR, "CC1_Dram_AddrBus_Test : line=0x%x, pixel=0x%x, exp=0x%08X rcv=0x%08X\n" ,line, pixels, exp, rcv);
                                        ++errCnt;
				}
		 }
	   }
	}
	busy(0);
	if (errCnt == 0)
		msg_printf(SUM, "CC1_Dram_AddrBus_Test Passed ......\n");
	else
		msg_printf(ERR, "CC1_Dram_AddrBus_Test Failed  %d errors\n", errCnt);

	return((int)errCnt);
}

static int
CC1_Dram_DataBus_Test(void)
{
	unsigned int errCnt;
        unsigned int rcv ;
        unsigned int ffsel, field_frames_sel, index;
	int line;

	errCnt = 0;
	msg_printf(SUM, "CC1_Dram_DataBus_Test......\n");
	line=0;

        for(ffsel=0x00; ffsel<0x7;ffsel++) {
               	field_frames_sel = (ffsel<<4) & 0x70;
		
		line = 100;
		WRITE_CC1_FRAME_BUFFER_SETUP(field_frames_sel, line);
		msg_printf(DBG, "CC1_Dram_DataBus_Test Writing\n");
		for(index=0; index<16; index++ ) {
			CC_W4 (base, fbdata, *(WalkOne + index));
			msg_printf(DBG, "CC1_Dram_DataBus_Test Writing 0x%08X......\n" ,*(WalkOne + index));
		}

		/* We have sent 64 Bytes of data so far. */
		/* Added 32 bytes more, Multiple of 48 */
		for(index=0; index<8; index++) {
			CC_W4 (base, fbdata, 0xDEADBEEF);
		}

		busy(1);
		READ_CC1_FRAME_BUFFER_SETUP(0, 0);
		WAIT_FOR_TRANSFER_READY;

		msg_printf(DBG, "CC1_Dram_DataBus_Test Readback + Verify\n");
		/* Read Back and Verify */
		READ_CC1_FRAME_BUFFER_SETUP(field_frames_sel, line);
		WAIT_FOR_TRANSFER_READY;
		for(index=0; index<16; index++ ){
                               rcv = CC_R4 (base, fbdata);
                               msg_printf(DBG,"CC1_Dram_Data_Test :: Exp=0x%08X Rcv= 0x%08X\n" , *(WalkOne + index), rcv);
                               if (rcv  != *(WalkOne + index))  {
                                       msg_printf(ERR,"CC1_Dram_Data_Test :: Exp=0x%08X Rcv= 0x%08X\n" , *(WalkOne + index), rcv);
					++errCnt;
				}

		}
	}
	busy(0);
        if (errCnt == 0)
                msg_printf(SUM, "\rCC1_Dram_DataBus_Test Passed\n");
        else
                msg_printf(ERR, "\rCC1_Dram_DataBus_Test had %d errors\n", errCnt);


	return((int)errCnt);
}


static int
CC1_Dram_Data_Test(void)
{
	unsigned int errCnt;
	unsigned int line, pixels;
	unsigned int loop, *patrn, rcv, tmpval ;
	unsigned int ffsel, field_frames_sel, index;

	errCnt=0;
	msg_printf(SUM, "CC1_Dram_Data_Test......\n");
	line=0;
	/* CC1 direct reg4, bits[6:4]  field buffer select for host read/write*/
	/* Bit[4]  :	Odd/Even Field Select */
	/* Bits[6:5]: Frame Buffer Select */
	for(ffsel=0x00; ffsel<0x7;ffsel++) {
		field_frames_sel = (ffsel<<4) & 0x70;

		for(loop=0; loop<3; loop++) {
			msg_printf(DBG, "CC1_Dram_Data_Test Writing\n");
                	for(line=0,index=0; line<LINE_COUNT; line++) {
				if ((line & 127) == 0) busy (1);
 				WRITE_CC1_FRAME_BUFFER_SETUP(field_frames_sel, line);
                        	for(pixels=0; pixels<PIXELS_PER_LINE; pixels+=4){
					patrn = &CC1_PatArray[loop];
					tmpval= *patrn;

					msg_printf(DBG,"CC1_Dram_Data_Test Writing line= 0x%04X pixel= 0x%04X data= 0x%08X\n" , line, pixels, *patrn);
					CC_W4 (base, fbdata, *(patrn++));
					msg_printf(DBG,"CC1_Dram_Data_Test Writing line= 0x%04X pixel= 0x%04X data= 0x%08X\n" , line, pixels+1, *patrn);
					CC_W4 (base, fbdata, *(patrn++));
					msg_printf(DBG,"CC1_Dram_Data_Test Writing line= 0x%04X pixel= 0x%04X data= 0x%08X\n" , line, pixels+2, *patrn);
					CC_W4 (base, fbdata, *(patrn++));
					msg_printf(DBG,"CC1_Dram_Data_Test Writing line= 0x%04X pixel= 0x%04X data= 0x%08X\n" , line, pixels+3, *patrn);
					CC_W4 (base, fbdata, tmpval);
				}
				/* PAD ... */
                        	/* CC Dram Writes  must  be  multiple of 48 (bytes) */
                        	/* We have to pad additional 16 bytes */
                        	CC_W4 (base, fbdata, pad_buf[0]);
                        	CC_W4 (base, fbdata, pad_buf[1]);
                        	CC_W4 (base, fbdata, pad_buf[2]);
                        	CC_W4 (base, fbdata, pad_buf[3]);
			}

			busy(1);
			/* Read Back and Verify */
			READ_CC1_FRAME_BUFFER_SETUP(0, 0);
			WAIT_FOR_TRANSFER_READY;
			msg_printf(DBG, "CC1_Dram_Data_Test Read Back + Verify......\n");

                	for(line=0,index=0; line<LINE_COUNT; line++) {
				if ((line & 127) == 0) busy (1);
			        READ_CC1_FRAME_BUFFER_SETUP(field_frames_sel, line);
				WAIT_FOR_TRANSFER_READY;
                        	for(pixels=0; pixels< PIXELS_PER_LINE;pixels +=4){
                                	patrn = &CC1_PatArray[loop];
					tmpval= *patrn;
					for(index=0;index<3; index++) {
						rcv = CC_R4 (base, fbdata);
						msg_printf(DBG, "Data_Test :: line= 0x%x, pixel= 0x%x, exp= 0x%x rcv= 0x%x\n" ,line, pixels, *patrn, rcv);
						if (rcv != *patrn) {
							msg_printf(ERR, "CC1_Dram_Data_Test :: loop= %d line= 0x%08X, pixel= 0x%08X, exp= 0x%08X rcv= 0x%08X\n" ,loop,line, pixels, *patrn, rcv);
							++errCnt;
						}
						++patrn;
					}
					rcv = CC_R4 (base, fbdata);
						msg_printf(DBG, "Data_Test :: line= 0x%x, pixel= 0x%x, exp= 0x%x rcv= 0x%x\n" ,line, pixels, tmpval, rcv);
					if (rcv != tmpval) {
							msg_printf(VRB, "CC1_Dram_Data_Test :: loop= %d line= 0x%08X, pixel= 0x%08X, exp= 0x%08X rcv= 0x%08X\n" ,loop,line, pixels, *patrn, rcv);
						++errCnt;
					}
				}
			}
		}
	}
	busy(0);
	if (errCnt > 0) 
		msg_printf(ERR, "\rCC1_Dram_Data_Test FAILED, Errors=%d\n", 
			errCnt);
	else 
		msg_printf(SUM, "\rCC1_Dram_Data_Test PASSED\n");

	return((int)errCnt);
}

/*ARGSUSED*/
int
CC1_AddrBus_Test(int argc, char *argv[])
{
	int errs; 

	errs = CC1_Dram_AddrBus_Test();
	RESULT_SUMMARY(errs, "CC1_Dram_AddrBus......\n");
	return(errs);
}

/*ARGSUSED*/
int
CC1_DataBus_Test(int argc, char *argv[])
{
	int errs; 

	errs = CC1_Dram_DataBus_Test();
	RESULT_SUMMARY(errs, "CC1_Dram_DataBus......\n");
	return(errs);
}

/*ARGSUSED*/
int
CC1_Dram_Mem_Test(int argc, char *argv[])
{
	int errs; 

	errs = CC1_Dram_AddrBus_Test();
	errs += CC1_Dram_DataBus_Test();
	errs += CC1_Dram_Data_Test();
	RESULT_SUMMARY(errs, "CC1_Dram_Mem_Test......\n");
	return(errs);
}
