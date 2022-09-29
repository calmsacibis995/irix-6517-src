#include "sys/types.h"
#include "sys/sbd.h"
#include "sys/sema.h"
#include "sys/gfx.h"
#include "sys/gr2.h"
#include "sys/gr2hw.h"
#include "sys/gr2_if.h"
#include "ide_msg.h"
#include "sys/param.h"
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
	while ((base->cc1.fbctl & 0x80) == 0)  {\
		msg_printf(DBG, "~transfer ready 0x%0X \n",base->cc1.fbctl); \
		DELAY(1); \
	}\
} 

#define WRITE_CC1_FRAME_BUFFER_SETUP(fld_frm_sel,startline) \
{\
	int fbsel;\
	fbsel = fld_frm_sel >> 1;\
        base->cc1.indrct_addreg = 56 ;\
        base->cc1.indrct_datareg = 0x30;\
	base->cc1.fbsel = fbsel<<5 ; \
	DELAY(1);\
	base->cc1.fbsel |= fld_frm_sel & 0x1 ; \
        base->cc1.indrct_addreg = 49;\
        base->cc1.indrct_datareg = ((startline >> 1)  & 0xFF); \
        base->cc1.indrct_addreg = 48;\
        base->cc1.indrct_datareg = 0x2;\
	base->cc1.fbctl  = (((startline & 0x1) << 5) | 0x40);\
}


#define	READ_CC1_FRAME_BUFFER_SETUP(fld_frm_sel,startline) \
{\
	int fbsel;\
	fbsel = fld_frm_sel >> 1;\
	base->cc1.indrct_addreg = 56;\
	base->cc1.indrct_datareg = 0x0;\
	base->cc1.fbsel = fbsel<<5; \
	DELAY(1);\
	base->cc1.fbsel |= fld_frm_sel & 0x1 ; \
	base->cc1.indrct_addreg = 49;\
        base->cc1.indrct_datareg = ((startline >> 1)  & 0xFF); \
	base->cc1.indrct_addreg = 48;\
	base->cc1.indrct_datareg = 0x6;\
	base->cc1.fbctl  = (((startline & 0x1) << 5) | 0x40);\
}


#define PIXELS_PER_LINE			1280/4
#define LINE_COUNT			320

#ifdef PAD
#define PIXELS_PER_LINE			1248/4
#define LINE_COUNT			30
#endif

unsigned int pad_buf [4] = {0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF};

