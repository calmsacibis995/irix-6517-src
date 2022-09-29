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
**      FileName:       ab1_memtest.c
**      $Revision: 1.2 $
*/

#include "sys/types.h"
#include "sys/sbd.h"
#include "sys/sema.h"
#include "ide_msg.h"
#include "sys/param.h"
#include "uif.h"
#include "sys/mgrashw.h"
#include "sys/mgras.h"
#include "mgv1_regio.h"

#define HQ3

/*
** External Variables and Function Declarations
*/

extern mgras_hw *mgbase;
extern mgv_initialize();
extern int gv_w1(mgras_hw *base, int, int, int, int);
extern int sprintf(char *, const char *, ...);

/*
** Local Variables and Function Declarations
*/

#define 	MAX_TRYCNT	1

#define		RED		0x0
#define		GREEN		0x8
#define		BLUE		0x10

#define 	AB1_ADDR_SIZE	0xFFFF

static int wrotefb = 0;
unsigned int row, column;
unsigned int wroteab = 0;
unsigned int readab = 0;
unsigned int wroteab1 = 0;
unsigned int readab1 = 0;

unsigned int basebuf[] = {
	0x00010001, 0x00020002, 0x00030003, 0x00040004,
	0x00050005, 0x00060006, 0x00070007, 0x00080008,
	0x00090009, 0x000A000A, 0x000B000B, 0x000C000C,
	0x000D000D, 0x000D000E, 0x000F000F, 0x00100010
};

static unsigned int Walk[] = {
         0x11111111,  0x22222222,  0x33333333,  0x44444444,
         0x55555555,  0x66666666,  0x77777777,  0x88888888,
         0x99999999,  0xaaaaaaaa,  0xbbbbbbbb,  0xcccccccc,
        ~0x11111111, ~0x22222222, ~0x33333333, ~0x44444444,
        ~0x55555555, ~0x66666666, ~0x77777777, ~0x88888888,
        ~0x99999999, ~0xaaaaaaaa, ~0xbbbbbbbb, ~0xcccccccc

};

extern unsigned int WalkOne[];
/* = {
	0xFFFE0001, 0xFFFD0002, 0xFFFB0004, 0xFFF70008,
	0xFFEF0010, 0xFFDF0020, 0xFFBF0040, 0xFF7F0080,
	0xFEFF0100, 0xFDFF0200, 0xFBFF0400, 0xF7FF0800,
	0xEFFF1000, 0xDFFF2000, 0xBFFF4000, 0x7FFF8000
}; */

extern unsigned int AB1_PatArray[];
/* = {
	0x55550000, 0xAAAA5555, 0x0000AAAA, 
	0x55550000, 0xAAAA5555, 0x0000AAAA
}; */

extern unsigned int CC1_PatArray[];
/*  = {
	0x55555555, 0xAAAAAAAA, 0x00000000, 
	0x55555555, 0xAAAAAAAA, 0x00000000
}; */

/*
** Macro Definitions
*/

 
#define MGV1_CHANNEL_ID(channel, RGBChannel) \
{\
	switch(channel) { \
		case RED: \
			sprintf(RGBChannel, "RED CHANNEL"); \
			break;\
		case GREEN:\
			sprintf(RGBChannel, "GREEN CHANNEL"); \
			break;\
		case BLUE:\
			sprintf(RGBChannel, "BLUE CHANNEL"); \
			break;\
	}\
}



/*
** NAME: mgv1_freeze_all_ABs
**
** FUNCTION: This routine freezes the transfer of data 
**           from graphics to video for all 3 windows.
**
** INPUTS: none
**
** OUTPUTS: none
*/
void
mgv1_freeze_all_ABs(void)
{
        AB_W1(mgbase, high_addr, 0);
        AB_W1(mgbase, low_addr, 6);
        AB_W1(mgbase, intrnl_reg, 0x3);
        AB_W1(mgbase, low_addr, 0x16);
        AB_W1(mgbase, intrnl_reg, 0x3);
        AB_W1(mgbase, low_addr, 0x26);
        AB_W1(mgbase, intrnl_reg, 0x1);
}

