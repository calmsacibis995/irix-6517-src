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
#include "libsc.h"
#include "uif.h"

extern struct gr2_hw  *base ;
extern ev1_initialize();

#define MAX_TRYCNT	5

#define		RED		0x0
#define		GREEN		0x8
#define		BLUE		0x10

#define 	AB1_ADDR_SIZE	0xFFFF

unsigned int WalkOne[] = {
	0xFFFE0001, 0xFFFD0002, 0xFFFB0004, 0xFFF70008,
	0xFFEF0010, 0xFFDF0020, 0xFFBF0040, 0xFF7F0080,
	0xFEFF0100, 0xFDFF0200, 0xFBFF0400, 0xF7FF0800,
	0xEFFF1000, 0xDFFF2000, 0xBFFF4000, 0x7FFF8000,
};

unsigned int AB1_PatArray[] = {
	0x55550000, 0xAAAA5555, 0x0000AAAA, 
	0x55550000, 0xAAAA5555, 0x0000AAAA
};

unsigned int CC1_PatArray[] = {
	0x55555555, 0xAAAAAAAA, 0x00000000, 
	0x55555555, 0xAAAAAAAA, 0x00000000
};

#define RESULT_SUMMARY(errs, str) \
{\
        if (errs)\
                sum_error(str);\
}

#define CHANNEL_ID(channel, RGBChannel) \
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


void
freeze_all_ABs()
{
        AB_W1 (base, high_addr, 0);
        AB_W1 (base, low_addr, 6);
        AB_W1 (base, intrnl_reg, 0x3);
        AB_W1 (base, low_addr, 0x16);
        AB_W1 (base, intrnl_reg, 0x3);
        AB_W1 (base, low_addr, 0x26);
        AB_W1 (base, intrnl_reg, 0x1);
	return;
}


