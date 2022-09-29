#include "sys/types.h"
#include "sys/sbd.h"
#include "sys/sema.h"
#include "sys/param.h"
#include "sys/mc.h"
#include "uif.h"
#include "ide_msg.h"
#include "fforward/graphics/dma.h"
#include "sys/mgrashw.h"
#include "sys/mgras.h"
#include "mgv1_regio.h"

extern mgras_hw *mgbase;

#define RESULT_SUMMARY(errs, str) \
{\
        if (errs)\
                sum_error(str);\
}
#define HQ3
#ifdef HQ3
#define WAIT_FOR_TRANSFER_READY \
{\
	static int tries=0; \
	msg_printf(DBG, "WAIT_FOR_TRANSFER_READY   \n"); \
	while ((CC_R1 (mgbase, fbctl) & 0x80) == 0)  {\
		++tries; \
		if (tries > 10000) {\
			msg_printf(VRB, "WAIT_FOR_TRANSFER_READY Time out transfer ~ready 0x%0X \n",CC_R1 (mgbase, fbctl)); \
			tries = 0; \
			break;\
		}\
	}\
} 
#else
#define WAIT_FOR_TRANSFER_READY \
{\
	static int tries=0; \
	msg_printf(DBG, "WAIT_FOR_TRANSFER_READY   \n"); \
	while ((CC_R1 (mgbase, fbctl) & 0x80) == 0)  {\
		++tries; \
		if (tries > 10000) {\
			msg_printf(VRB, "WAIT_FOR_TRANSFER_READY Time out transfer ~ready 0x%0X \n",CC_R1 (mgbase, fbctl)); \
			tries = 0; \
			break;\
		}\
	}\
} 
#endif
#define WRITE_CC1_FRAME_BUFFER_SETUP(fld_frm_sel,startline) \
{\
        CC_W1 (mgbase, indrct_addreg, 56 );\
        CC_W1 (mgbase, indrct_datareg, 0x30);\
	CC_W1 (mgbase, fbsel, fld_frm_sel ); \
        CC_W1 (mgbase, indrct_addreg, 49);\
        CC_W1 (mgbase, indrct_datareg, ((startline >> 1)  & 0xFF)); \
        CC_W1 (mgbase, indrct_addreg, 48);\
        CC_W1 (mgbase, indrct_datareg, 0x2);\
 	CC_W1 (mgbase, fbctl, (((startline & 0x1) << 5) | 0xC0));\
	CC_W1 (mgbase, fbctl, 0);\
}


#define	READ_CC1_FRAME_BUFFER_SETUP(fld_frm_sel,startline) \
{\
	CC_W1 (mgbase, indrct_addreg, 56);\
	CC_W1 (mgbase, indrct_datareg, 0x0);\
	CC_W1 (mgbase, fbsel, fld_frm_sel ); \
	CC_W1 (mgbase, indrct_addreg, 49);\
        CC_W1 (mgbase, indrct_datareg, ((startline >> 1)  & 0xFF)); \
	CC_W1 (mgbase, indrct_addreg, 48);\
	CC_W1 (mgbase, indrct_datareg, 0x6);\
 	CC_W1 (mgbase, fbctl, (((startline & 0x1) << 5) | 0xC0));\
	CC_W1 (mgbase, fbctl, 0);\
}


#define PIXELS_PER_LINE			1280/4
#define LINE_COUNT			320
#define HQ3 

mgv1_CC1_Dram_DMA_Test(int argc, char *argv[])
{

	extern int GVType;

	unsigned int errCnt;
	unsigned int *wbuf;
	unsigned int line, pixels, i;
	unsigned int loop, *patrn, exp, rcv ;
	unsigned int ffsel, field_frames_sel;
	unsigned long gioaddr;
#ifndef HQ3
	int nbytes;
#endif

	errCnt=0;
	msg_printf(SUM, "CC1_Dram_DMA_Test......\n");

	if (GVType == GVType_ng1) {	/* skip for now */
		msg_printf (SUM, "(Bypassed)\n");
		return (0);
	}

	basic_DMA_setup(0);
	CC_W1 (mgbase, sysctl, 0x48);

        /*
         * Initialize data buffer
         */
        wbuf = (unsigned int *) PHYS_TO_K0(vdma_wphys);

	/* we wrote total of 1296 bytes of data (even multiple of 48) */
        for (pixels=0; pixels<PIXELS_PER_LINE; pixels++)
                *wbuf++ = pixels;
	for (i=0; i<4; i++)
		*wbuf++ = 0xdeadbeef;

#ifndef HQ3
	nbytes = 1280+16;
#endif

        /* writeback-invalidate data cache */
        flush_cache();

	/* CC1 direct reg4, bits[6:4]  field buffer select for host read/write*/
	/* Bit[4]  :	Odd/Even Field Select */
	/* Bits[6:5]: Frame Buffer Select */
	for(ffsel=0x00; ffsel<0x7;ffsel++) {
		field_frames_sel = (ffsel<<4) & 0x70;

	 	msg_printf(DBG, "CC1_Dram_DMA_Test :  Writing..\n");
		for(line=0; line < LINE_COUNT; line++) {
			if ((line & 127) == 0) busy (1);
		        /* set DMA_SYNC source to 0 to synchronize the 
				transfer with vertical retrace. */
#ifndef HQ3
        		base->hq.dmasync = 0;
#endif
			writemcreg (DMA_CAUSE,0);

 			WRITE_CC1_FRAME_BUFFER_SETUP(field_frames_sel, line);
			WAIT_FOR_TRANSFER_READY;

        		/*
         	  	 * Start the dma
         	 	 */
#ifndef HQ3
        		dma_go (&base->cc1.fbdata, vdma_wphys, 1, nbytes, 0, 1,
				0x50|VDMA_SYNC);
#endif 
        		if (dma_error_handler()) return -1;
		}

		
		busy(0);
		/* Read Back and Verify */
		/* Read do not need to worry about the padding */
	 	msg_printf(DBG, "CC1_Dram_DMA_Test :  Read Back and Verify\n");
		READ_CC1_FRAME_BUFFER_SETUP(field_frames_sel, line);
	        WAIT_FOR_TRANSFER_READY;
		for(line=0; line<LINE_COUNT; line++) {
			if ((line & 127) == 0) busy (1);
			READ_CC1_FRAME_BUFFER_SETUP(field_frames_sel, line);
                        for(pixels=0; pixels< PIXELS_PER_LINE; pixels++){
			       WAIT_FOR_TRANSFER_READY;
                               rcv = CC_R4 (mgbase, fbdata);
msg_printf(DBG,"CC1_Dram_DMA_Test : line=0x%x, pixel=0x%x, exp=0x%x rcv=0x%x\n" ,line, pixels, 0x55AADAAD, rcv);
                               if (rcv  != pixels)  {
					msg_printf(ERR, "CC1_Dram_AddrBus_Test : line=0x%x, pixel=0x%x, exp= 0x%08X rcv=0x%x\n" ,line, pixels, pixels, rcv);
                                        ++errCnt;
				}
		 	}
	   	}
	}
	busy(0);
	if (errCnt)
	 	msg_printf(SUM, "\rCC1_Dram_DMA_Test Failed... Errors 0x%08X \n", errCnt);
	else {
	 	msg_printf(SUM, "\rCC1_Dram_DMA_Test PASSED\n");
	}
	return(errCnt);

}


