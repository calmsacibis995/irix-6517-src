#include "sys/types.h"
#include "sys/sbd.h"
#include "sys/sema.h"
#include "sys/param.h"
#include "sys/mc.h"
#include "sys/gfx.h"
#include "sys/gr2.h"
#include "sys/gr2hw.h"
#include "sys/gr2_if.h"
#include "ide_msg.h"
#include "../../graphics/dma.h"
#include "uif.h"

extern struct gr2_hw  *base ;

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
        base->cc1.indrct_addreg = 56 ;\
        base->cc1.indrct_datareg = 0x30;\
	base->cc1.fbsel = fld_frm_sel ; \
        base->cc1.indrct_addreg = 49;\
        base->cc1.indrct_datareg = ((startline >> 1)  & 0xFF); \
        base->cc1.indrct_addreg = 48;\
        base->cc1.indrct_datareg = 0x2;\
 	base->cc1.fbctl  = (((startline & 0x1) << 5) | 0x40);\
}


#define	READ_CC1_FRAME_BUFFER_SETUP(fld_frm_sel,startline) \
{\
	base->cc1.indrct_addreg = 56;\
	base->cc1.indrct_datareg = 0x0;\
	base->cc1.fbsel = fld_frm_sel ; \
	base->cc1.indrct_addreg = 49;\
        base->cc1.indrct_datareg = ((startline >> 1)  & 0xFF); \
	base->cc1.indrct_addreg = 48;\
	base->cc1.indrct_datareg = 0x6;\
 	base->cc1.fbctl  = (((startline & 0x1) << 5) | 0x40);\
}


#define PIXELS_PER_LINE			1280/4
#define LINE_COUNT			320

CC1_Dram_DMA_Test(int argc, char *argv[])
{
	unsigned int errCnt;
	unsigned int *wbuf;
	unsigned int line, pixels, i;
	unsigned int loop, *patrn, exp, rcv ;
	unsigned int ffsel, field_frames_sel;
	int nbytes;

	errCnt=0;
	msg_printf(DBG, "CC1_Dram_Test......\n");

	basic_DMA_setup(0);
	base->cc1.sysctl  = 0x48;

        /*
         * Initialize data buffer
         */
        wbuf = (unsigned int *) PHYS_TO_K0(vdma_wphys);

	/* we wrote total of 1296 bytes of data (even multiple of 48) */
        for (pixels=0; pixels<PIXELS_PER_LINE; pixels++)
                *wbuf++ = pixels;
	for (i=0; i<4; i++)
		*wbuf++ = 0xdeadbeef;

	nbytes = 1280+16;

        /* writeback-invalidate data cache */
        flush_cache();

	/* CC1 direct reg4, bits[6:4]  field buffer select for host read/write*/
	/* Bit[4]  :	Odd/Even Field Select */
	/* Bits[6:5]: Frame Buffer Select */
	for(ffsel=0x00; ffsel<0x7;ffsel++) {
		field_frames_sel = (ffsel<<4) & 0x70;

		for(line=0; line < LINE_COUNT; line++) {
		        /* set DMA_SYNC source to 0 to synchronize the 
				transfer with vertical retrace. */
        		base->hq.dmasync = 0;
			writemcreg (DMA_CAUSE,0);

 			WRITE_CC1_FRAME_BUFFER_SETUP(field_frames_sel, line);
			WAIT_FOR_TRANSFER_READY;

        		/*
         	  	 * Start the dma
         	 	 */
        		dma_go ((caddr_t)&base->cc1.fbdata, vdma_wphys, 1,
				nbytes, 0, 1, 0x50|VDMA_SYNC);

        		if (dma_error_handler()) return -1;
		}

		
		/* Read Back and Verify */
		/* Read do not need to worry about the padding */
		READ_CC1_FRAME_BUFFER_SETUP(field_frames_sel, line);
	        WAIT_FOR_TRANSFER_READY;
		for(line=0; line<LINE_COUNT; line++) {
			READ_CC1_FRAME_BUFFER_SETUP(field_frames_sel, line);
                        for(pixels=0; pixels< PIXELS_PER_LINE; pixels++){
			       WAIT_FOR_TRANSFER_READY;
                               rcv = base->cc1.fbdata;
#if 0
msg_printf(DBG,"dRAM_DMA_test : line=0x%x, pixel=0x%x, exp=0x%x rcv=0x%x\n" ,line, pixels, 0x55AADAAD, rcv);
#endif
                               if (rcv  != pixels)  {
					msg_printf(ERR, "CC1_Dram_AddrBus_Test : line=0x%x, pixel=0x%x, exp= 0x%08X rcv=0x%x\n" ,line, pixels, pixels, rcv);
                                        ++errCnt;
				}
		 	}
	   	}
	}
	if (errCnt)
		sum_error("CC1 dRAM DMA test");
	else
		okydoky();
	return(errCnt);
}