int
Ab1_Dram_AddrBus_Test(int channel)
{
	unsigned int i, loop, row, column, rcv;
	unsigned int index, errCnt, addr_range;
	unsigned int *dataptr, databuf[16];
	unsigned int basebuf[] = {
		0x00010001, 0x00020002, 0x00030003, 0x00040004,
		0x00050005, 0x00060006, 0x00070007, 0x00080008,
		0x00090009, 0x000A000A, 0x000B000B, 0x000C000C,
		0x000D000D, 0x000D000E, 0x000F000F, 0x00100010,
	};
	char RGBChannel[20];
	int trycnt;


	errCnt=0;
	CHANNEL_ID(channel, RGBChannel);

	bcopy(basebuf, databuf,sizeof(basebuf));
 	dataptr = &databuf[0];

        /* Select the desired AB1 channel, also diagnostic mode  */
        AB_W1 (base, sysctl, channel);
	msg_printf(VRB, "%s Ab1_Dram_AddrBus_Test ......\n", RGBChannel);
        for(addr_range=0x0000; addr_range<AB1_ADDR_SIZE;) {
	  if ((addr_range & 0xFFF) == 0) {
		busy (1);
		msg_printf(DBG, "\r%s Ab1_Dram_AddrBus_Test  Writing %08X Addr region\n" , RGBChannel, addr_range);
	  }
	  for(loop=0;loop<0x4; loop++, ++addr_range) {
                row = addr_range >> 7;
                column = addr_range & 0x7F;

        	/* (8 LSBs of row address ) */
                AB_W1 (base, high_addr, row >> 1);
        	/* MSB of row, 7 Bits of column */
                AB_W1 (base, low_addr, (((row & 0x1) << 7) | (column & 0x7F)));

                /* Write to the AB1 external dRam */
                for(i=0; i<0x4; i++) {
                        AB_W4 (base, dRam, *dataptr);
			msg_printf(DBG, "\r%s Ab1_Dram_AddrBus_Test Addrs= 0x%08X data sent =0x%08X\n" ,RGBChannel, addr_range, *dataptr);
			dataptr++;
		}
	  }
	  dataptr=&databuf[0];
	  for(index=0; index<16; index++ )
		*(dataptr+index) += 0x00010001;	/* Inc datavalues */
        }

	 busy(1);
	/* ReadBack and verify */
	msg_printf(DBG, "\r%s Ab1_Dram_AddrBus_Test Readback + Verify\n", RGBChannel);
	bcopy(basebuf, databuf,sizeof(basebuf));
 	dataptr = &databuf[0];
        for(addr_range=0x0000; addr_range<AB1_ADDR_SIZE;) {
	  if ((addr_range & 0xFFF) == 0) {
		busy (1);
		msg_printf(DBG, "\r%s Ab1_Dram_AddrBus_Test  Reading %08X Addr region\n" , RGBChannel, addr_range);
	  }

	  for(loop=0;loop<0x4; loop++, ++addr_range) {
                row = addr_range >> 7;
                column = addr_range & 0x7F;

		/* HACK!!! try at least 5 times to read the 1st word
		 * This is trying to fix a crosstalk problem happened
		 * in read back from AB Dram.
		 */
		for (trycnt = 0; trycnt < MAX_TRYCNT; trycnt++) {
		    /* (8 LSBs of row address ) */
                    AB_W1 (base, high_addr, row >> 1);
		    /* MSB of row, 7 Bits of column */
                    AB_W1 (base, low_addr, (((row & 0x1) << 7) | (column & 0x7F)));

		    rcv = AB_R4 (base, dRam);
		    if (rcv == *dataptr) 
			break;
		}
		if (trycnt == MAX_TRYCNT) {
			msg_printf(ERR, "\r%s Failed Addrs= 0x%08X Exp=0x%08X Rcv=0x%08X\n" ,RGBChannel, addr_range, *dataptr, rcv);
			++errCnt;
		}
		++dataptr;
		for(i=1; i<4; i++) {
			rcv = AB_R4 (base, dRam);
			msg_printf(DBG, "\r%s Ab1_Dram_AddrBus_Test Addrs= 0x%08X Exp=0x%08X Rcv=0x%08X\n" ,RGBChannel, addr_range, *dataptr, rcv);
			if (rcv != *dataptr) {
				msg_printf(ERR, "\r%s Failed Addrs= 0x%08X Exp=0x%08X Rcv=0x%08X\n" ,RGBChannel, addr_range, *dataptr, rcv);
				++errCnt;
			} 
			++dataptr;
		}
          }
	/*  Update to next set of expected values */
  	dataptr=&databuf[0];
  	for(index=0; index<16; index++ )
		*(dataptr+index) += 0x00010001;	/* Inc datavalues */

	}
	busy (0);
	if (errCnt == 0)
		 msg_printf(SUM, "%s Ab1_Dram_AddrBus_Test PASSED\n", RGBChannel);
	else 
		msg_printf(ERR, "%s Ab1_Dram_AddrBus_Test FAILED Errors= %d\n", RGBChannel, errCnt);

	return(errCnt);
}


