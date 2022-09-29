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
**      FileName:       cc1_memtest.c
**      $Revision: 1.1 $
*/

#include "sys/types.h"
#include "sys/sbd.h"
#include "sys/sema.h"
#include "ide_msg.h"
#include "sys/param.h"
#include "uif.h"
#define HQ3
#include "sys/mgrashw.h"
#include "sys/mgras.h"
#include "mgv1_regio.h"

extern mgras_hw *mgbase;

extern mgv_initialize();

static int wrotefb = 0;

static unsigned int Walk[] = {
         0x11111111,  0x22222222,  0x33333333,  0x44444444,
         0x55555555,  0x66666666,  0x77777777,  0x88888888,
         0x99999999,  0xaaaaaaaa,  0xbbbbbbbb,  0xcccccccc,
        ~0x11111111, ~0x22222222, ~0x33333333, ~0x44444444,
        ~0x55555555, ~0x66666666, ~0x77777777, ~0x88888888,
        ~0x99999999, ~0xaaaaaaaa, ~0xbbbbbbbb, ~0xcccccccc

};

static unsigned int WalkOne[] = {
	0xFFFE0001, 0xFFFD0002, 0xFFFB0004, 0xFFF70008,
	0xFFEF0010, 0xFFDF0020, 0xFFBF0040, 0xFF7F0080,
	0xFEFF0100, 0xFDFF0200, 0xFBFF0400, 0xF7FF0800,
	0xEFFF1000, 0xDFFF2000, 0xBFFF4000, 0x7FFF8000
};


static unsigned int CC1_PatArray[] = {
	0x55555555, 0xAAAAAAAA, 0x00000000, 
	0x55555555, 0xAAAAAAAA, 0x00000000
};

#ifdef HQ3
#define WAIT_FOR_TRANSFER_READY \
{\
        static int tries=0; \
        while ((CC_R1 (mgbase, fbctl) & 0x80) == 0)  {\
                ++tries; \
                if (tries > 10000) {\
                        msg_printf(ERR, "WAIT_FOR_TRANSFER_READY Time out transfer ~ready \n"); \
                        tries = 0; \
                        break;\
                }\
        }\
}
#else
#define WAIT_FOR_TRANSFER_READY \
{\
	static int tries=0; \
	while ((CC_R1 (mgbase, fbctl) & 0x80) == 0)  {\
		++tries; \
		if (tries > 10000) {\
			msg_printf(ERR, "WAIT_FOR_TRANSFER_READY Time out transfer ~ready 0x%0X \n",mgbase->cc1.fbctl); \
			tries = 0; \
			break;\
		}\
	}\
} 
#endif

#define WRITE_CC1_FRAME_BUFFER_SETUP(fld_frm_sel,startline) \
{\
	int fbsel;\
	fbsel = fld_frm_sel >> 1;\
        CC_W1 (mgbase, indrct_addreg, 56 );\
        CC_W1 (mgbase, indrct_datareg, 0x30);\
	CC_W1 (mgbase, fbsel, fbsel<<5 ); \
	CC_W1 (mgbase, fbsel, CC_R1(mgbase, fbsel) | fld_frm_sel & 0x1); \
        CC_W1 (mgbase, indrct_addreg, 49);\
        CC_W1 (mgbase, indrct_datareg, ((startline >> 1)  & 0xFF)); \
        CC_W1 (mgbase, indrct_addreg, 48);\
        CC_W1 (mgbase, indrct_datareg, 0x2);\
	CC_W1 (mgbase, fbctl, (((startline & 0x1) << 5) | 0xC0));\
	CC_W1 (mgbase, fbctl, 0);\
}


#define	READ_CC1_FRAME_BUFFER_SETUP(fld_frm_sel,startline) \
{\
	int fbsel;\
	fbsel = fld_frm_sel >> 1;\
	CC_W1 (mgbase, indrct_addreg, 56);\
	CC_W1 (mgbase, indrct_datareg, 0x0);\
	CC_W1 (mgbase, fbsel, fbsel<<5); \
	CC_W1 (mgbase, fbsel, CC_R1(mgbase, fbsel) | fld_frm_sel & 0x1); \
	CC_W1 (mgbase, indrct_addreg, 49);\
        CC_W1 (mgbase, indrct_datareg, ((startline >> 1)  & 0xFF)); \
	CC_W1 (mgbase, indrct_addreg, 48);\
	CC_W1 (mgbase, indrct_datareg, 0x6);\
	CC_W1 (mgbase, fbctl, (((startline & 0x1) << 5) | 0xC0));\
	CC_W1 (mgbase, fbctl, 0);\
}