static int
CC1_Dram_AddrBus_Test()
{
	unsigned int errCnt;
	unsigned int line, pixels, dataval;
	unsigned int loop, exp, rcv ;
	unsigned int ffsel, field_frames_sel;

	errCnt=0;
	dataval=0;
	msg_printf(DBG, "CC1_Dram_AddrBus_Test......\n");
	base->cc1.sysctl  = 0x48;



	for(ffsel=0x00; ffsel<0x7;ffsel++) {
		field_frames_sel = (ffsel<<4) & 0x70;

		for(line=0; line < LINE_COUNT; line++) {
 			WRITE_CC1_FRAME_BUFFER_SETUP(field_frames_sel, line);
			for(pixels=0; pixels< PIXELS_PER_LINE; pixels++){
				WAIT_FOR_TRANSFER_READY;
				base->cc1.fbdata  = 0x80808080;
			}

			/* CC Dram Writes  must  be  multiple of 48 (bytes) */
			/* We have to pad additional 16 bytes */
			WAIT_FOR_TRANSFER_READY;
			base->cc1.fbdata = pad_buf[0];
			WAIT_FOR_TRANSFER_READY;
			base->cc1.fbdata = pad_buf[1];
			WAIT_FOR_TRANSFER_READY;
			base->cc1.fbdata = pad_buf[2];
			WAIT_FOR_TRANSFER_READY;
			base->cc1.fbdata = pad_buf[3];
			WAIT_FOR_TRANSFER_READY;
		}
	}


	/* CC1 direct reg4, bits[6:4]  field buffer select for host read/write*/
	/* Bit[4]  :	Odd/Even Field Select */
	/* Bits[6:5]: Frame Buffer Select */
	for(ffsel=0x00; ffsel<0x7;ffsel++) {
		field_frames_sel = (ffsel<<4) & 0x70;

		for(line=0; line < LINE_COUNT; line++) {
 			WRITE_CC1_FRAME_BUFFER_SETUP(field_frames_sel, line);
			for(pixels=0; pixels< PIXELS_PER_LINE; pixels++){
				dataval = (((line<<16) & 0xFFFF0000) | (pixels & 0xFFFF));
				WAIT_FOR_TRANSFER_READY;
				base->cc1.fbdata  = dataval;
			}

			/* CC Dram Writes  must  be  multiple of 48 (bytes) */
			/* We have to pad additional 16 bytes */
			WAIT_FOR_TRANSFER_READY;
			base->cc1.fbdata = pad_buf[0];
			WAIT_FOR_TRANSFER_READY;
			base->cc1.fbdata = pad_buf[1];
			WAIT_FOR_TRANSFER_READY;
			base->cc1.fbdata = pad_buf[2];
			WAIT_FOR_TRANSFER_READY;
			base->cc1.fbdata = pad_buf[3];
			WAIT_FOR_TRANSFER_READY;
		}
		
		/* Read Back and Verify */
		/* Read do not need to worry about the padding */
		/* Dummy Setup needed for WRITE to READ transition */


		READ_CC1_FRAME_BUFFER_SETUP(0, 0);
		WAIT_FOR_TRANSFER_READY;

		for(line=0; line<LINE_COUNT; line++) {
			READ_CC1_FRAME_BUFFER_SETUP(field_frames_sel, line);
                        for(pixels=0; pixels< PIXELS_PER_LINE; pixels++){
			       WAIT_FOR_TRANSFER_READY;
			       DELAY(1);
			       exp = (((line<<16) & 0xFFFF0000) | (pixels & 0xFFFF));
                               rcv = base->cc1.fbdata;
msg_printf(DBG,"AddrBus_Test : line=0x%04X, pixel=0x%04X, exp=0x%08X rcv=0x%08X\n" ,line, pixels, exp, rcv);
                               if (rcv  != exp)  {
					msg_printf(ERR, "CC1_Dram_AddrBus_Test : line=0x%x, pixel=0x%x, exp=0x%08X rcv=0x%08X\n" ,line, pixels, exp, rcv);
                                        ++errCnt;
				}
		 }
	   }
	}
	return(errCnt);
}

static int
CC1_Dram_DataBus_Test()
{
	unsigned int errCnt;
	unsigned int line, pixels;
        unsigned int loop, *patrn, exp, rcv ;
        unsigned int ffsel, field_frames_sel, index;

	errCnt = 0;
	msg_printf(DBG, "CC1_Dram_DataBus_Test......\n");
	line=0;

        for(ffsel=0x00; ffsel<0x7;ffsel++) {
               	field_frames_sel = (ffsel<<4) & 0x70;
		
		line = 100;
		WRITE_CC1_FRAME_BUFFER_SETUP(field_frames_sel, line);
		for(index=0; index<16; index++ ) {
			WAIT_FOR_TRANSFER_READY;
			base->cc1.fbdata= *(WalkOne + index);
		}

		/* We have sent 64 Bytes of data so far. */
		/* Added 32 bytes more, Multiple of 48 */
		for(index=0; index<8; index++) {
			WAIT_FOR_TRANSFER_READY;
			base->cc1.fbdata= 0xDEADBEEF;
		}

		READ_CC1_FRAME_BUFFER_SETUP(0, 0);
		WAIT_FOR_TRANSFER_READY;

		/* Read Back and Verify */
		READ_CC1_FRAME_BUFFER_SETUP(field_frames_sel, line);
		for(index=0; index<16; index++ ){
			       WAIT_FOR_TRANSFER_READY;
			       DELAY(1);
                               rcv = base->cc1.fbdata;
                               if (rcv  != *(WalkOne + index))  {
                                       msg_printf(ERR,"CC1_Dram_Data_Test :: Exp=0x%08X Rcv= 0x%08X\n" , *(WalkOne + index), rcv);
					++errCnt;
				}

		}
	}
	return(errCnt);
}