int
Ab1_Dram_DataBus_Test(int channel)
{
	/* Walking Ones and Zeros  Test across the Data Bus */

	unsigned int i, *dataptr, rcv, errCnt;
	unsigned int row, column, addr_range;
	unsigned int loop = 0;
	char RGBChannel[20];
	int trycnt;

	errCnt=0;
	CHANNEL_ID(channel, RGBChannel);
	dataptr = &WalkOne[0];

	/* Select the desired AB1 channel, also diagnostic mode  */
	AB_W1 (base, sysctl, channel);
	msg_printf(VRB, "%s Ab1_Dram_DataBus_Test ......\n", RGBChannel);
	for(addr_range=0; addr_range<0x10;) {
		row = addr_range >> 7;
		column = addr_range & 0x7F;

		AB_W1 (base, high_addr, row >> 1);/* (8 LSBs of row address ) */
		/* MSB of row, 7 Bits of column */
		AB_W1 (base, low_addr, (((row & 0x1) << 7) | (column & 0x7F)));

		dataptr = &WalkOne[loop & 0xF ];
		/* Write to the AB1 external dRam  */
		for(i=0; i<0x4; i++){
			msg_printf(DBG, "\r%s Ab1_Dram_DataBus_Test Addrs= 0x%08X data sent =0x%08X\n" ,RGBChannel, addr_range, *dataptr);
			AB_W4 (base, dRam, *dataptr++);
		}
		++addr_range;
		loop+=4;
	}

	busy(1);
	loop=0;
	/* ReadBack and verify */
	msg_printf(VRB, "\r%s Ab1_Dram_DataBus_Test ReadBack and verify\n", RGBChannel);
        for(addr_range=0; addr_range<0x10;) {
                row = addr_range >> 7;
                column = addr_range & 0x7F;

		dataptr = &WalkOne[loop & 0xF ];
		/* HACK!!! try at least 5 times to read the 1st word 
		 * This is trying to fix a crosstalk problem happened
		 * in read back from AB Dram.
		 */
                for (trycnt = 0; trycnt < MAX_TRYCNT; trycnt++) {
                    /* (8 LSBs of row address ) */
                    AB_W1 (base, high_addr, row >> 1);
                    /* MSB of row, 7 Bits of column */
                    AB_W1 (base, low_addr, (((row & 0x1) << 7) | (column & 0x7F)));
                    rcv = AB_R4 (base, dRam);
                    if (rcv == *dataptr)
                        break;
                }
                if (trycnt == MAX_TRYCNT) {
                        msg_printf(ERR, "\r%s Failed Addrs= 0x%08X Exp=0x%08X Rcv=0x%08X\n" ,RGBChannel, addr_range, *dataptr, rcv);
                        ++errCnt;
                }
                ++dataptr;
                for(i=1; i<4; i++) {
			rcv = AB_R4 (base, dRam) ;
                        msg_printf(DBG,"%s Ab1_Dram_DataBus_Test Addrs= 0x%x Exp=0x%x Rcv=0x%x\n" ,RGBChannel, addr_range, *dataptr, rcv);
			if (rcv != *dataptr) {
                               msg_printf(ERR,"%s Ab1_Dram_DataBus_Test Failed Addrs= 0x%x Exp=0x%x Rcv=0x%x\n" ,RGBChannel, addr_range, *dataptr, rcv);

				++errCnt;
			} else 
			++dataptr;
		}
		++addr_range;
		loop+=4;
	}
	busy(0);
	if (errCnt == 0)
		 msg_printf(SUM, "%s Ab1_Dram_DataBus_Test PASSED.\n", RGBChannel);
	else
		msg_printf(SUM, "%s Ab1_Dram_DataBus_Test Failed Errors=%d .\n", RGBChannel, errCnt);

	return(errCnt);
}


