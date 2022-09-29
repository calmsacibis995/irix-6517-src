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

#define		RED		0x0
#define		GREEN		0x8
#define		BLUE		0x10

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
        else\
                okydoky();\
}


static int
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


	errCnt=0;
	msg_printf(DBG, "Ab1_Dram_AddrBus_Test......\n");

	bcopy(basebuf, databuf,sizeof(basebuf));
 	dataptr = &databuf[0];

        /* Select the desired AB1 channel, also diagnostic mode  */
        base->ab1.sysctl = channel;
        for(addr_range=0; addr_range<0xFFFF;) {
	  for(loop=0;loop<0x4; loop++, ++addr_range) {
                row = addr_range >> 7;
                column = addr_range & 0x7F;

        	/* (8 LSBs of row address ) */
                base->ab1.high_addr = row >> 1;
        	/* MSB of row, 7 Bits of column */
                base->ab1.low_addr = (((row & 0x1) << 7) | (column & 0x7F));

                /* Write to the AB1 external dRam */
                for(i=0; i<0x4; i++) {
                        base->ab1.dRam = *dataptr;
			  /* msg_printf(DBG,"data sent 0x%08X\n", *dataptr);  */
			dataptr++;
		}
	  }
	  dataptr=&databuf[0];
	  for(index=0; index<16; index++ )
		*(dataptr+index) += 0x00010001;	/* Inc datavalues */
        }

	/* ReadBack and verify */
	bcopy(basebuf, databuf,sizeof(basebuf));
 	dataptr = &databuf[0];
        for(addr_range=0; addr_range<0xFFFF;) {
	  for(loop=0;loop<0x4; loop++) {
                row = addr_range >> 7;
                column = addr_range & 0x7F;

		/* (8 LSBs of row address ) */
                base->ab1.high_addr = row >> 1;
		/* MSB of row, 7 Bits of column */
                base->ab1.low_addr = (((row & 0x1) << 7) | (column & 0x7F));

		for(i=0; i<0x4; i++) {
			rcv = base->ab1.dRam ;
			if (rcv != *dataptr) {
				msg_printf(ERR, "Ab1_Dram_AddrBus_Test:: Addrs= 0x%08X Exp=0x%08X Rcv=0x%08X\n" ,addr_range, *dataptr, rcv);
				++errCnt;
			}
			++dataptr;
		}
		++addr_range;
          }
	/*  Update to next set of expected values */
  	dataptr=&databuf[0];
  	for(index=0; index<16; index++ )
		*(dataptr+index) += 0x00010001;	/* Inc datavalues */

	}
	return(errCnt);
}

static int
Ab1_Dram_DataBus_Test(int channel)
{
	/* Walking Ones and Zeros  Test across the Data Bus */

	unsigned int i, j, *dataptr, rcv, errCnt;
	unsigned int row, column, addr_range;
	unsigned int cur_row, cur_column, rcv2[4];
	unsigned int loop = 0;

	errCnt=0;
	msg_printf(DBG, "Ab1_Dram_DataBus_Test......\n");
	dataptr = &WalkOne[0];

	/* Select the desired AB1 channel, also diagnostic mode  */
	base->ab1.sysctl = channel;
	for(addr_range=0; addr_range<0x10;) {
		row = addr_range >> 7;
		column = addr_range & 0x7F;

		base->ab1.high_addr = row >> 1;/* (8 LSBs of row address ) */
		/* MSB of row, 7 Bits of column */
		base->ab1.low_addr = (((row & 0x1) << 7) | (column & 0x7F));

		dataptr = &WalkOne[loop & 0xF ];
		/* Write to the AB1 external dRam  */
		for(i=0; i<0x4; i++){
			msg_printf(DBG, "Write 0x%08X\n" ,*dataptr);
			base->ab1.dRam = *dataptr++;
		}
		++addr_range;
		loop+=4;
	}

	loop=0;
	/* ReadBack and verify */
        for(addr_range=0; addr_range<0x10;) {
                row = addr_range >> 7;
                column = addr_range & 0x7F;

                base->ab1.high_addr = row >> 1;/* (8 LSBs of row address ) */
	 	/* MSB of row, 7 Bits of column */
                base->ab1.low_addr = (((row & 0x1) << 7) | (column & 0x7F));

		dataptr = &WalkOne[loop & 0xF ];
		for(i=0; i<0x4; i++) {
			rcv = base->ab1.dRam ;
			if (rcv != *dataptr) {
                               msg_printf(ERR,"Ab1_Dram_DataBus_Test:: Addrs= 0x%x Exp=0x%x Rcv=0x%x\n" ,addr_range, *dataptr, rcv);

				++errCnt;
			}
			++dataptr;
		}
		++addr_range;
		loop+=4;
	}
	return(errCnt);
}