/*
** NAME: Ab1_Dram_AddrBus_Test
**
** FUNCTION: This routine tests the specified AB1 DRAM address
**           bus.  The data written is an incrementing data
**           pattern.
**
** INPUTS: channel -> R, G, B or A channel
**
**                    RED             0x0
**                    GREEN           0x8
**                    BLUE            0x10
**
** OUTPUTS: errcnt -> number of errors found during test
*/
static int
Ab1_Dram_AddrBus_Test(int channel)
{
	unsigned int i, loop, row, column, rcv;
	unsigned int index, errCnt, addr_range;
	unsigned int *dataptr, databuf[16];
	char RGBChannel[20];
	char err_cd[10];
	char err_cd1[10];
	int trycnt;

	/* Set error count to zero */
	errCnt = 0;

	/* Fetch the ascii string for the channel being tested */
	MGV1_CHANNEL_ID(channel, RGBChannel);

	/* Setup the correct error code for the specified channel */
	if (channel == RED) {
		sprintf(err_cd, "AB1_0_02");
		sprintf(err_cd1, "AB1_0_03");
	} else if (channel == GREEN) {
		sprintf(err_cd, "AB1_1_02");
		sprintf(err_cd1, "AB1_1_03");
	} else if (channel == BLUE) {
		sprintf(err_cd, "AB1_2_02");
		sprintf(err_cd1, "AB1_2_03");
	}

	/* Print message as to what test is being executed */
	msg_printf(SUM, "%s AB1 DRAM Address Bus Test\n", RGBChannel);

	/* Copy data over to different buffer */
	bcopy(basebuf, databuf, sizeof(basebuf));
 	dataptr = &databuf[0];

        /* Select the desired AB1 channel, also diagnostic mode  */
        AB_W1 (mgbase, sysctl, channel);

	/* Write the data to the selected AB1 DRAM */
	for (addr_range = 0x0000; addr_range < AB1_ADDR_SIZE;) {

		/* Every 4096 bytes, print a debug message */
		if ((addr_range & 0xFFF) == 0) {
			busy (1);
			msg_printf(VRB, "\n%s AB1 DRAM Address Bus Test Writing %08X Address region\n\n" , RGBChannel, addr_range);
		}

		/*
		** Each one of the following loops, writes 4
		** words to AB1 memory
		*/
		for (loop = 0; loop < 0x4; loop++, addr_range++) {

			/* Set the addres in the registers */
			row = addr_range >> 7;
			column = addr_range & 0x7F;

			/* (8 LSBs of row address ) */
			AB_W1(mgbase, high_addr, row >> 1);
			/* MSB of row, 7 Bits of column */
			AB_W1(mgbase, low_addr, (((row & 0x1) << 7) | 
						(column & 0x7F)));

			/* Write to the AB1 external dRam, 4 times */
			for (i = 0; i < 0x4; i++) {
				AB_W4(mgbase, dRam, *dataptr);
				dataptr++;
			}
		}

		/* Increment the data values and write some more */
		dataptr = &databuf[0];
		for (index = 0; index < 16; index++)
			*(dataptr + index) += 0x00010001;
	}

	/* Wait here for a second, for some reason */
	busy(1);

	/* ReadBack and verify */
	msg_printf(VRB, "%s AB1 DRAM Address Bus Test Readback & Verify\n", 
						RGBChannel);

	/* Reinitialize data buffer */
	bcopy(basebuf, databuf,sizeof(basebuf));
 	dataptr = &databuf[0];

	/* Read back the AB1 DRAM and verify */
        for (addr_range = 0x0000; addr_range < AB1_ADDR_SIZE;) {

		/* Every 4096 bytes, print a debug message */
		if ((addr_range & 0xFFF) == 0) {
			busy (1);
			msg_printf(DBG, "%s AB1 DRAM Address Bus Test Reading %08X Address region\n" , RGBChannel, addr_range);
		}

		/*
                ** Each one of the following loops, reads 4
                ** words from AB1 memory
                */
		for (loop = 0; loop < 0x4; loop++, addr_range++) {

			/* Setup the address */
			row = addr_range >> 7;
			column = addr_range & 0x7F;

			/*
			** HACK!!! try at least 5 times to read the 1st word
			** This is trying to fix a crosstalk problem happened
			** in read back from AB Dram.
			*/
			for (trycnt = 0; trycnt < MAX_TRYCNT; trycnt++) {

				/* (8 LSBs of row address ) */
				AB_W1 (mgbase, high_addr, row >> 1);
				/* MSB of row, 7 Bits of column */
				AB_W1 (mgbase, low_addr, (((row & 0x1) << 7) | (column & 0x7F)));

				/* Read the first word and verify it */
				rcv = AB_R4 (mgbase, dRam);

				/*
				** If the word is correct, break from
				** the current for loop and continue with
				** the other three words
				*/
				if (rcv == *dataptr) 
					break;
			}

			/* If retries were exhausted, the test failed */
			if (trycnt == MAX_TRYCNT) {
			
				msg_printf(ERR, "*** %s AB1 DRAM Address Bus Test failed\n", RGBChannel);
				msg_printf(ERR, "*** Error Code: %s\n", err_cd);
				msg_printf(INFO, "\tFailed Addrs = 0x%08X, Expected 0x%x, Actual 0x%x\n", addr_range, *dataptr, rcv);
				msg_printf(INFO, "\tFailed bits 0x%x\n", *dataptr ^ rcv);
			}

			/* Proceed to next valid data */
			++dataptr;

			/* Read and verify the next three words */
			for (i = 1; i < 4; i++) {

				/* Perform the read */
				rcv = AB_R4 (mgbase, dRam);

				/* If the data is not good, print error message */
				if (rcv != *dataptr) {
					msg_printf(ERR, "*** %s AB1 DRAM Address Bus Test failed\n", RGBChannel);
					msg_printf(ERR, "*** Error Code: %s\n", err_cd1);
					msg_printf(INFO, "\tFailed Addrs = 0x%08X, Expected 0x%x, Actual 0x%x\n", addr_range, *dataptr, rcv);
					msg_printf(INFO, "\tFailed bits 0x%x\n", *dataptr ^ rcv);
				} 

				/* Proceed to next valid data */
				++dataptr;
			}
		}

		/*
		** Update to next set of expected values 
		** and read some more
		*/
		dataptr = &databuf[0];
		for (index = 0; index < 16; index++)
			*(dataptr + index) += 0x00010001;

	}
	busy(0);

	return(errCnt);
}