int
Ab1_Patrn_Test(int channel)
{
	/* Dram Patrn Test */
	
	unsigned int loop, errCnt;
        unsigned int i, rcv, *dataptr;
        int row, column, addr_range;
	char RGBChannel[20];
	int trycnt;


	errCnt=0;
	CHANNEL_ID(channel, RGBChannel);
        /* Select the desired AB1 channel, also diagnostic mode  */
        AB_W1 (base, sysctl, channel);
	msg_printf(VRB, "%s Ab1_Patrn_Test\n", RGBChannel);
	for(loop=0; loop<3; loop++) {
		msg_printf(VRB, "\r%s Ab1_Patrn_Test Writing Pass = %d......\n", RGBChannel, loop);
        	for(addr_range=0; addr_range<AB1_ADDR_SIZE;) {
	  		if ((addr_range & 0xFFF) == 0) {
				busy (1);
				msg_printf(DBG, "\r%s Ab1_Patrn_Test  Writing 0x%08X Addr region\n" , RGBChannel, addr_range);
			}

                	row = addr_range >> 7;
                	column = addr_range & 0x7F;
	
			/* (8 LSBs of row address ) */
                	AB_W1 (base, high_addr, row >> 1); 
			/* MSB of row, 7 Bits of column */
                	AB_W1 (base, low_addr, (((row & 0x1) << 7) | (column & 0x7F)));
	
       			dataptr = &AB1_PatArray[loop];							/* Reset the data ptr */
                	/* Write to the AB1 external dRam  */
                	for(i=0; i<0x4; i++){
				msg_printf(DBG, "\r%s Ab1_Patrn_Test Addrs= 0x%08X data sent =0x%08X\n" ,RGBChannel, addr_range, *dataptr);
                        	AB_W4 (base, dRam, *(dataptr++));
			}

			++addr_range;
        	}


		busy(1);
        	/* ReadBack and verify */
		msg_printf(DBG, "\r%s Ab1_Patrn_Test Readback + Verify Pass=%d \n", RGBChannel, loop);
        	for(addr_range=0; addr_range<AB1_ADDR_SIZE;) {
	  		if ((addr_range & 0xFFF) == 0)
				busy (1);
				msg_printf(DBG, "\r%s Ab1_Patrn_Test  Reading 0x%08X Addr region\n" , RGBChannel, addr_range);

               		row = (addr_range) >> 7;
               		column = (addr_range) & 0x7F;
			dataptr = &AB1_PatArray[loop];  
			/* HACK!!! try at least 5 times to read the 1st word 
		 	 * This is trying to fix a crosstalk problem happened
		 	 * in read back from AB Dram.
		 	 */
                    	for (trycnt = 0; trycnt < MAX_TRYCNT; trycnt++) {
                    		/* (8 LSBs of row address ) */
                    		AB_W1 (base, high_addr, row >> 1);
                    		/* MSB of row, 7 Bits of column */
                    		AB_W1 (base, low_addr, (((row & 0x1) << 7) | (column & 0x7F)));
                    		rcv = AB_R4 (base, dRam);
                    		if (rcv == *dataptr)
                        		break;
                	}
                	if (trycnt == MAX_TRYCNT) {
                        	msg_printf(ERR, "\r%s Failed Addrs= 0x%08X Exp=0x%08X Rcv=0x%08X\n" ,RGBChannel, addr_range, *dataptr, rcv);
                        	++errCnt;
			}
                	++dataptr;
                	for(i=1; i<4; i++) {
                        	rcv = AB_R4 (base, dRam) ;
                               	msg_printf(DBG, "\r%s Ab1_Patrn_Test Pass= %d Addrs= 0x%08X Exp=0x%08X Rcv=0x%08X ....\n" ,RGBChannel, loop, addr_range, *dataptr, rcv);
				if (rcv != *dataptr) {
					++errCnt;
                                	msg_printf(ERR, "\r%s Ab1_Patrn_Test Failed  Pass= %d Addrs= 0x%08X Exp=0x%08X Rcv=0x%08X ....\n" ,RGBChannel, loop, addr_range, *dataptr, rcv);
				} 
                         	++dataptr;
                	}
			++addr_range;
        	}
	}
	busy (0);
	if (errCnt == 0) 
		msg_printf(SUM, "%s Ab1_Patrn_Test PASSED\n", RGBChannel);
	else 
		msg_printf(ERR, "%s Ab1_Patrn_Test Failed ..Errors=%d\n" ,RGBChannel, errCnt);

        return(errCnt);
}


