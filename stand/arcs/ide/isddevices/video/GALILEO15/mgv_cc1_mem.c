/*
**               Copyright (C) 1989, Silicon Graphics, Inc.
**
**  These coded instructions, statements, and computer programs  contain
**  unpublished  proprietary  information of Silicon Graphics, Inc., and
**  are protected by Federal copyright law.  They  may  not be disclosed
**  to  third  parties  or copied or duplicated in any form, in whole or
**  in part, without the prior written consent of Silicon Graphics, Inc.
*/

/*
**      FileName:       mgv_cc1_mem.c
*/

#include "mgv_diag.h"

void 	_mgv_CC1TestSetup(void);

/*
 * NAME: _mgv_CC1WaitForTransferReady
 *
 * FUNCTION: Wait until the data is transferred
 *
 * INPUTS: mgvbase
 *
 * OUTPUTS: 0x1 if NOT ready, 0x0 if ready
 */
__uint32_t
_mgv_CC1WaitForTransferReady (mgvdiag *mgvbase)
{
    __uint32_t tries = 0, _val;

    do {
	CC_R1(mgvbase, CC_FRAME_BUF, _val);
	tries++;
	if (tries > 10000) {
	    msg_printf(ERR, "CC1 Transfer Ready Time Out\n");
	    return (0x1);
	}
    } while ((_val & 0x80) == 0x0);

    return(0x0);

}

/*
 * NAME: _mgv_CC1WriteFrameBufferSetup
 *
 * FUNCTION: Setup CC for data write
 *
 * INPUTS: mgvbase, fld_frm_sel, startline
 *
 * OUTPUTS: none
 */
void
_mgv_CC1WriteFrameBufferSetup(mgvdiag *mgvbase, __uint32_t fld_frm_sel, __uint32_t startline)
{
    __uint32_t fbsel, tmp;

    fbsel = fld_frm_sel >> 1;

    CC_W1 (mgvbase, CC_INDIR1, 56 );
    CC_W1 (mgvbase, CC_INDIR2, 0x30);
    CC_W1 (mgvbase, CC_FLD_FRM_SEL, fbsel<<5 ); 

    CC_R1 (mgvbase, CC_FLD_FRM_SEL, tmp);
    CC_W1 (mgvbase, CC_FLD_FRM_SEL, tmp | fld_frm_sel & 0x1); 

    CC_W1 (mgvbase, CC_INDIR1, 49);
    CC_W1 (mgvbase, CC_INDIR2, ((startline >> 1)  & 0xFF)); 
    CC_W1 (mgvbase, CC_INDIR1, 48);
    CC_W1 (mgvbase, CC_INDIR2, 0x2);
    CC_W1 (mgvbase, CC_FRAME_BUF, (((startline & 0x1) << 5) | 0xC0));
    CC_W1 (mgvbase, CC_FRAME_BUF, 0);
}

/*
 * NAME: _mgv_CC1ReadFrameBufferSetup
 *
 * FUNCTION: Setup CC for data read
 *
 * INPUTS: mgvbase, fld_frm_sel, startline
 *
 * OUTPUTS: none
 */
void
_mgv_CC1ReadFrameBufferSetup(mgvdiag *mgvbase, __uint32_t fld_frm_sel, __uint32_t startline)
{
    __uint32_t fbsel, tmp;

    fbsel = fld_frm_sel >> 1;

    CC_W1 (mgvbase, CC_INDIR1, 56 );
    CC_W1 (mgvbase, CC_INDIR2, 0x0);
    CC_W1 (mgvbase, CC_FLD_FRM_SEL, fbsel<<5 ); 

    CC_R1 (mgvbase, CC_FLD_FRM_SEL, tmp);
    CC_W1 (mgvbase, CC_FLD_FRM_SEL, tmp | fld_frm_sel & 0x1); 

    CC_W1 (mgvbase, CC_INDIR1, 49);
    CC_W1 (mgvbase, CC_INDIR2, ((startline >> 1)  & 0xFF)); 
    CC_W1 (mgvbase, CC_INDIR1, 48);
    CC_W1 (mgvbase, CC_INDIR2, 0x6);
    CC_W1 (mgvbase, CC_FRAME_BUF, (((startline & 0x1) << 5) | 0xC0));
    CC_W1 (mgvbase, CC_FRAME_BUF, 0);
}