static int
CC1_Dram_Data_Test()
{
	unsigned int errCnt;
	unsigned int line, pixels;
	unsigned int loop, *patrn, exp, rcv, tmpval ;
	unsigned int ffsel, field_frames_sel, index;

	errCnt=0;
	msg_printf(DBG, "CC1_Dram_Data_Test......\n");
	line=0;
	/* CC1 direct reg4, bits[6:4]  field buffer select for host read/write*/
	/* Bit[4]  :	Odd/Even Field Select */
	/* Bits[6:5]: Frame Buffer Select */
	for(ffsel=0x00; ffsel<0x7;ffsel++) {
		field_frames_sel = (ffsel<<4) & 0x70;

		for(loop=0; loop<3; loop++) {
                	for(line=0,index=0; line<LINE_COUNT; line++) {
 				WRITE_CC1_FRAME_BUFFER_SETUP(field_frames_sel, line);
                        	for(pixels=0; pixels<PIXELS_PER_LINE; pixels+=4){
					patrn = &CC1_PatArray[loop];
					tmpval= *patrn;

					WAIT_FOR_TRANSFER_READY;
					base->cc1.fbdata = *(patrn++);
					WAIT_FOR_TRANSFER_READY;
					base->cc1.fbdata = *(patrn++);
					WAIT_FOR_TRANSFER_READY;
					base->cc1.fbdata = *(patrn++);
					WAIT_FOR_TRANSFER_READY;
					base->cc1.fbdata = tmpval;
					WAIT_FOR_TRANSFER_READY;
				}
				/* PAD ... */
                        	/* CC Dram Writes  must  be  multiple of 48 (bytes) */
                        	/* We have to pad additional 16 bytes */
                        	base->cc1.fbdata = pad_buf[0];
				WAIT_FOR_TRANSFER_READY;
                        	base->cc1.fbdata = pad_buf[1];
				WAIT_FOR_TRANSFER_READY;
                        	base->cc1.fbdata = pad_buf[2];
				WAIT_FOR_TRANSFER_READY;
                        	base->cc1.fbdata = pad_buf[3];
				WAIT_FOR_TRANSFER_READY;
			}

			/* Read Back and Verify */
			READ_CC1_FRAME_BUFFER_SETUP(0, 0);
			WAIT_FOR_TRANSFER_READY;

                	for(line=0,index=0; line<LINE_COUNT; line++) {
			        READ_CC1_FRAME_BUFFER_SETUP(field_frames_sel, line);
                        	for(pixels=0; pixels< PIXELS_PER_LINE;pixels +=4){
                                	patrn = &CC1_PatArray[loop];
					tmpval= *patrn;
					for(index=0;index<3; index++) {
						WAIT_FOR_TRANSFER_READY;
						rcv = base->cc1.fbdata;
msg_printf(DBG, "Data_Test :: line= 0x%x, pixel= 0x%x, exp= 0x%x rcv= 0x%x\n" ,line, pixels, *patrn, rcv);
						if (rcv != *patrn) {
							msg_printf(VRB, "CC1_Dram_Data_Test :: loop= %d line= 0x%08X, pixel= 0x%08X, exp= 0x%08X rcv= 0x%08X\n" ,loop,line, pixels, *patrn, rcv);
							++errCnt;
						}
						++patrn;
					}
					WAIT_FOR_TRANSFER_READY;
					rcv = base->cc1.fbdata;
					if (rcv != tmpval) {
							msg_printf(VRB, "CC1_Dram_Data_Test :: loop= %d line= 0x%08X, pixel= 0x%08X, exp= 0x%08X rcv= 0x%08X\n" ,loop,line, pixels, *patrn, rcv);
						++errCnt;
					}
				}
			}
		}
	}

	if (errCnt > 0) 
		msg_printf(ERR, "CC1_Dram_Data_Test :: Too Many Errors, Errors=%d\n" ,errCnt);
	return(errCnt);
}

CC1_AddrBus_Test(int argc, char *argv[])
{
	int errs; 

	msg_printf(DBG, "CC1_Dram_AddrBus_Test......\n");
	errs = CC1_Dram_AddrBus_Test();
	RESULT_SUMMARY(errs, "CC1_Dram_AddrBus......\n");
	return(errs);
}

CC1_DataBus_Test(int argc, char *argv[])
{
	int errs; 

	msg_printf(DBG, "CC1_Dram_DataBus......\n");
	errs = CC1_Dram_DataBus_Test();
	RESULT_SUMMARY(errs, "CC1_Dram_DataBus......\n");
	return(errs);
}

CC1_Dram_Mem_Test(int argc, char *argv[])
{
	int errs; 

	msg_printf(DBG, "CC1_Dram_Mem_Test......\n");
	errs = CC1_Dram_Data_Test();
	RESULT_SUMMARY(errs, "CC1_Dram_Mem_Test......\n");
	return(errs);
}