int
Patrn_Test(int channel)
{
	/* Dram Patrn Test */
	
	unsigned int loop, errCnt;
        unsigned int i, rcv, *dataptr;
        int row, column, addr_range;
	char RGBChannel[20];
	int trycnt;


	errCnt=0;
	CHANNEL_ID(channel, RGBChannel);
        /* Select the desired AB1 channel, also diagnostic mode  */
        AB_W1 (base, sysctl, channel);
	msg_printf (VRB, "%s Pattern test.....\n", RGBChannel);
	for(loop=0; loop<3; loop++) {
		msg_printf(VRB, "\r%s Patrn(16) Pass = %d......\n", RGBChannel, loop);
        	for(addr_range=0; addr_range<AB1_ADDR_SIZE;) {
	  		if ((addr_range & 0xFFF) == 0)
				busy (1);
				msg_printf(DBG, "\r%s Patrn_Test(16)  Testing 0x%08X Addr region\n" , RGBChannel, addr_range);

                	row = addr_range >> 7;
                	column = addr_range & 0x7F;
	
			/* (8 LSBs of row address ) */
                	AB_W1 (base, high_addr, row >> 1); 
			/* MSB of row, 7 Bits of column */
                	AB_W1 (base, low_addr, (((row & 0x1) << 7) | (column & 0x7F)));
	
       			dataptr = &AB1_PatArray[loop];							/* Reset the data ptr */
                	/* Write to the AB1 external dRam  */
                	for(i=0; i<0x4; i++){
                        	AB_W4 (base, dRam, *(dataptr++));
			}

			busy(1);
			dataptr = &AB1_PatArray[loop];  
			/* HACK!!! Try at least 5 time to read 1st word
		 	 * This is trying to fix a crosstalk problem happened
		 	 * in read back from AB Dram.
		 	 */
        		/* ReadBack and verify */
                	for (trycnt = 0; trycnt < MAX_TRYCNT; trycnt++) {
                    		/* (8 LSBs of row address ) */
                    		AB_W1 (base, high_addr, row >> 1);
                    		/* MSB of row, 7 Bits of column */
                    		AB_W1 (base, low_addr, (((row & 0x1) << 7) | (column & 0x7F)));
                    		rcv = AB_R4 (base, dRam);
                    		if (rcv == *dataptr)
                        	break;
                	}
                	if (trycnt == MAX_TRYCNT) {
                        	msg_printf(ERR, "\r%s Failed Addrs= 0x%08X Exp=0x%08X Rcv=0x%08X\n" ,RGBChannel, addr_range, *dataptr, rcv);
                        	++errCnt;
                	}
                	++dataptr;
                	for(i=1; i<4; i++) {
                        	rcv = AB_R4 (base, dRam) ;
                               	msg_printf(DBG, "\rAddrs= 0x%04X Exp=0x%08X Rcv=0x%08X ....\n" , addr_range, *dataptr, rcv);
				if (rcv != *dataptr) {
					++errCnt;
                                	msg_printf(ERR, "\rFailed Addrs= 0x%04X Exp=0x%08X Rcv=0x%08X ....\n" ,addr_range, *dataptr, rcv);
				} 
                         	++dataptr;
                	}
			++addr_range;
        	} /* For */
	}
	busy (0);
	if (errCnt == 0) 
		msg_printf(SUM, "%s Patrn_Test(16) PASSED\n");
	else 
		msg_printf(ERR, "%s Patrn_Test(16) Failed ..Errors=%d\n" ,errCnt);

        return(errCnt);
}


/*ARGSUSED*/
int
RedChannel_AddrBus_Test(int argc, char *argv[])
{
	int errs; 

	errs = Ab1_Dram_AddrBus_Test(RED);
	RESULT_SUMMARY(errs, "RedChannel_AddrBus_Test");
        return(errs);
}

/*ARGSUSED*/
int
RedChannel_Data_Test(int argc, char *argv[])
{
	int errs; 

	errs = Ab1_Dram_DataBus_Test(RED);
	errs += Ab1_Patrn_Test(RED);
	RESULT_SUMMARY(errs, "RedChannel_Data_Test");
        return(errs);
}


/*ARGSUSED*/
int
RedChannel_DataBus_Test(int argc, char *argv[])
{
	int errs; 

	errs = Ab1_Dram_DataBus_Test(RED);
	RESULT_SUMMARY(errs, "RedChannel_DataBus_Test");
        return(errs);
}

/*ARGSUSED*/
int
RedChannel_Patrn_Test(int argc, char *argv[])
{
	int errs; 

	errs = Ab1_Patrn_Test(RED);
	RESULT_SUMMARY(errs, "RedChannel_Patrn_Test");
        return(errs);
}

/*ARGSUSED*/
int
RedChannel_Patrn(int argc, char *argv[])
{
	int errs; 

	errs = Patrn_Test(RED);
	RESULT_SUMMARY(errs, "RedChannel_Patrn");
        return(errs);
}

/*ARGSUSED*/
int
GreenChannel_Patrn(int argc, char *argv[])
{
	int errs; 

	errs = Patrn_Test(GREEN);
	RESULT_SUMMARY(errs, "GreenChannel_Patrn");
        return(errs);
}

/*ARGSUSED*/
int
BlueChannel_Patrn(int argc, char *argv[])
{
	int errs; 

	errs = Patrn_Test(BLUE);
	RESULT_SUMMARY(errs, "BlueChannel_Patrn");
        return(errs);
}