/*
** NAME: Ab1_Dram_DataBus_Test
**
** FUNCTION: This routine tests the DRAM databus of the 
**           specified channel using a walking ones and zeros.
**
** INPUTS: channel -> R, G, B or A channel
**
**                    RED             0x0
**                    GREEN           0x8
**                    BLUE            0x10
**
** OUTPUTS: errcnt -> number of errors found during test
*/
static int
Ab1_Dram_DataBus_Test(int channel)
{

	unsigned int i, j, *dataptr, rcv, errCnt;
	unsigned int row, column, addr_range;
	unsigned int cur_row, cur_column, rcv2[4];
	unsigned int loop = 0;
	char RGBChannel[20];
	char err_cd[10];
        char err_cd1[10];
        int trycnt;

        /* Set error count to zero */
        errCnt = 0;

        /* Fetch the ascii string for the channel being tested */
	MGV1_CHANNEL_ID(channel, RGBChannel);

	/* Setup the correct error code for the specified channel */
        if (channel == RED) {
                sprintf(err_cd, "AB1_0_04");
                sprintf(err_cd1, "AB1_0_05");
        } else if (channel == GREEN) {
                sprintf(err_cd, "AB1_1_04");
                sprintf(err_cd1, "AB1_1_05");
        } else if (channel == BLUE) {
                sprintf(err_cd, "AB1_2_04");
                sprintf(err_cd1, "AB1_2_05");
        }
	dataptr = &WalkOne[0];

	/* Select the desired AB1 channel, also diagnostic mode  */
	AB_W1 (mgbase, sysctl, channel);
	msg_printf(SUM, "%s AB1 DRAM Data Bus Test\n", RGBChannel);

	/*
	** Each one of the following loops, writes 4
	** words to AB1 memory
	*/
	for (addr_range = 0; addr_range < 0x10; ++addr_range, loop += 4) {

		/* Set the address in the registers */
		row = addr_range >> 7;
		column = addr_range & 0x7F;

		/* (8 LSBs of row address ) */
		AB_W1(mgbase, high_addr, row >> 1);
		/* MSB of row, 7 Bits of column */
		AB_W1(mgbase, low_addr, (((row & 0x1) << 7) | (column & 0x7F)));

		dataptr = &WalkOne[loop & 0xF];

		/* Write to the AB1 external dRam  */
		for (i = 0; i < 0x4; i++){
			AB_W4(mgbase, dRam, *dataptr++);
		}
	}

	/* Wait here for a second */
	busy(1);
	loop = 0;

	/* ReadBack and verify */
	msg_printf(VRB, "%s AB1 DRAM Data Bus Test ReadBack & verify\n", RGBChannel);

	/* Read back the AB1 DRAM and verify */
        for (addr_range = 0; addr_range < 0x10; ++addr_range, loop += 4) {

		/* Setup the address */
                row = addr_range >> 7;
                column = addr_range & 0x7F;

		dataptr = &WalkOne[loop & 0xF];

		/*
		** HACK!!! try at least 5 times to read the 1st word 
		** This is trying to fix a crosstalk problem happened
		** in read back from AB Dram.
		*/
                for (trycnt = 0; trycnt < MAX_TRYCNT; trycnt++) {

                	/* (8 LSBs of row address ) */
			AB_W1 (mgbase, high_addr, row >> 1);
			/* MSB of row, 7 Bits of column */
			AB_W1 (mgbase, low_addr, (((row & 0x1) << 7) | (column & 0x7F)));
			rcv = AB_R4 (mgbase, dRam);
			if (rcv == *dataptr)
				break;
                }

		/* If retries were exhausted, the test failed */
                if (trycnt == MAX_TRYCNT) {
			msg_printf(ERR, "*** %s AB1 DRAM Data Bus Test failed\n", RGBChannel);
			msg_printf(ERR, "*** Error Code: %s\n", err_cd);
			msg_printf(INFO, "\tFailed Addrs = 0x%08X, Expected 0x%x, Actual 0x%x\n", addr_range, *dataptr, rcv);
			msg_printf(INFO, "\tFailed bits 0x%x\n", *dataptr ^ rcv);
                }
                ++dataptr;

		/* Read and verify the next three words */
                for (i = 1; i < 4; i++) {

			/* Perform the read */
			rcv = AB_R4 (mgbase, dRam) ;

                        msg_printf(DBG,"%s Ab1_Dram_DataBus_Test Addrs= 0x%x Exp=0x%x Rcv=0x%x\n" ,RGBChannel, addr_range, *dataptr, rcv);

			/* If the data is not good, print error message */
			if (rcv != *dataptr) {
				msg_printf(ERR, "*** %s AB1 DRAM Data Bus Test failed\n", RGBChannel);
				msg_printf(ERR, "*** Error Code: %s\n", err_cd1);
				msg_printf(INFO, "\tFailed Addrs = 0x%08X, Expected 0x%x, Actual 0x%x\n", addr_range, *dataptr, rcv);
				msg_printf(INFO, "\tFailed bits 0x%x\n", *dataptr ^ rcv);
			} else 
				++dataptr;
		}
	}

	busy(0);

	/* Return the error count */
	return(errCnt);
}