static int
Ab1_Dram_Data_Test(int channel)
{
	/* Dram Patrn Test */
	
	unsigned int loop, errCnt;
        unsigned int i, rcv, *dataptr;
        int row, column, addr_range;


	errCnt=0;
	msg_printf(DBG, "Ab1_Dram_Data_Test......\n");
        /* Select the desired AB1 channel, also diagnostic mode  */
        base->ab1.sysctl = channel;
	for(loop=0; loop<3; loop++) {
        	for(addr_range=0; addr_range<0xFFFF;) {
                	row = addr_range >> 7;
                	column = addr_range & 0x7F;
	
			/* (8 LSBs of row address ) */
                	base->ab1.high_addr = row >> 1; 
			/* MSB of row, 7 Bits of column */
                	base->ab1.low_addr = (((row & 0x1) << 7) | (column & 0x7F));
	
       			dataptr = &AB1_PatArray[loop];							/* Reset the data ptr */
                	/* Write to the AB1 external dRam  */
                	for(i=0; i<0x4; i++)
                        	base->ab1.dRam = *(dataptr++);

			++addr_range;
        	}


        	/* ReadBack and verify */
        	for(addr_range=0; addr_range<0xFFFF;) {
               		row = (addr_range) >> 7;
               		column = (addr_range) & 0x7F;

               		base->ab1.high_addr = row >> 1;/* (8 LSBs of row address ) */
			/* MSB of row, 7 Bits of column */
               		base->ab1.low_addr = (((row & 0x1) << 7) | (column & 0x7F));

			dataptr = &AB1_PatArray[loop];  
                	for(i=0; i<0x4; i++) {
                        	rcv = base->ab1.dRam ;
				if (rcv != *dataptr) {
                                	msg_printf(ERR, "Ab1_Dram_DataBus_Test:: Addrs= 0x%08X Exp=0x%08X Rcv=0x%08X ....\n" ,addr_range, *dataptr, rcv);
				}
                         	++dataptr;
                	}
			++addr_range;
        	}
	}
        return(errCnt);
}

RedChannel_AddrBus_Test(int argc, char *argv[])
{
	int errs; 

	msg_printf(DBG, "RedChannel_AddrBus_Test......\n");
	errs = Ab1_Dram_AddrBus_Test(RED);

	RESULT_SUMMARY(errs, "RedChannel_AddrBus_Test");
        return(errs);
}

RedChannel_Data_Test(int argc, char *argv[])
{
	int errs; 

	msg_printf(DBG, "RedChannel_Data_Test......\n");
	errs = Ab1_Dram_DataBus_Test(RED);
	errs += Ab1_Dram_Data_Test(RED);

	RESULT_SUMMARY(errs, "RedChannel_Data_Test");
        return(errs);
}


RedChannel_DataBus_Test(int argc, char *argv[])
{
	int errs; 

	msg_printf(DBG, "RedChannel_DataBus_Test......\n");
	errs = Ab1_Dram_DataBus_Test(RED);

	RESULT_SUMMARY(errs, "RedChannel_DataBus_Test");
        return(errs);
}

RedChannel_Mem_Test(int argc, char *argv[])
{
	int errs; 

	msg_printf(DBG, "RedChannel_Mem_Test......\n");
	errs = Ab1_Dram_AddrBus_Test(RED);
	errs += Ab1_Dram_DataBus_Test(RED);
	errs += Ab1_Dram_Data_Test(RED);

	RESULT_SUMMARY(errs, "RedChannel_Mem_Test");
        return(errs);
}

GreenChannel_AddrBus_Test(int argc, char *argv[])
{
	int errs; 

	msg_printf(DBG, "GreenChannel_AddrBus_Test......\n");
	errs = Ab1_Dram_AddrBus_Test(GREEN);

	RESULT_SUMMARY(errs, "GreenChannel_AddrBus_Test");
        return(errs);


}

GreenChannel_Data_Test(int argc, char *argv[])
{
	int errs; 

	msg_printf(DBG, "GreenChannel_Data_Test......\n");
	errs = Ab1_Dram_DataBus_Test(GREEN);
	errs += Ab1_Dram_Data_Test(GREEN);

	RESULT_SUMMARY(errs, "GreenChannel_Data_Test");
        return(errs);
}

GreenChannel_DataBus_Test(int argc, char *argv[])
{
	int errs; 

	msg_printf(DBG, "GreenChannel_DataBus_Test......\n");
	errs = Ab1_Dram_DataBus_Test(GREEN);

	RESULT_SUMMARY(errs, "GreenChannel_DataBus_Test");
        return(errs);
}