#define PIXELS_PER_LINE			1280/4
#define LINE_COUNT			320

#ifdef PAD
#define PIXELS_PER_LINE			1248/4
#define LINE_COUNT			30
#endif

extern unsigned int pad_buf[];

/*
** NAME: CC1_Dram_AddrBus_Test
**
** FUNCTION: This test writes all four framebuffers of the CC1 using
**           an incrementing pattern.  The pattern is a 32 bit word
**           comprising the line number in the upper 16 bits and the
**           pixel number in the lower 16 bits.  Once all four buffers
**           are written, they are read back and verified.
**
** INPUTS: none
**
** OUTPUTS: errcnt -> 0 no errors
**                  > 0 errors
*/
int
CC1_Dram_AddrBus_Test(void)
{
	unsigned int errCnt;
	unsigned int line, pixels, dataval;
	unsigned int loop, exp, rcv ;
	unsigned int ffsel, field_frames_sel;

	errCnt = 0;
	dataval = 0;
	msg_printf(SUM, "CC1 DRAM Address Bus Test\n");
	CC_W1(mgbase, sysctl, 0x48);

	msg_printf(VRB, "CC1 DRAM Address Bus Test - Write\n");
	/*
	** CC1 direct reg4, bits[6:4]  field buffer select for host read/write
	** Bit[4]  :	Odd/Even Field Select
	** Bits[6:5]: Frame Buffer Select 
	*/
	for(ffsel = 0x00; ffsel < 0x7; ffsel++) {
		msg_printf(VRB, "Testing frame buffer %d, %s field\n", ((ffsel >> 1) & 3), ((ffsel & 1) ? "even" : "odd"));
		field_frames_sel = (ffsel << 4) & 0x70;

		for(line = 0; line < LINE_COUNT; line++) {

			/* Wait awhile between each 128 lines */
			if ((line & 127) == 0) 
				busy (1);

			/* Go set up the framebuffer control stuff */
 			WRITE_CC1_FRAME_BUFFER_SETUP(field_frames_sel, line);

			/* Now lets do the write for this line */
			for(pixels = 0; pixels < PIXELS_PER_LINE; pixels++){
				dataval = (((line << 16) & 0xFFFF0000) | (pixels & 0xFFFF));
				CC_W4(mgbase, fbdata, dataval);
			}

			/*
			** CC Dram Writes  must  be  multiple of 48 (bytes)
			** We have to pad additional 16 bytes 
			*/
			CC_W4(mgbase, fbdata, pad_buf[0]);
			CC_W4(mgbase, fbdata, pad_buf[1]);
			CC_W4(mgbase, fbdata, pad_buf[2]);
			CC_W4(mgbase, fbdata, pad_buf[3]);
		}
		
		busy(1);

		/*
		** Read Back and Verify
		** Read do not need to worry about the padding
		** Dummy Setup needed for WRITE to READ transition 
		*/

		/* Go set up the framebuffer control stuff */
		READ_CC1_FRAME_BUFFER_SETUP(0, 0);
		WAIT_FOR_TRANSFER_READY;

		msg_printf(VRB, "CC1 DRAM Address Bus Test - Read Back & Verify\n");
		for(line = 0; line < LINE_COUNT; line++) {

			/* Wait awhile between each 128 lines */
			if ((line & 127) == 0) 
				busy (1);

			/* Go set up the framebuffer control stuff */
			READ_CC1_FRAME_BUFFER_SETUP(field_frames_sel, line);
			WAIT_FOR_TRANSFER_READY;

			/* Go read the line of this buffer */
                        for(pixels = 0; pixels < PIXELS_PER_LINE; pixels++){
				exp = (((line << 16) & 0xFFFF0000) 
				    | (pixels & 0xFFFF));
				rcv = CC_R4(mgbase, fbdata);
                               if (rcv != exp)  {
					msg_printf(ERR, "*** CC1 DRAM Address Bus Test failed\n");
					msg_printf(ERR, "*** Error Code: CC1_0_01\n");
					msg_printf(INFO, "\tFrame buffer %d, %s field, Line = 0x%x, Pixel = 0x%x\n", ((ffsel >> 1) & 3), ((ffsel & 1) ? "even" : "odd") ,line, pixels);
					msg_printf(INFO, "\texp = 0x%08X rcv = 0x%08X\n", exp, rcv);
					msg_printf(INFO, "\tFailed bits 0x%x\n", exp ^ rcv);
				}
			}
		}
	}
	busy(0);

	return(errCnt);
}