/*
** NAME: Ab1_Patrn_Test
**
** FUNCTION: This test performs a data pattern test on each
**           memory cell.  The data values used are
**
**			0x55550000
**			0xAAAA5555
**			0x0000AAAA
**
**           The test is looped three times to make sure that
**           each memory cell can hold a 0 or a 1.
**
** INPUTS: channel -> channel to be tested
**			0x00 -- RED
**                      0x08 -- GREEN
**                      0x10 -- BLUE
**
** OUTPUTS: errcnt -> 0 -- no errors
**                   >0 -- errors
*/
static int
Ab1_Patrn_Test(int channel)
{
	/* Dram Patrn Test */
	
	unsigned int loop, errCnt;
        unsigned int i, rcv, *dataptr;
        int row, column, addr_range;
	char RGBChannel[20];
	char err_cd[10];
        char err_cd1[10];
        int trycnt;

        /* Set error count to zero */
        errCnt = 0;

        /* Fetch the ascii string for the channel being tested */
        MGV1_CHANNEL_ID(channel, RGBChannel);

        /* Setup the correct error code for the specified channel */
        if (channel == RED) {
                sprintf(err_cd, "AB1_0_06");
                sprintf(err_cd1, "AB1_0_07");
        } else if (channel == GREEN) {
                sprintf(err_cd, "AB1_1_06");
                sprintf(err_cd1, "AB1_1_07");
        } else if (channel == BLUE) {
                sprintf(err_cd, "AB1_2_06");
                sprintf(err_cd1, "AB1_2_07");
        }

        /* Select the desired AB1 channel, also diagnostic mode  */
        AB_W1 (mgbase, sysctl, channel);

	/* Print message as to what test is being executed */
	msg_printf(SUM, "%s AB1 Pattern Test\n", RGBChannel);

	/* Loop through the data array patterns */
	for (loop = 0; loop < 3; loop++) {

		/* Write this series of patterns into memory */
        	for (addr_range = 0; addr_range < AB1_ADDR_SIZE;) {

			/* Every 4096 bytes, wait awhile */
	  		if ((addr_range & 0xFFF) == 0) {
				busy (1);
				msg_printf(VRB, "%s AB1 Pattern Test  Writing 0x%08X Addr region\n" , RGBChannel, addr_range);
			}

			/* Setup the next 16 byte address range */
                	row = addr_range >> 7;
                	column = addr_range & 0x7F;
	
			/* (8 LSBs of row address ) */
                	AB_W1(mgbase, high_addr, row >> 1); 
			/* MSB of row, 7 Bits of column */
                	AB_W1(mgbase, low_addr, (((row & 0x1) << 7) | (column & 0x7F)));
	
			/* Reset the data ptr */
       			dataptr = &AB1_PatArray[loop];

                	/* Write to the AB1 external dRam  */
                	for (i = 0; i < 0x4; i++) {
                        	AB_W4(mgbase, dRam, *(dataptr++));
			}

			/* Increment the address range */
			++addr_range;
        	}

		busy(1);

        	/* ReadBack and verify */
		msg_printf(DBG, "%s AB1 Pattern Test Readback & Verify\n", RGBChannel);

		/* Initialize the starting address and read and verify memory */
        	for (addr_range = 0; addr_range < AB1_ADDR_SIZE;) {

			/* After every 4096 bytes, wait awhile */
	  		if ((addr_range & 0xFFF) == 0) {
				busy (1);
				msg_printf(DBG, "%s AB1 Pattern Test  Reading 0x%08X Address region\n" , RGBChannel, addr_range);
			}

			/* Init the address */
               		row = (addr_range) >> 7;
               		column = (addr_range) & 0x7F;

			/* Setup the data pattern */
			dataptr = &AB1_PatArray[loop];  

			/*
			** HACK!!! try at least 5 times to read the 1st word 
		 	** This is trying to fix a crosstalk problem happened
		 	** in read back from AB Dram.
		 	*/
                    	for (trycnt = 0; trycnt < MAX_TRYCNT; trycnt++) {
                    		/* (8 LSBs of row address ) */
                    		AB_W1(mgbase, high_addr, row >> 1);
                    		/* MSB of row, 7 Bits of column */
                    		AB_W1(mgbase, low_addr, (((row & 0x1) << 7) | (column & 0x7F)));
                    		rcv = AB_R4(mgbase, dRam);
                    		if (rcv == *dataptr)
                        		break;
                	}
                	if (trycnt == MAX_TRYCNT) {
				msg_printf(ERR, "*** %s AB1 Pattern Test failed\n", RGBChannel);
				msg_printf(ERR, "*** Error Code: %s\n", err_cd);
				msg_printf(INFO, "\tFailed Addrs = 0x%08X, Expected 0x%x, Actual 0x%x\n", addr_range, *dataptr, rcv);
				msg_printf(INFO, "\tFailed bits 0x%x\n", *dataptr ^ rcv);
			}
                	++dataptr;

			/* Verify the rest of the 16 bytes */
                	for (i = 1; i < 4; i++) {
                        	rcv = AB_R4(mgbase, dRam) ;
				if (rcv != *dataptr) {
					msg_printf(ERR, "*** %s AB1 Pattern Test failed\n", RGBChannel);
					msg_printf(ERR, "*** Error Code: %s\n", err_cd1);
					msg_printf(INFO, "\tFailed Addrs = 0x%08X, Expected 0x%x, Actual 0x%x\n", addr_range, *dataptr, rcv);
					msg_printf(INFO, "\tFailed bits 0x%x\n", *dataptr ^ rcv);
				} 
                         	++dataptr;
                	}
			++addr_range;
        	}
	}
	busy (0);

        /* Return the error count */
        return(errCnt);
}