/*
 * NAME: mgv_CC1DramAddrBusTest
 *
 * FUNCTION: This test writes all four framebuffers of the CC1 using
 *           an incrementing pattern.  The pattern is a 32 bit word
 *           comprising the line number in the upper 16 bits and the
 *           pixel number in the lower 16 bits.  Once all four buffers
 *           are written, they are read back and verified.
 *
 * INPUTS: none
 *
 * OUTPUTS: errors
 */
__uint32_t
mgv_CC1DramAddrBusTest(void)
{
    __uint32_t errors = 0;
    __uint32_t line, pixels, dataval = 0;
    __uint32_t exp, rcv ;
    __uint32_t ffsel, field_frames_sel;
    __uint32_t pad_buf [4] = {0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF};

    msg_printf(SUM, "IMPV:- %s\n", mgv_err[CC1_DRAM_ADDR_BUS_TEST].TestStr);

	CC_W1(mgvbase, CC_SYSCON, 0x46);  

/***    _mgv_CC1TestSetup(); ***/ 

    /*
     * CC1 direct reg4, bits[6:4]  field buffer select for host read/write
     * Bit[4]  :	Odd/Even Field Select
     * Bits[6:5]: Frame Buffer Select 
     */
    for(ffsel = 0x00; ffsel < 0x7; ffsel++) {
	field_frames_sel = (ffsel << 4) & 0x70;
    
	for(line = 0; line < CC1_LINE_COUNT; line++) {

	    /* Wait awhile between each 128 lines */
	    if ((line & 127) == 0) busy (1);

	    /* Go set up the framebuffer control stuff */
	    _mgv_CC1WriteFrameBufferSetup(mgvbase, field_frames_sel, line);

	    /* Now lets do the write for this line */
	    for(pixels = 0; pixels < CC1_PIXELS_PER_LINE; pixels++){
		dataval = (((line << 16) & 0xFFFF0000) | (pixels & 0xFFFF));
		CC_W4(mgvbase, CC_FRM_BUF_DATA, dataval);
	    }

	    /*
 	     * CC Dram Writes  must  be  multiple of 48 (bytes)
 	     * We have to pad additional 16 bytes 
 	     */
	    CC_W4(mgvbase, CC_FRM_BUF_DATA, pad_buf[0]);
	    CC_W4(mgvbase, CC_FRM_BUF_DATA, pad_buf[1]);
	    CC_W4(mgvbase, CC_FRM_BUF_DATA, pad_buf[2]);
	    CC_W4(mgvbase, CC_FRM_BUF_DATA, pad_buf[3]);
	}
		
	busy(1);

	/*
	 * Read Back and Verify
	 * Read do not need to worry about the padding
	 * Dummy Setup needed for WRITE to READ transition 
	 */

	/* Go set up the framebuffer control stuff */
	_mgv_CC1ReadFrameBufferSetup(mgvbase, 0, 0);
	if (_mgv_CC1WaitForTransferReady(mgvbase)) {
	   errors++;
	   goto __end_addr_bus;
	}

	for(line = 1; line < (CC1_LINE_COUNT-1); line++) {

	    /* Wait awhile between each 128 lines */
	    if ((line & 127) == 0) busy (1);

	    /* Go set up the framebuffer control stuff */
	    _mgv_CC1ReadFrameBufferSetup(mgvbase, field_frames_sel, line);
	    if (_mgv_CC1WaitForTransferReady(mgvbase)) {
		errors++;
		goto __end_addr_bus;
	    }

	    /* Go read the line of this buffer */
	    for(pixels = 0; pixels < CC1_PIXELS_PER_LINE; pixels++){
		exp = (((line << 16) & 0xFFFF0000) | (pixels & 0xFFFF));
		CC_R4(mgvbase, CC_FRM_BUF_DATA, rcv);
		if (rcv != exp)  {
		   msg_printf(ERR, "Frame buffer %d, %s field, Line = 0x%x, \
		      Pixel = 0x%x, exp = 0x%08X rcv = 0x%08X\n", 
		      ((ffsel >> 1) & 3),  ((ffsel & 1) ? "even" : "odd") ,
		      line, pixels, exp, rcv);
		   errors++;
	   	   goto __end_addr_bus;
		}
	    }
	}
    }
    busy(0);

__end_addr_bus:
    return(_mgv_reportPassOrFail((&mgv_err[CC1_DRAM_ADDR_BUS_TEST]) ,errors));
}

/*
 * NAME: mgv_CC1DramDataBusTest
 *
 * FUNCTION: This test is for the databus into the CC1 framebuffer
 *           It writes a walking one pattern into a single line
 *           to test for any shorts/opens.  Once the write is done
 *           a read is performed and the data is verified.
 *
 * INPUTS: none
 *
 * OUTPUTS: errors
 */