/*
** NAME: CC1_Dram_DataBus_Test
**
** FUNCTION: This test is for the databus into the CC1 framebuffer
**           It writes a walking one pattern into a single line
**           to test for any shorts/opens.  Once the write is done
**           a read is performed and the data is verified.
**
** INPUTS: none
**
** OUTPUTS: errcnt -> 0 no errors
**                    >0 errors
*/
int
CC1_Dram_DataBus_Test(void)
{
	unsigned int errCnt;
	unsigned int line, pixels;
        unsigned int loop, *patrn, exp, rcv ;
        unsigned int ffsel, field_frames_sel, index;

	errCnt = 0;
	msg_printf(SUM, "CC1 DRAM Data Bus Test\n");
	line = 0;

	msg_printf(VRB, "CC1 DRAM Data Bus Test - Write\n");
	/* Select the a field */
        for (ffsel = 0x00; ffsel < 0x7; ffsel++) {
		msg_printf(VRB, "Testing frame buffer %d, %s field\n", ((ffsel >> 1) & 3), ((ffsel & 1) ? "even" : "odd"));
               	field_frames_sel = (ffsel << 4) & 0x70;
		line = 100;

		/* Setup the framebuffer control */
		WRITE_CC1_FRAME_BUFFER_SETUP(field_frames_sel, line);

		/* Write the data */
		for (index = 0; index < 16; index++) {
			CC_W4(mgbase, fbdata, *(WalkOne + index));
		}

		/*
		** We have sent 64 Bytes of data so far.
		** Added 32 bytes more, Multiple of 48 
		*/
		for (index = 0; index < 8; index++) {
			CC_W4(mgbase, fbdata, 0xDEADBEEF);
		}

		busy(1);

		/* Get ready to read back the data */
		READ_CC1_FRAME_BUFFER_SETUP(0, 0);
		WAIT_FOR_TRANSFER_READY;

		msg_printf(VRB, "CC1 DRAM Data Bus Test Readback & Verify\n");

		/* Read Back and Verify */
		READ_CC1_FRAME_BUFFER_SETUP(field_frames_sel, line);
		WAIT_FOR_TRANSFER_READY;
		for (index = 0; index < 16; index++){
			rcv = CC_R4(mgbase, fbdata);
			if (rcv  != *(WalkOne + index))  {
				msg_printf(ERR, "*** CC1 DRAM Data Bus Test failed\n");
				msg_printf(ERR, "*** Error Code: CC1_0_02\n");
				msg_printf(INFO, "\tFrame buffer %d, %s field, Line = 0x%x, 4 byte index = 0x%x\n", ((ffsel >> 1) & 3), ((ffsel & 1) ? "even" : "odd") ,line, index);
				msg_printf(INFO, "\texp = 0x%08X rcv = 0x%08X\n", *(WalkOne + index), rcv);
				msg_printf(INFO, "\tFailed bits 0x%x\n", *(WalkOne + index) ^ rcv);
			}

		}
	}
	busy(0);

	return(errCnt);
}