/*
** NAME: Ab1_Patrn_Test
**
** FUNCTION: This test performs a data pattern test on each
**           memory cell.  The data values used are
**
**                      0x55550000
**                      0xAAAA5555
**                      0x0000AAAA
**
**           The test is looped three times to make sure that
**           each memory cell can hold a 0 or a 1.
**           The difference between this test and the previous one
**           is this test writes 16 bytes and reads them back.  The
**           previous one writes the whole memory then reads them
**           back.
**
** INPUTS: channel -> channel to be tested
**                      0x00 -- RED
**                      0x08 -- GREEN
**                      0x10 -- BLUE
**
** OUTPUTS: errcnt -> 0 -- no errors
**                   >0 -- errors
*/
static int
Patrn_Test(int channel)
{
	unsigned int loop, errCnt;
        unsigned int i, rcv, *dataptr;
        int row, column, addr_range;
	char RGBChannel[20];
	char err_cd[10];
        char err_cd1[10];
        int trycnt;

        /* Set error count to zero */
        errCnt = 0;

        /* Fetch the ascii string for the channel being tested */
        MGV1_CHANNEL_ID(channel, RGBChannel);

        /* Setup the correct error code for the specified channel */
        if (channel == RED) {
                sprintf(err_cd, "AB1_0_08");
                sprintf(err_cd1, "AB1_0_09");
        } else if (channel == GREEN) {
                sprintf(err_cd, "AB1_1_08");
                sprintf(err_cd1, "AB1_1_09");
        } else if (channel == BLUE) {
                sprintf(err_cd, "AB1_2_08");
                sprintf(err_cd1, "AB1_2_09");
        }

        /* Select the desired AB1 channel, also diagnostic mode  */
        AB_W1 (mgbase, sysctl, channel);

	/* Print message as to what test is being executed */
        msg_printf(SUM, "%s AB1 Section Pattern Test\n", RGBChannel);

	/* Loop through all the data patterns in the array */
	for (loop = 0; loop < 3; loop++) {

		/* Write this series of patterns into memory */
        	for (addr_range = 0; addr_range < AB1_ADDR_SIZE;) {

			/* Every 4096 bytes, wait awhile */
	  		if ((addr_range & 0xFFF) == 0) {
				busy (1);
				msg_printf(VRB, "%s AB1 Section Pattern Test  Writing 0x%08X Addr region\n" , RGBChannel, addr_range);
			}

			/* Setup the next address */
                	row = addr_range >> 7;
                	column = addr_range & 0x7F;
	
			/* (8 LSBs of row address ) */
                	AB_W1 (mgbase, high_addr, row >> 1); 
			/* MSB of row, 7 Bits of column */
                	AB_W1 (mgbase, low_addr, (((row & 0x1) << 7) | (column & 0x7F)));
	
			/* Reset the data ptr */
       			dataptr = &AB1_PatArray[loop];

                	/* Write to the AB1 external dRam  */
                	for (i = 0; i < 0x4; i++) {
                        	AB_W4 (mgbase, dRam, *(dataptr++));
			}

			busy(1);

			/* ReadBack and verify */
			msg_printf(DBG, "%s AB1 Section Pattern Test Readback & Verify\n", RGBChannel);
			/* Reset the data ptr */
			dataptr = &AB1_PatArray[loop];  

			/*
			** HACK!!! Try at least 5 time to read 1st word
		 	** This is trying to fix a crosstalk problem happened
		 	** in read back from AB Dram.
		 	*/
        		/* ReadBack and verify */
                	for (trycnt = 0; trycnt < MAX_TRYCNT; trycnt++) {

                    		/* (8 LSBs of row address ) */
                    		AB_W1 (mgbase, high_addr, row >> 1);
                    		/* MSB of row, 7 Bits of column */
                    		AB_W1 (mgbase, low_addr, (((row & 0x1) << 7) | (column & 0x7F)));
                    		rcv = AB_R4 (mgbase, dRam);
                    		if (rcv == *dataptr)
					break;
                	}
                	if (trycnt == MAX_TRYCNT) {
				msg_printf(ERR, "*** %s AB1 Section Pattern Test failed\n", RGBChannel);
                                msg_printf(ERR, "*** Error Code: %s\n", err_cd);                                msg_printf(INFO, "\tFailed Addrs = 0x%08X, Expected 0x%x, Actual 0x%x\n", addr_range, *dataptr, rcv);
                                msg_printf(INFO, "\tFailed bits 0x%x\n", *dataptr ^ rcv);
                	}
                	++dataptr;

			/* Read and verify the rest of the 16 byte group */
                	for (i = 1; i < 4; i++) {
                        	rcv = AB_R4(mgbase, dRam) ;
				if (rcv != *dataptr) {
					msg_printf(ERR, "*** %s AB1 Section Pattern Test failed\n", RGBChannel);
                                        msg_printf(ERR, "*** Error Code: %s\n",
err_cd1);
                                        msg_printf(INFO, "\tFailed Addrs = 0x%08X, Expected 0x%x, Actual 0x%x\n", addr_range, *dataptr, rcv);
                                        msg_printf(INFO, "\tFailed bits 0x%x\n", *dataptr ^ rcv);
				} 
                         	++dataptr;
                	}
			++addr_range;
        	}
	}
	busy (0);

        /* Return the error count */
        return(errCnt);
}