__uint32_t
mgv_CC1DramDataBusTest(void)
{
    __uint32_t errors = 0;
    __uint32_t line;
    __uint32_t rcv ;
    __uint32_t ffsel, field_frames_sel, index;

    msg_printf(SUM, "IMPV:- %s\n", mgv_err[CC1_DRAM_DATA_BUS_TEST].TestStr);

     _mgv_CC1TestSetup();  

    /* Select the a field */
    for (ffsel = 0x00; ffsel < 0x7; ffsel++) {
	field_frames_sel = (ffsel << 4) & 0x70;
	line = 100;

	/* Setup the framebuffer control */
	_mgv_CC1WriteFrameBufferSetup(mgvbase, field_frames_sel, line);

	/* Write the data */
	for (index = 0; index < 16; index++) {
	   CC_W4(mgvbase, CC_FRM_BUF_DATA, *(mgv_walk_one + index));
	}

	/*
 	 * We have sent 64 Bytes of data so far.
 	 * Added 32 bytes more, Multiple of 48 
	 */
	for (index = 0; index < 8; index++) {
	    CC_W4(mgvbase, CC_FRM_BUF_DATA, 0xDEADBEEF);
	}

	busy(1);

	/* Get ready to read back the data */
	_mgv_CC1ReadFrameBufferSetup(mgvbase, 0, 0);
	if (_mgv_CC1WaitForTransferReady(mgvbase)) {
	    errors++;
	    goto __end_data_bus;
	}

	/* Read Back and Verify */
	_mgv_CC1ReadFrameBufferSetup(mgvbase, field_frames_sel, line);
	if (_mgv_CC1WaitForTransferReady(mgvbase)) {
	    errors++;
	    goto __end_data_bus;
	}
	for (index = 0; index < 16; index++){
	    CC_R4(mgvbase, CC_FRM_BUF_DATA, rcv);
	    if (rcv  != *(mgv_walk_one + index))  {
	   	msg_printf(ERR, "Frame buffer %d, %s field, Line = 0x%x, \
		    4 byte index = 0x%x, exp = 0x%08X rcv = 0x%08X\n", 
		    ((ffsel >> 1) & 3), ((ffsel & 1) ? "even" : "odd") ,
		    line, index, *(mgv_walk_one + index), rcv);
		errors++;
	    }

	}
    }
    busy(0);

__end_data_bus:
    return(_mgv_reportPassOrFail((&mgv_err[CC1_DRAM_DATA_BUS_TEST]) ,errors));
}


/*
 * NAME: mgv_CC1DramPatrnTest
 *
 * FUNCTION: This test verifies that each cell of the framebuffer
 *           memory can hold a 0 or a 1.  Each field is written and
 *           verified with 3 different data values.
 *
 *                   0x55555555
 *                   0xaaaaaaaa
 *                   0x00000000
 *
 *
 * INPUTS: none
 *
 * OUTPUTS: errors
 */