/*
** NAME: CC1_Dram_Data_Test
**
** FUNCTION: This test verifies that each cell of the framebuffer
**           memory can hold a 0 or a 1.  Each field is written and
**           verified with 3 different data values.
**
**                   0x55555555
**                   0xaaaaaaaa
**                   0x00000000
**
**
** INPUTS: none
**
** OUTPUTS: errcnt -> 0 no errors
**                    >0 errors
*/
int
CC1_Dram_Data_Test(void)
{
	unsigned int errCnt;
	unsigned int line, pixels;
	unsigned int loop, *patrn, exp, rcv, tmpval ;
	unsigned int ffsel, field_frames_sel, index;

	errCnt = 0;
	msg_printf(SUM, "CC1 DRAM Data Test\n");
	line = 0;

        msg_printf(VRB, "CC1 DRAM Address Bus Test - Write\n");
	/*
	** CC1 direct reg4, bits[6:4]  field buffer select for host read/write
	** Bit[4]  :	Odd/Even Field Select
	** Bits[6:5]: Frame Buffer Select 
	*/
	for (ffsel = 0x00; ffsel < 0x7; ffsel++) {
		msg_printf(VRB, "Testing frame buffer %d, %s field\n", ((ffsel >> 1) & 3), ((ffsel & 1) ? "even" : "odd"));
		field_frames_sel = (ffsel << 4) & 0x70;

		/* Loop through the different data values */
		for (loop = 0; loop < 3; loop++) {
			msg_printf(VRB, "CC1 DRAM Data Test Writing\n");

			/* Start writing the lines */
                	for (line = 0, index = 0; line < LINE_COUNT; line++) {

				/* Wait awhile after every 128 lines */
				if ((line & 127) == 0) busy (1);

				/* Setup the frame buffer control stuff */
 				WRITE_CC1_FRAME_BUFFER_SETUP(field_frames_sel, line);
				/* Write all the pixels in this line */
                        	for(pixels = 0; pixels < PIXELS_PER_LINE; pixels += 4){

					/*
					** Each consecutive pixel gets a different
					** pattern.  When the test is done, each
					** pixel gets each data pattern.
					*/
					patrn = &CC1_PatArray[loop];
					tmpval= *patrn;

					CC_W4(mgbase, fbdata, *(patrn++));
					CC_W4(mgbase, fbdata, *(patrn++));
					CC_W4(mgbase, fbdata, *(patrn++));
					CC_W4(mgbase, fbdata, tmpval);
				}

				/*
				** PAD ...
                        	** CC Dram Writes  must  be  multiple 
				** of 48 (bytes)
                        	** We have to pad additional 16 bytes 
				*/
                        	CC_W4(mgbase, fbdata, pad_buf[0]);
                        	CC_W4(mgbase, fbdata, pad_buf[1]);
                        	CC_W4(mgbase, fbdata, pad_buf[2]);
                        	CC_W4(mgbase, fbdata, pad_buf[3]);
			}

			busy(1);

			/* Read Back and Verify */
			READ_CC1_FRAME_BUFFER_SETUP(0, 0);
			WAIT_FOR_TRANSFER_READY;

			msg_printf(VRB, "CC1 DRAM Data Test Readback & Verify\n");

			/* Start reading back line by line */
                	for (line = 0, index = 0; line < LINE_COUNT; line++) {

				/* Wait awhile after each 128 lines */
				if ((line & 127) == 0) busy (1);

				/* Setup the frambuffer cotrol stuff */
			        READ_CC1_FRAME_BUFFER_SETUP(field_frames_sel, line);
				WAIT_FOR_TRANSFER_READY;

				/* Start reading back the pixels */
                        	for (pixels = 0; pixels < PIXELS_PER_LINE; pixels += 4){
                                	patrn = &CC1_PatArray[loop];
					tmpval = *patrn;
					for (index = 0; index < 3; index++) {

						/* Read each pixel and compare */
						rcv = CC_R4(mgbase, fbdata);
						if (rcv != *patrn) {
							msg_printf(ERR, "*** CC1 DRAM Data Test failed\n");
							msg_printf(ERR, "*** Error Code: CC1_0_03\n");
							msg_printf(INFO, "\tFrame buffer %d, %s field, Line = 0x%x, Pixel = 0x%x\n", ((ffsel >> 1) & 3), ((ffsel & 1) ? "even" : "odd") ,line, pixels + index);
							msg_printf(INFO, "\texp = 0x%08X rcv = 0x%08X\n", *patrn, rcv);
							msg_printf(INFO, "\tFailed bits 0x%x\n", *patrn ^ rcv);
						}
						++patrn;
					}
					rcv = CC_R4(mgbase, fbdata);
					if (rcv != tmpval) {
						msg_printf(ERR, "*** CC1 DRAM Data Test failed\n");
						msg_printf(ERR, "*** Error Code: CC1_0_03\n");
						msg_printf(INFO, "\tFrame buffer %d, %s field, Line = 0x%x, Pixel = 0x%x\n", ((ffsel >> 1) & 3), ((ffsel & 1) ? "even" : "odd") ,line, pixels + index);
						msg_printf(INFO, "\texp = 0x%08X rcv = 0x%08X\n", tmpval, rcv);
						msg_printf(INFO, "\tFailed bits 0x%x\n", tmpval ^ rcv);
					}
				}
			}
		}
	}
	busy(0);

	return(errCnt);
}

int
mgv1_CC1_AddrBus_Test(int argc, char *argv[])
{
        int errs;

        errs = CC1_Dram_AddrBus_Test();
        return(errs);
}