/*
** NAME: Channel_Mem_Test
**
** FUNCTION: The routine calls all three types of memory
**           tests for a specific channel.
**
** INPUTS: channel -> channel to be tested
**                      0x00 -- RED
**                      0x08 -- GREEN
**                      0x10 -- BLUE
**
** OUTPUTS: errs -> total of errors found from all tests
*/
int
Channel_Mem_Test(int channel)
{
	int errs; 

	errs = Ab1_Dram_AddrBus_Test(channel);
	errs += Ab1_Dram_DataBus_Test(channel);
	errs += Ab1_Patrn_Test(channel);
        return(errs);
}

/*
** NAME: Channel_Mem_Test
**
** FUNCTION: The routine calls the full memory test
**           for all three channels.
**
** INPUTS: none
**
** OUTPUTS: errs -> total of errors found from all tests
*/
int
mgv1_AB1_Full_MemTest(int argc, char *argv[])
{
	int errs; 

	errs = Channel_Mem_Test(RED);
	errs += Channel_Mem_Test(GREEN);
	errs += Channel_Mem_Test(BLUE);
	return(errs);
}

/*
** The following routines are used by IDE to call the memory tests for
** the AB1's.
*/

int
mgv1_RedChannel_AddrBus_Test(int argc, char *argv[])
{
	int errs; 

	errs = Ab1_Dram_AddrBus_Test(RED);
        return(errs);
}

int
mgv1_RedChannel_Data_Test(int argc, char *argv[])
{
	int errs; 

	errs = Ab1_Dram_DataBus_Test(RED);
	errs += Ab1_Patrn_Test(RED);
        return(errs);
}


int
mgv1_RedChannel_DataBus_Test(int argc, char *argv[])
{
	int errs; 

	errs = Ab1_Dram_DataBus_Test(RED);
        return(errs);
}

int
mgv1_RedChannel_Patrn_Test(int argc, char *argv[])
{
	int errs; 

	errs = Ab1_Patrn_Test(RED);
        return(errs);
}

int
mgv1_RedChannel_Patrn(int argc, char *argv[])
{
	int errs; 

	errs = Patrn_Test(RED);
        return(errs);
}

int
mgv1_GreenChannel_Patrn(int argc, char *argv[])
{
	int errs; 

	errs = Patrn_Test(GREEN);
        return(errs);
}

int
mgv1_BlueChannel_Patrn(int argc, char *argv[])
{
	int errs; 

	errs = Patrn_Test(BLUE);
        return(errs);
}

int
mgv1_RedChannel_Mem_Test(int argc, char *argv[])
{
	int errs; 
	errs = Ab1_Dram_AddrBus_Test(RED);
	errs += Ab1_Dram_DataBus_Test(RED);
	errs += Ab1_Patrn_Test(RED);
        return(errs);
}

int
mgv1_GreenChannel_AddrBus_Test(int argc, char *argv[])
{
	int errs; 

	errs = Ab1_Dram_AddrBus_Test(GREEN);
        return(errs);


}

int
mgv1_GreenChannel_Data_Test(int argc, char *argv[])
{
	int errs; 

	errs = Ab1_Dram_DataBus_Test(GREEN);
	errs += Ab1_Patrn_Test(GREEN);
        return(errs);
}

int
mgv1_GreenChannel_DataBus_Test(int argc, char *argv[])
{
	int errs; 

	errs = Ab1_Dram_DataBus_Test(GREEN);
        return(errs);
}

int
mgv1_GreenChannel_Patrn_Test(int argc, char *argv[])
{
	int errs; 

	errs = Ab1_Patrn_Test(GREEN);
        return(errs);
}

int
mgv1_GreenChannel_Mem_Test(int argc, char *argv[])
{
	int errs; 

	errs = Ab1_Dram_AddrBus_Test(GREEN);
	errs += Ab1_Dram_DataBus_Test(GREEN);
	errs += Ab1_Patrn_Test(GREEN);
        return(errs);
}


int
mgv1_BlueChannel_AddrBus_Test(int argc, char *argv[])
{
	int errs; 

	errs = Ab1_Dram_AddrBus_Test(BLUE);
        return(errs);
}

int
mgv1_BlueChannel_Patrn_Test(int argc, char *argv[])
{
	int errs; 

	errs = Ab1_Patrn_Test(BLUE);
        return(errs);
}

int
mgv1_BlueChannel_Data_Test(int argc, char *argv[])
{
	int errs; 

	errs = Ab1_Dram_DataBus_Test(BLUE);
	errs +=Ab1_Patrn_Test(BLUE);
        return(errs);
}

int
mgv1_BlueChannel_DataBus_Test(int argc, char *argv[])
{
	int errs; 

	errs = Ab1_Dram_DataBus_Test(BLUE);
        return(errs);
}