__uint32_t
mgv_CC1DramPatrnTest(void)
{
    __uint32_t errors = 0;
    __uint32_t line, pixels;
    __uint32_t loop, *patrn, rcv, tmpval ;
    __uint32_t ffsel, field_frames_sel, index;
    __uint32_t pad_buf [4] = {0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF};

    msg_printf(SUM, "IMPV:- %s\n", mgv_err[CC1_DRAM_PATRN_TEST].TestStr);

    line = 0;

    /*** _mgv_CC1TestSetup();  ***/

    /*
     * CC1 direct reg4, bits[6:4]  field buffer select for host read/write
     * Bit[4]  :	Odd/Even Field Select
     * Bits[6:5]: Frame Buffer Select 
     */
    for (ffsel = 0x00; ffsel < 0x7; ffsel++) {
	field_frames_sel = (ffsel << 4) & 0x70;

	/* Loop through the different data values */
	for (loop = 0; loop < 3; loop++) {

	    /* Start writing the lines */
	    for (line = 0, index = 0; line < CC1_LINE_COUNT; line++) {

		/* Wait awhile after every 128 lines */
		if ((line & 127) == 0) busy (1);

		/* Setup the frame buffer control stuff */
		_mgv_CC1WriteFrameBufferSetup(mgvbase, field_frames_sel, line);

		/* Write all the pixels in this line */
		for(pixels = 0; pixels < CC1_PIXELS_PER_LINE; pixels += 4) {

		    /*
		     * Each consecutive pixel gets a different
		     * pattern.  When the test is done, each
		     * pixel gets each data pattern.
		     */
		    patrn = &mgv_patrn[loop];
		    tmpval= *patrn;

		    CC_W4(mgvbase, CC_FRM_BUF_DATA, *(patrn++));
		    CC_W4(mgvbase, CC_FRM_BUF_DATA, *(patrn++));
		    CC_W4(mgvbase, CC_FRM_BUF_DATA, *(patrn++));
		    CC_W4(mgvbase, CC_FRM_BUF_DATA, tmpval);
		}

		/*
		* PAD ...
		* CC Dram Writes  must  be  multiple 
		* of 48 (bytes)
		* We have to pad additional 16 bytes 
		*/
		CC_W4(mgvbase, CC_FRM_BUF_DATA, pad_buf[0]);
		CC_W4(mgvbase, CC_FRM_BUF_DATA, pad_buf[1]);
		CC_W4(mgvbase, CC_FRM_BUF_DATA, pad_buf[2]);
		CC_W4(mgvbase, CC_FRM_BUF_DATA, pad_buf[3]);
	    }

	    busy(1);

	    /* Read Back and Verify */
	    _mgv_CC1ReadFrameBufferSetup(mgvbase, 0, 0);
	    if (_mgv_CC1WaitForTransferReady(mgvbase)) {
		errors++;
		goto __end_patrn;
	    }

	    /* Start reading back line by line */
	    for (line = 0, index = 0; line < CC1_LINE_COUNT; line++) {

	    	/* Wait awhile after each 128 lines */
	    	if ((line & 127) == 0) busy (1);

		/* Setup the frambuffer cotrol stuff */
		_mgv_CC1ReadFrameBufferSetup(mgvbase, field_frames_sel, line);
	    	if (_mgv_CC1WaitForTransferReady(mgvbase)) {
		    errors++;
		    goto __end_patrn;
	    	}
		for (pixels = 0; pixels < CC1_PIXELS_PER_LINE; pixels += 4){
		    patrn = &mgv_patrn[loop];
		    tmpval = *patrn;
		    for (index = 0; index < 3; index++) {

		   	/* Read each pixel and compare */
			CC_R4(mgvbase, CC_FRM_BUF_DATA, rcv);
			if (rcv != *patrn) {
			    msg_printf(ERR, "Frame buffer %d, %s field, Line =\
			    0x%x, Pixel = 0x%x, exp = 0x%08X rcv = 0x%08X\n", 
			    ((ffsel >> 1) & 3), ((ffsel & 1) ? "even" : "odd"),
			    line, pixels + index, *patrn, rcv);
			    errors++;
			}
			++patrn;
		    }
		    CC_R4(mgvbase, CC_FRM_BUF_DATA, rcv);
		    if (rcv != tmpval) {
			msg_printf(ERR, "Frame buffer %d, %s field, Line = \
			0x%x, Pixel = 0x%x, exp = 0x%08X rcv = 0x%08X\n", 
			((ffsel >> 1) & 3), ((ffsel & 1) ? "even" : "odd"),
			line, pixels + index, tmpval, rcv);
			errors++;
		    }
		}
	    }
	}
    }
    busy(0);

__end_patrn:
    return(_mgv_reportPassOrFail((&mgv_err[CC1_DRAM_PATRN_TEST]) ,errors));
}

void
_mgv_CC1TestSetup(void)
{
    unsigned char xp;

    /* set Xpoint sw for REFBLACK -> both CC1 inputs */
    xp = GAL_OUT_CH1_CC1 | GAL_BLACK_REF_IN_SHIFT;
    GAL_IND_W1(mgvbase, GAL_MUX_ADDR, xp);
    xp = GAL_OUT_CH2_CC1 | GAL_BLACK_REF_IN_SHIFT;
    GAL_IND_W1(mgvbase, GAL_MUX_ADDR, xp);

    /* set Genlock reference to be loop-thru */
#if 0
    GAL_IND_W1(mgvbase, GAL_REF_CTRL, 1); 

    /* set default for VCXO free run frequency high and low bytes */
    GAL_IND_W1(mgvbase, GAL_VCXO_LO, LOBYTE(DAC_CENTER));
    GAL_IND_W1(mgvbase, GAL_VCXO_HI, HIBYTE(DAC_CENTER));

    /* set default for black timing */
    GAL_IND_W1(mgvbase, GAL_REF_OFFSET_LO, LOBYTE(BOFFSET_CENTER));
    GAL_IND_W1(mgvbase, GAL_REF_OFFSET_HI, HIBYTE(BOFFSET_CENTER));
#endif

}