GreenChannel_Mem_Test(int argc, char *argv[])
{
	int errs; 

	msg_printf(DBG, "GreenChannel_Mem_Test......\n");
	errs = Ab1_Dram_AddrBus_Test(GREEN);
	errs += Ab1_Dram_DataBus_Test(GREEN);
	errs += Ab1_Dram_Data_Test(GREEN);

	RESULT_SUMMARY(errs, "GreenChannel_Mem_Test");
        return(errs);
}


BlueChannel_AddrBus_Test(int argc, char *argv[])
{
	int errs; 

	msg_printf(DBG, "BlueChannel_AddrBus_Test......\n");
	errs = Ab1_Dram_AddrBus_Test(BLUE);

	RESULT_SUMMARY(errs, "GreenChannel_Mem_Test");
        return(errs);
}

BlueChannel_Data_Test(int argc, char *argv[])
{
	int errs; 

	msg_printf(DBG, "BlueChannel_Data_Test......\n");
	errs = Ab1_Dram_DataBus_Test(BLUE);
	errs +=Ab1_Dram_Data_Test(BLUE);

	RESULT_SUMMARY(errs, "BlueChannel_Data_Test");
        return(errs);
}

BlueChannel_DataBus_Test(int argc, char *argv[])
{
	int errs; 

	msg_printf(DBG, "BlueChannel_DataBus_Test......\n");
	errs = Ab1_Dram_DataBus_Test(BLUE);

	RESULT_SUMMARY(errs, "BlueChannel_DataBus_Test");
        return(errs);
}

BlueChannel_Mem_Test(int argc, char *argv[])
{
	int errs; 

	msg_printf(DBG, "BlueChannel_Mem_Test......\n");
	errs = Ab1_Dram_AddrBus_Test(BLUE);
	errs += Ab1_Dram_DataBus_Test(BLUE);
	errs += Ab1_Dram_Data_Test(BLUE);

	RESULT_SUMMARY(errs, "BlueChannel_Mem_Test");
        return(errs);
}



AB1_Full_MemTest(int argc, char *argv[])
{
	int errs; 

        msg_printf(DBG, "RedChannel_Mem_Test......\n");
        errs = Ab1_Dram_AddrBus_Test(RED);
        errs += Ab1_Dram_DataBus_Test(RED);
        errs += Ab1_Dram_Data_Test(RED);

	errs = 0;
	msg_printf(DBG, "Red Channel Ab1_Dram_AddrBus_Test ...\n");
	errs = Ab1_Dram_AddrBus_Test(RED);
	if (errs)  
	 	msg_printf(ERR, "%d errors detected\n", errs);

	errs = 0;
	msg_printf(DBG, "Red Channel Ab1_Dram_DataBus_Test ...\n");
	errs = Ab1_Dram_DataBus_Test(RED);
	if (errs)  
	 	msg_printf(ERR, "%d errors detected\n", errs);

	errs = 0;
	msg_printf(DBG, "Red Channel Ab1_Dram_Data_Test ...\n");
	errs = Ab1_Dram_Data_Test(RED);
	if (errs > 0)  
	 	msg_printf(ERR, "%d errors detected\n", errs);

	msg_printf(DBG, "Green Channel Ab1_Dram_AddrBus_Test ...\n");
	errs = Ab1_Dram_AddrBus_Test(GREEN);
	if (errs > 0)  
	 	msg_printf(ERR, "%d errors detected\n", errs);

	msg_printf(DBG, "Green Channel Ab1_Dram_DataBus_Test ...\n");
	errs = Ab1_Dram_DataBus_Test(GREEN);
	if (errs > 0)  
	 	msg_printf(ERR, "%d errors detected\n", errs);

	msg_printf(DBG, "Green Channel Ab1_Dram_Data_Test ...\n");
	errs = Ab1_Dram_Data_Test(GREEN);
	if (errs > 0)  
	 	msg_printf(ERR, "%d errors detected\n", errs);

	msg_printf(DBG, "Blue Channel Ab1_Dram_AddrBus_Test ...\n");
	errs = Ab1_Dram_AddrBus_Test(BLUE);
	if (errs > 0)  
	 	msg_printf(ERR, "%d errors detected\n", errs);


	msg_printf(DBG, "Blue Channel Ab1_Dram_DataBus_Test ...\n");
	errs = Ab1_Dram_DataBus_Test(BLUE);
	if (errs > 0)  
		msg_printf(ERR, "%d errors detected\n", errs);

	msg_printf(DBG, "Blue Channel Ab1_Dram_Dram_Data_Test ...\n");
	errs = Ab1_Dram_Data_Test(BLUE);
	if (errs > 0)  
		msg_printf(ERR, "%d errors detected\n", errs);

	RESULT_SUMMARY(errs, "AB1_Full_MemTest");
	return(errs);
}