int
mgv1_BlueChannel_Mem_Test(int argc, char *argv[])
{
	int errs; 

	errs = Ab1_Dram_AddrBus_Test(BLUE);
	errs += Ab1_Dram_DataBus_Test(BLUE);
	errs += Ab1_Patrn_Test(BLUE);
        return(errs);
}


/*
**
** AB Memory Individual Access Tests
**          Allows for the 4 byte write and read of AB1 memory:
**          absetdiag (r|g|b|a) which sets up the transfer,
**          abset4 (writes 4 bytes), absetrest (writes remaining bytes for full
**          16 byte write), abget4 (gets 4 bytes), abgetrest (gets what
**          remains of 16 bytes. If no column, row is given; then 0,0 is
**          default.  In set commands value comes before row column.
**          1 byte set and get are written but not tested.
**
**  jfg 5/95
*/



/*
** NAME: ABSetDiag
**
** FUNCTION: This routine selects the specified AB1 channel
**
** INPUTS: argc, argv -> IDE interface args. Below are the needed args
**         channel -> channel to be tested
**                      0x00 -- RED
**                      0x08 -- GREEN
**                      0x10 -- BLUE
**
** OUTPUTS: none
*/
int 
ABSetDiag(int argc, char **argv) 
{
	row = column = wroteab = readab = 0;
	if ((argc > 1) && (strncmp(argv[1], "r", 1) == 0)) {
		AB_W1 (mgbase, sysctl, RED);
		msg_printf(DBG, "Set RED DIAG MODE\n");
	} else if ((argc > 1)&&(strncmp(argv[1],"g",1) == 0)) {
		AB_W1 (mgbase, sysctl, GREEN);
		msg_printf(DBG, "Set GREEN DIAG MODE\n");
	} else if ((argc > 1)&&(strncmp(argv[1],"b",1) == 0)) {
		AB_W1 (mgbase, sysctl, BLUE);
		msg_printf(DBG, "Set BLUE DIAG MODE\n");
	} else
		msg_printf(SUM, "DIAG MODE SET INCORRECTLY\n");

	return(0);
}

/*
** NAME: abset1
**
** FUNCTION: This routine sets the specified memory location
**           with the specified value.
**
** INPUTS: argc, argv -> IDE interface args. Below are the needed args
**         xrow -- starting row address
**         xcolumn -- starting column address
**         value -- data to be written
**
** OUTPUTS: none
*/
void 
abset1(int argc, char **argv)
{
	unsigned int val;


	if (wroteab == 0) {

		/* Setup the address */
		if (argc > 3) {  
			atobu(argv[2], &row);
			atobu(argv[3], &column);
		} else { 
			row = 0;
			column = 0;
		}

		AB_W1(mgbase, high_addr, row >> 1);
		AB_W1(mgbase, low_addr, (((row & 0x1) << 7) | (column & 0x7F)));
	}

	if (argc < 2)
		val = 0x11;
	else
		atobu(argv[1], &val);

	/* Write the memory */
	AB_W1(mgbase, dRam, val);

	msg_printf(SUM, "Wrote AB 1 byte  0x%x in row 0x%x and column 0x%x byte %d \n", val, row, column, wroteab + wroteab1);

	/* Keep track of how many bytes are written */
	wroteab1++;
	if (wroteab1 > 3) {
		wroteab1 = 0;
		wroteab++;
	}
	if (wroteab > 3)
		wroteab = 0;
}

/*
** NAME: abset4
**
** FUNCTION: This routine sets the specified memory locations
**           with the specified value.
**
** INPUTS: argc, argv -> IDE interface args. Below are the needed args
**         xrow -- starting row address
**         xcolumn -- starting column address
**         value -- data to be written
**
** OUTPUTS: none
*/
void 
abset4(int argc, char **argv)
{
   unsigned int val;
   int i;


	if (wroteab == 0) {

		if (argc > 3) {  
			atobu(argv[2],&row);
			atobu(argv[3],&column);
		} else { 
			row = 0;
			column = 0;
		}

		AB_W1(mgbase, high_addr, row >> 1);
		AB_W1(mgbase, low_addr, (((row & 0x1) << 7) | (column & 0x7F)));
	}

	/* Make sure we are on a 4 byte boundary */
	if (wroteab1 > 0) {
		val = 0xab;
		for (i = wroteab1; i < 4; i++) {
			AB_W1(mgbase, dRam, val);
			msg_printf(SUM, "Wrote AB 1  0x%x in row 0x%x and column 0x%x byte %d \n", val, row, column, wroteab1 + wroteab);
			wroteab1++;
		}
		if (wroteab1 > 3) {
			wroteab1 = 0;
			wroteab++;
		}
	}

	if (argc < 2)
		val = 0xbadefade;
	else
		atobu(argv[1], &val);

	/* Write the specified value to 4 bytes */
	AB_W4(mgbase, dRam, val);

	msg_printf(SUM, "Wrote AB  0x%x in row 0x%x and column 0x%x byte %d \n",
				val, row, column, wroteab * 4);

	wroteab++;
	if (wroteab > 3)
		wroteab = 0;
}