int
mgv1_CC1_DataBus_Test(int argc, char *argv[])
{
        int errs;

        errs = CC1_Dram_DataBus_Test();
        return(errs);
}

/*
** NAME: mgv1_CC1_Dram_Mem_Test
**
** FUCTION: This routine calls all 3 CC1 memory tests and
**          returns the number of errors.
**
** INPUTS: none
**
** OUTPUTS: errs -> numer of errors found in the tests
*/
int
mgv1_CC1_Dram_Mem_Test(int argc, char *argv[])
{
	int errs; 

	errs = CC1_Dram_AddrBus_Test();
	errs += CC1_Dram_DataBus_Test();
	errs += CC1_Dram_Data_Test();
	return(errs);
}

/*
**
** Frame Buffer Tests
**          Allows for the 4 byte write and read of the CC1 frame buffer:
**          fbsetup (sets up the transfer), fbsetall (writes 48 bytes,
**          which is the smallest unit needed for actual write), fbset4
**          (writes 4 bytes), fbsetrest (writes remaining bytes for full
**          48 bytes read), fbgetsetup (setup for get), fbget4 (gets
**          4 bytes). Typical sequence would be: fbsetup, fbset4 0x12345678,
**          fbsetrest, fbgetsetup, fbget4.
**
** jfg 5/95
*/


/*
** NAME: fbsetsetup
**
** FUNCTION: This routine sets up the framebuffer
**           for writes to line zero, field zero.
**
** INPUTS: none
**
** OUTPUTS: Returns to zero to conform with the function call type
*/
int 
fbsetsetup(void)
{
        CC_W1(mgbase, sysctl, 0x48);
        WRITE_CC1_FRAME_BUFFER_SETUP(0,0);
	return(0);
}

/*
** NAME: fbsetall
**
** FUCNTION: This routine writes 48 bytes of incrementing
**           data starting at line 0, field 0
**
** INPUTS: none
**
** OUTPUTS: none
*/
void 
fbsetall(void)
{
	unsigned int val;
	int	i;

        CC_W1(mgbase, sysctl, 0x48);
        WRITE_CC1_FRAME_BUFFER_SETUP(0,0);

	for (i = 0; i < 12; i++) {
		CC_W4(mgbase, fbdata, Walk[i]);
		msg_printf(SUM, "Wrote FB  0x%x \n", Walk[i]);
	}
}

/*
** NAME: fbset4
**
** FUNCTION: This routine writes 4 bytes of data at the
**           previously set address.
**
** INPUTS: write_data -> data to be written
**
** OUTPUTS: none
*/
void
fbset4(int argc, char **argv)
{
        unsigned int val;

        if (argc < 2) {
                msg_printf(SUM, "Please provide hex value.\n");
                return;
        }

        atobu(argv[1],&val);

        CC_W4 (mgbase, fbdata, val);
        wrotefb++;
        msg_printf(SUM, "Wrote FB  0x%x \n", val);
}

/*
** NAME: fbsetrest
**
** FUNCTION: This routine writes the rest of the 48 bytes
**           required with junk data.
**
** INPUTS: none
**
** OUTPUTS: none
*/
void 
fbsetrest(void)
{
	unsigned int junk = 0xafafafaf;
	int i;

	for (i = wrotefb; i < 12; i++)
		CC_W4 (mgbase, fbdata, junk);
	msg_printf(SUM, "Wrote FBs other %d bytes with value 0x%x\n", 
					(48 - (4 * wrotefb)), junk);
	wrotefb = 0;
}

/*
** NAME: fbgetsetup
**
** FUNCTION: This routine sets up the frane buffer to 
**           reads from line 0, field 0
**
** INPUTS: none
**
** OUTPUTS: none
*/
void 
fbgetsetup(void)
{
        CC_W1 (mgbase, sysctl, 0x48);
        READ_CC1_FRAME_BUFFER_SETUP(0, 0);
        WAIT_FOR_TRANSFER_READY;

}

/*
** NAME: fbget4
**
** FUNCTION: This routine reads 4 bytes of data from the
**           previously set address.
**
** INPUTS: write_data -> data to be written
**
** OUTPUTS: none
*/
void 
fbget4(void)
{
	unsigned int rcv;

        WAIT_FOR_TRANSFER_READY;
        rcv = CC_R4 (mgbase, fbdata);
        msg_printf(SUM, "Read FB  0x%x \n", rcv);
}