/*ARGSUSED*/
int
RedChannel_Mem_Test(int argc, char *argv[])
{
	int errs; 

	errs = Ab1_Dram_AddrBus_Test(RED);
	errs += Ab1_Dram_DataBus_Test(RED);
	errs += Ab1_Patrn_Test(RED);
	RESULT_SUMMARY(errs, "RedChannel_Mem_Test");
        return(errs);
}

/*ARGSUSED*/
int
GreenChannel_AddrBus_Test(int argc, char *argv[])
{
	int errs; 

	errs = Ab1_Dram_AddrBus_Test(GREEN);
	RESULT_SUMMARY(errs, "GreenChannel_AddrBus_Test");
        return(errs);


}

/*ARGSUSED*/
int
GreenChannel_Data_Test(int argc, char *argv[])
{
	int errs; 

	errs = Ab1_Dram_DataBus_Test(GREEN);
	errs += Ab1_Patrn_Test(GREEN);
	RESULT_SUMMARY(errs, "GreenChannel_Data_Test");
        return(errs);
}

/*ARGSUSED*/
int
GreenChannel_DataBus_Test(int argc, char *argv[])
{
	int errs; 

	errs = Ab1_Dram_DataBus_Test(GREEN);
	RESULT_SUMMARY(errs, "GreenChannel_DataBus_Test");
        return(errs);
}

/*ARGSUSED*/
int
GreenChannel_Patrn_Test(int argc, char *argv[])
{
	int errs; 

	errs = Ab1_Patrn_Test(GREEN);
	RESULT_SUMMARY(errs, "GreenChannel_Patrn_Test");
        return(errs);
}

/*ARGSUSED*/
int
GreenChannel_Mem_Test(int argc, char *argv[])
{
	int errs; 

	errs = Ab1_Dram_AddrBus_Test(GREEN);
	errs += Ab1_Dram_DataBus_Test(GREEN);
	errs += Ab1_Patrn_Test(GREEN);
	RESULT_SUMMARY(errs, "GreenChannel_Mem_Test");
        return(errs);
}


/*ARGSUSED*/
int
BlueChannel_AddrBus_Test(int argc, char *argv[])
{
	int errs; 

	errs = Ab1_Dram_AddrBus_Test(BLUE);
	RESULT_SUMMARY(errs, "GreenChannel_Mem_Test");
        return(errs);
}

/*ARGSUSED*/
int
BlueChannel_Patrn_Test(int argc, char *argv[])
{
	int errs; 

	errs = Ab1_Patrn_Test(BLUE);
	RESULT_SUMMARY(errs, "BlueChannel_Patrn_Test");
        return(errs);
}

/*ARGSUSED*/
int
BlueChannel_Data_Test(int argc, char *argv[])
{
	int errs; 

	errs = Ab1_Dram_DataBus_Test(BLUE);
	errs +=Ab1_Patrn_Test(BLUE);

	RESULT_SUMMARY(errs, "BlueChannel_Data_Test");
        return(errs);
}

/*ARGSUSED*/
int
BlueChannel_DataBus_Test(int argc, char *argv[])
{
	int errs; 

	errs = Ab1_Dram_DataBus_Test(BLUE);
	RESULT_SUMMARY(errs, "BlueChannel_DataBus_Test");
        return(errs);
}

/*ARGSUSED*/
int
BlueChannel_Mem_Test(int argc, char *argv[])
{
	int errs; 

	errs = Ab1_Dram_AddrBus_Test(BLUE);
	errs += Ab1_Dram_DataBus_Test(BLUE);
	errs += Ab1_Patrn_Test(BLUE);

	RESULT_SUMMARY(errs, "BlueChannel_Mem_Test");
        return(errs);
}



int
AB1_Full_MemTest(int argc, char *argv[])
{
	int errs; 

	errs = RedChannel_Mem_Test(argc, argv);
	errs += GreenChannel_Mem_Test(argc, argv);
	errs += BlueChannel_Mem_Test(argc, argv);
	RESULT_SUMMARY(errs, "AB1_Full_MemTest");
	return(errs);
}