/*
** NAME: absetrest
**
** FUNCTION: This routine sets the remaining 48 byte section of memory
**           with the specified value.
**
** INPUTS: argc, argv -> IDE interface args. Below are the needed args
**         xrow -- starting row address
**         xcolumn -- starting column address
**         value -- data to be written
**
** OUTPUTS: none
*/
void 
absetrest(int argc, char **argv)
{
   unsigned int val;
   int i;


	if (wroteab == 0) {

		if (argc > 3) {  
			atobu(argv[2],&row);
			atobu(argv[3],&column);

		} else { 
			row = 0;
			column = 0;
		}

		AB_W1(mgbase, high_addr, row >> 1);
		AB_W1(mgbase, low_addr, (((row & 0x1) << 7) | (column & 0x7F)));
	}
	if (wroteab1 > 0) {
		val = 0xab;
		for (i = wroteab1; i < 4; i++) {
			AB_W1(mgbase, dRam, val);
			msg_printf(SUM, "Wrote AB 1  0x%x in row 0x%x and column 0x%x byte %d \n", val, row, column, wroteab1 + wroteab);
			wroteab1++;
		}
		if (wroteab1 > 3) {
			wroteab1 = 0;
			wroteab++;
		}
	}

	if (argc < 2)
		val = 0xbadefade;
	else    
		atobu(argv[1], &val);

	for (i = wroteab; i < 4; i++) {
		AB_W4(mgbase, dRam, val);
		msg_printf(SUM, "Wrote AB  0x%x in row 0x%x and column 0x%x byte %d \n", 
				val, row, column, wroteab * 4);
		wroteab++;
	}
	if (wroteab > 3)
		wroteab = 0;
}

/*
** NAME: abget1
**
** FUNCTION: This routine reads 1 byte from the specified memory location
**
** INPUTS: argc, argv -> IDE interface args. Below are the needed args
**         row -- starting row address
**         column -- starting column address
**
** OUTPUTS: none
*/
void 
abget1(int argc, char **argv)
{
	unsigned int rcv;

	if (readab == 0) {
		if (argc > 2) {  
			atobu(argv[1], &row);
			atobu(argv[2], &column);
		} else { 
			row = 0;
			column = 0;
		}

		AB_W1(mgbase, high_addr, row >> 1);
		AB_W1(mgbase, low_addr, (((row & 0x1) << 7) | (column & 0x7F)));
	}
	rcv = AB_R1 (mgbase, dRam);

	msg_printf(SUM, "Read AB  0x%x in row 0x%x and column 0x%x byte %d \n", 
				rcv, row, column, readab + readab1);
	readab1++;
	if(readab1 >  3){
		readab1 = 0;
		readab++;
	}
	if(readab >  3)
		readab = 0;
}

/*
** NAME: abget4
**
** FUNCTION: This routine reads 4 bytes from the specified memory location
**
** INPUTS: argc, argv -> IDE interface args. Below are the needed args
**         xrow -- starting row address
**         xcolumn -- starting column address
**
** OUTPUTS: none
*/
void 
abget4(int argc, char **argv)
{
	unsigned int rcv;
	unsigned int val;
	int i;

	if (readab == 0) {

		if (argc > 2) {  
			atobu(argv[1],&row);
			atobu(argv[2],&column);
		} else { 
			row = 0;
			column = 0;
		}

		AB_W1(mgbase, high_addr, row >> 1);
		AB_W1(mgbase, low_addr, (((row & 0x1) << 7) | (column & 0x7F)));
	}

	if (readab1 > 0) {
		for (i = readab1; i < 4; i++) {
			val = AB_R1(mgbase, dRam);
			msg_printf(SUM, "Read AB 1  0x%x in row 0x%x and column 0x%x byte %d \n", val, row, column, readab1 + readab);
			readab1++;
		}
		if(readab1 > 3){
			readab1 = 0;
			readab++;
		}
	}

	rcv = AB_R4(mgbase, dRam);

	msg_printf(SUM, "Read AB  0x%x in row 0x%x and column 0x%x byte %d \n", 
				rcv, row, column, readab * 4);
	readab++;
	if(readab >  3)
		readab = 0;
}

/*
** NAME: abgetrest
**
** FUNCTION: This routine gets the remaining 48 byte section of memory
**
** INPUTS: argc, argv -> IDE interface args. Below are the needed args
**         xrow -- starting row address
**         xcolumn -- starting column address
**
** OUTPUTS: none
*/
int 
abgetrest(int argc, char **argv)
{
	unsigned int rcv;
	unsigned int val;
	int i;

	if (readab == 0) {
		if (argc > 2) {  
			atobu(argv[1], &row);
			atobu(argv[2], &column);
		} else { 
			row = 0;
			column = 0;
		}
		AB_W1(mgbase, high_addr, row >> 1);
		AB_W1(mgbase, low_addr, (((row & 0x1) << 7) | (column & 0x7F)));
	}

	if (readab1 > 0) {
		for (i = readab1; i < 4; i++) {
			val = AB_R1(mgbase, dRam);
			msg_printf(SUM, "Read AB 1  0x%x in row 0x%x and column 0x%x byte %d \n", val, row, column, readab1 + readab);
			readab1++;
		}
		if (readab1 > 3) {
			readab1 = 0;
			readab++;
		}
	}

	for (i = readab; i < 4;i++) {
		rcv = AB_R4(mgbase, dRam);
		msg_printf(SUM, "Read AB  0x%x in row 0x%x and column 0x%x byte %d \n", rcv, row, column, readab * 4);
		readab++;
	}

	if(readab > 3)
		readab = 0;

	return 0;
}
