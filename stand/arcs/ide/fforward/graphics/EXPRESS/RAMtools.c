
#define GR2_RAMTOOLS	1

#include "sys/gr2hw.h"
#include "ide_msg.h"
#include "gr2_loc.h"    
#include "gr3_loc.h"    
#include "gu1_loc.h"    
#include "sys/param.h"
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "gr2.h"

#if IP20
void gr2_reset(void);
#endif

/* #define MFG_USED 1 */

/* Values are used to determine which RAM to test and also index into the raminfo  structure */
#define GE_0    0		
#define GE_1    1
#define GE_2    2
#define GE_3    3
#define GE_4    4
#define GE_5    5
#define GE_6    6
#define GE_7    7
#define SHRAM_TYPE      8
#define HQ2URAM_TYPE    9
#define GE7URAM_TYPE 	10	
#define FIFO_TYPE 	11	
#define ALLRAM  12 		/*Must be the highest value*/
#define WRRD4_SHRAM	13 		
#define WRRD4_NOCOMP	14 		
#define	WRRD1_SHRAM_1   15

#define ALLTEST  	0 		
#define CHECKPAT 	1
#define INC_COMPLEMENT 	2 
#define CELL_ADDR	3 
extern struct gr2_hw *base;
extern int Gr2ProbGEs();
extern int GBL_GRVers;

/* used to figure which gfx slot to reset */
#define GR2_GFX_BASE	((struct gr2_hw *)PHYS_TO_K1(0x1f000000))

static int numge;
struct raminfo {
	char name[16];
	int size;
};

#define FIFO_LIMIT 	64
/*Names indexed by number assigned*/ 
struct raminfo RAMinfo[12] = {  	"GE_0 RAM0", 256,
					"GE_1 RAM0", 256,
					"GE_2 RAM0", 256,
					"GE_3 RAM0", 256,
					"GE_4 RAM0", 256,
					"GE_5 RAM0", 256,
					"GE_6 RAM0", 256,
					"GE_7 RAM0", 256,
					"SHRAM", 32768,
					"HQ2 URAM", 8192,
					"GE7URAM", 64*1024,
					"FIFO",	32768
};

/* STOP after 50 errors if console set to graphics, so error will be reported */
int stop_at_50() {
	if (console_is_gfx())
	    return 1;
        else 
	    return 0;
	}

void
RAMmenu(void)
{
        msg_printf(SUM, "RAM TYPE (choose one):\n");	
	msg_printf(SUM, "		SHRAM = %d		HQ2URAM = %d\n",SHRAM_TYPE,HQ2URAM_TYPE);
	msg_printf(SUM, "		GE7URAM = %d		FIFO = %d\n",GE7URAM_TYPE,FIFO_TYPE);


        msg_printf(SUM, "		GE RAM0 = (0..7)	ALL RAMS = %d\n",ALLRAM);
}
void testmenu(void){
        msg_printf(SUM, "\n\nCan specify type of test to run:\n");	
        msg_printf(SUM, "		patterns = %d		Incrementing & complement patterns = %d\n",CHECKPAT,INC_COMPLEMENT);
        msg_printf(SUM, "		cell's phys addr = %d	All tests = %d\n",CELL_ADDR,ALLTEST);
 }
void GE7URAMwr(unsigned int pattern, int offset)
{
	
	base->hq.gepc = offset;
        base->ge[0].ram0[248] = pattern;        /*Bits 0:31 */
        base->ge[0].ram0[249] = pattern;        /*Bits 32:63*/
        base->ge[0].ram0[250] = pattern;        /*Bits 64:95*/
        base->ge[0].ram0[251] = pattern & 0x7f; /*Bits 96102*/
        base->hq.ge7loaducode = pattern & 0x3dfffff; /*Bits 0:25, but not 21*/

}

int
GE7URAMrdcomp(unsigned int pattern, int offset)
{
	unsigned int rddata0, rddata1,rddata2, rddata3,rduload;
	int error;

		error = 0;
		base->hq.gepc = offset;
                rduload = base->hq.ge7loaducode;
                rddata0 = base->ge[0].ram0[248];
                rddata1 = base->ge[0].ram0[249];
                rddata2 = base->ge[0].ram0[250];
                rddata3 = base->ge[0].ram0[251];

                if (rddata0 != pattern) {
		   if ((rddata0 & 0xffff) != (pattern & 0xffff) ) {
			 msg_printf(ERR,"GE7URAM %#x,Bits[0:15]:  expected %#x, returned %#x\n",
                                         offset,pattern & 0xffff, rddata0 & 0xffff);
			if (GBL_GRVers < 4)
			 		print_GR2_loc(GE7_URAM0_LOC);
			else if ((GBL_GRVers > 4) && (GBL_GRVers <= 8 ))
					print_GU1_loc(GU1_GE7_URAM0_LOC);
			else {
#if IP22
					if (is_indyelan())
						print_GR3_loc(GR3_GE7_URAM0_LOC_INDY);
					else
#endif
						print_GR3_loc(GR3_GE7_URAM0_LOC);
			 }
		  }
		   if ((rddata0 & 0xffff0000) != (pattern & 0xffff0000) ) {
			 msg_printf(ERR,"GE7URAM %#x,Bits[16:31]:  expected %#x, returned %#x\n",
                                         offset,pattern >> 16, (rddata0 >> 16));
			if (GBL_GRVers < 4)
			 		print_GR2_loc(GE7_URAM1_LOC);
			else if ((GBL_GRVers > 4) && (GBL_GRVers <= 8 ))
					print_GU1_loc(GU1_GE7_URAM1_LOC);
			else {
#if IP22
					if (is_indyelan())
						print_GR3_loc(GR3_GE7_URAM1_LOC_INDY);
					else
#endif
						print_GR3_loc(GR3_GE7_URAM1_LOC);
			 }
			}
                        error++;
                }
                if (rddata1 != pattern) {
		   if ((rddata1 & 0xffff) != (pattern & 0xffff) ) {
			 msg_printf(ERR,"GE7URAM %#x,Bits[32:47]:  expected %#x, returned %#x\n",
                                         offset,pattern & 0xffff, rddata1 & 0xffff);
			if (GBL_GRVers < 4)
			 		print_GR2_loc(GE7_URAM2_LOC);
			else if ((GBL_GRVers > 4) && (GBL_GRVers <= 8 ))
					print_GU1_loc(GU1_GE7_URAM2_LOC);
			else {
#if IP22
					if (is_indyelan())
						print_GR3_loc(GR3_GE7_URAM2_LOC_INDY);
					else
#endif
						print_GR3_loc(GR3_GE7_URAM2_LOC);
			 }
		  }
		   if ((rddata1 & 0xffff0000) != (pattern & 0xffff0000) ) {
			 msg_printf(ERR,"GE7URAM %#x,Bits[48:63]:  expected %#x, returned %#x\n",
                                         offset,pattern >> 16, rddata1 >> 16);
			if (GBL_GRVers < 4)
			 		print_GR2_loc(GE7_URAM3_LOC);
			else if ((GBL_GRVers > 4) && (GBL_GRVers <= 8 ))
					print_GU1_loc(GU1_GE7_URAM3_LOC);
			else {
#if IP22
					if (is_indyelan())
						print_GR3_loc(GR3_GE7_URAM3_LOC_INDY);
					else
#endif
						print_GR3_loc(GR3_GE7_URAM3_LOC);
			 }
                	}
                        error++;
                }
                if (rddata2 != pattern) {
		   if ((rddata2 & 0xffff) != (pattern & 0xffff) ) {
			 msg_printf(ERR,"GE7URAM %#x,Bits[64:79]:  expected %#x, returned %#x\n",
                                         offset,pattern & 0xffff, rddata2 & 0xffff);
			if (GBL_GRVers < 4)
			 		print_GR2_loc(GE7_URAM4_LOC);
			else if ((GBL_GRVers > 4) && (GBL_GRVers <= 8 ))
					print_GU1_loc(GU1_GE7_URAM4_LOC);
			else {
#if IP22
					if (is_indyelan())
						print_GR3_loc(GR3_GE7_URAM4_LOC_INDY);
					else
#endif
						print_GR3_loc(GR3_GE7_URAM4_LOC);
			 }
		   }
		   if ((rddata2 & 0xffff0000) != (pattern & 0xffff0000) ) {
			 msg_printf(ERR,"GE7URAM %#x,Bits[80:95]:  expected %#x, returned %#x\n",
                                         offset,pattern >> 16, rddata2 >> 16);
			if (GBL_GRVers < 4)
			 		print_GR2_loc(GE7_URAM5_LOC);
			else if ((GBL_GRVers > 4) && (GBL_GRVers <= 8 ))
					print_GU1_loc(GU1_GE7_URAM5_LOC);
			else {
#if IP22
					if (is_indyelan())
						print_GR3_loc(GR3_GE7_URAM5_LOC_INDY);
					else
#endif
						print_GR3_loc(GR3_GE7_URAM5_LOC);
			 }
                	}
                        error++;
                }

                if ((rddata3 & 0x7f ) != (pattern & 0x7f)) {
                        msg_printf(ERR,"GE7URAM %#x,Bits[96:102]:  expected %#x, returned %#x\n",
                                 offset,pattern & 0x7f, rddata3 & 0x7f);
			if (GBL_GRVers < 4)
			 		print_GR2_loc(GE7_URAM6_LOC);
			else if ((GBL_GRVers > 4) && (GBL_GRVers <= 8 ))
					print_GU1_loc(GU1_GE7_URAM6_LOC);
			else { 
#if IP22
					if (is_indyelan())
						print_GR3_loc(GR3_GE7_URAM6_LOC_INDY);
					else
#endif
						print_GR3_loc(GR3_GE7_URAM6_LOC);
			 }
                        error++;
                }
                if ((rduload & 0x3dfffff ) != (pattern & 0x3dfffff)) {
		   if ((rduload & 0x1ff) != (pattern & 0x1ff) ) {
			 msg_printf(ERR,"GE7URAM %#x,Bits[0:8] to HQ:  expected %#x, returned %#x\n",
                                         offset,pattern & 0x1ff, rduload & 0x1ff);
			if (GBL_GRVers < 4)
			 		print_GR2_loc(GE7_URAM7_LOC);
			else if ((GBL_GRVers > 4) && (GBL_GRVers <= 8 ))
					print_GU1_loc(GU1_GE7_URAM7_LOC);
			else {
#if IP22
					if (is_indyelan())
						print_GR3_loc(GR3_GE7_URAM7_LOC_INDY);
					else
#endif
						print_GR3_loc(GR3_GE7_URAM7_LOC);
			 }
		     } else {
			 msg_printf(ERR,"GE7URAM %#x,Bits[9:25](ignore [21]) to HQ:  expected %#x, returned %#x\n",
                                         offset,(pattern & 0x3dfffff) >> 9, (rduload & 0x3dfffff) >> 9);
			if (GBL_GRVers < 4)
			 		print_GR2_loc(GE7_URAM7_LOC);
			else if ((GBL_GRVers > 4) && (GBL_GRVers <= 8 ))
					print_GU1_loc(GU1_GE7_URAM7_LOC);
			else {
#if IP22
					if (is_indyelan())
						print_GR3_loc(GR3_GE7_URAM7_LOC_INDY);
					else
#endif
						print_GR3_loc(GR3_GE7_URAM7_LOC);
			 }
			}
                        error++;
                }
	return error;
}


int
compdata(unsigned int rdval,unsigned int pattern,int ramtype,int offset)
{
	int error;
	error = 0;

	if (ramtype == HQ2URAM_TYPE) {
		if ((rdval & 0xffffff) != (pattern & 0xffffff)) {
			msg_printf(ERR,"%s %#x:	expected %#06x, returned %#06x\n",
			 RAMinfo[ramtype].name,offset,pattern & 0xffffff, rdval & 0xffffff);
		switch (GBL_GRVers) {
			case 0:
			case 1:
			case 2:
			case 3:
		  		if ((rdval & 0xff) != (pattern & 0xff)) 
					print_GR2_loc(HQ2_URAM0_LOC);
		  		if ((rdval & 0x00ff00) != (pattern & 0x00ff00)) 
					print_GR2_loc(HQ2_URAM1_LOC);
		  		if ((rdval & 0xff0000) != (pattern & 0xffff00)) 
					print_GR2_loc(HQ2_URAM2_LOC);
				break;
			case 4:
			case 9:
			case 10:
			case 11:
#if IP22
				if (is_indyelan()) {
		  		if ((rdval & 0xff) != (pattern & 0xff)) 
					print_GR3_loc(GR3_HQ2_URAM0_LOC_INDY);
		  		if ((rdval & 0x00ff00) != (pattern & 0x00ff00))
					print_GR3_loc(GR3_HQ2_URAM1_LOC_INDY);
		  		if ((rdval & 0xff0000) != (pattern & 0xffff00))
					print_GR3_loc(GR3_HQ2_URAM2_LOC_INDY);
				} else
#endif
				{
		  		if ((rdval & 0xff) != (pattern & 0xff)) 
					print_GR3_loc(GR3_HQ2_URAM0_LOC);
		  		if ((rdval & 0x00ff00) != (pattern & 0x00ff00))
					print_GR3_loc(GR3_HQ2_URAM1_LOC);
		  		if ((rdval & 0xff0000) != (pattern & 0xffff00))
					print_GR3_loc(GR3_HQ2_URAM2_LOC);
				}
				break;
			case 5:
		  		if ((rdval & 0xff) != (pattern & 0xff)) 
					print_GU1_loc(GU1_HQ2_URAM0_LOC);
		  		if ((rdval & 0x00ff00) != (pattern & 0x00ff00)) 
					print_GU1_loc(GU1_HQ2_URAM1_LOC);
		  		if ((rdval & 0xff0000) != (pattern & 0xffff00)) 
					print_GU1_loc(GU1_HQ2_URAM2_LOC);
				break;
			case 6:
			case 7:
			case 8:
		  		if ((rdval & 0xff) != (pattern & 0xff)) 
					print_GU1_loc(GU1_004_HQ2_URAM0_LOC);
		  		if ((rdval & 0x00ff00) != (pattern & 0x00ff00)) 
					print_GU1_loc(GU1_004_HQ2_URAM1_LOC);
		  		if ((rdval & 0xff0000) != (pattern & 0xffff00)) 
					print_GU1_loc(GU1_004_HQ2_URAM2_LOC);
				break;
			default:
				printf("Board id 12-15 undefined\n");
				break;
		}
	          error++;				
		 } 
	}
	else {
		if (rdval != pattern) {
		  msg_printf(ERR,"%s %#x:	expected %#x, returned %#x\n",
			 RAMinfo[ramtype].name,offset,pattern, rdval);
		switch(GBL_GRVers) {
		   case 0:
		   case 1:
		   case 2:
		   case 3:
		  	if (ramtype == GE_0) print_GR2_loc(GE7_0_LOC);
		  	if (ramtype == GE_1) print_GR2_loc(GE7_1_LOC);
		  	if (ramtype == GE_2) print_GR2_loc(GE7_2_LOC);
		  	if (ramtype == GE_3) print_GR2_loc(GE7_3_LOC);
		  	if (ramtype == GE_4) print_GR2_loc(GE7_4_LOC);
		  	if (ramtype == GE_5) print_GR2_loc(GE7_5_LOC);
		  	if (ramtype == GE_6) print_GR2_loc(GE7_6_LOC);
		  	if (ramtype == GE_7) print_GR2_loc(GE7_7_LOC);
			break;
		case 4:
		case 9:
		case 10:
		case 11:
#if IP22
		      if(is_indyelan()) {
		  	if (ramtype == GE_0) print_GR3_loc(GR3_GE7_0_LOC_INDY);
		  	if (ramtype == GE_1) print_GR3_loc(GR3_GE7_1_LOC_INDY);
		  	if (ramtype == GE_2) print_GR3_loc(GR3_GE7_2_LOC_INDY);
		  	if (ramtype == GE_3) print_GR3_loc(GR3_GE7_3_LOC_INDY);
		  	if (ramtype == GE_4) print_GR3_loc(GR3_GE7_4_LOC_INDY);
		  	if (ramtype == GE_5) print_GR3_loc(GR3_GE7_5_LOC_INDY);
		  	if (ramtype == GE_6) print_GR3_loc(GR3_GE7_6_LOC_INDY);
		  	if (ramtype == GE_7) print_GR3_loc(GR3_GE7_7_LOC_INDY);
		      } else
#endif
			{
		  	if (ramtype == GE_0) print_GR3_loc(GR3_GE7_0_LOC);
		  	if (ramtype == GE_1) print_GR3_loc(GR3_GE7_1_LOC);
		  	if (ramtype == GE_2) print_GR3_loc(GR3_GE7_2_LOC);
		  	if (ramtype == GE_3) print_GR3_loc(GR3_GE7_3_LOC);
		  	if (ramtype == GE_4) print_GR3_loc(GR3_GE7_4_LOC);
		  	if (ramtype == GE_5) print_GR3_loc(GR3_GE7_5_LOC);
		  	if (ramtype == GE_6) print_GR3_loc(GR3_GE7_6_LOC);
		  	if (ramtype == GE_7) print_GR3_loc(GR3_GE7_7_LOC);
			}
		   	break;

		   case 5:
		   case 6:
		   case 7:
		   case 8:
		  	if (ramtype == GE_0) print_GU1_loc(GU1_GE7_0_LOC);
		  	if (ramtype == GE_1) print_GU1_loc(GU1_GE7_1_LOC);
		  	if (ramtype == GE_2) print_GU1_loc(GU1_GE7_2_LOC);
		  	if (ramtype == GE_3) print_GU1_loc(GU1_GE7_3_LOC);
		  	if (ramtype == GE_4) print_GU1_loc(GU1_GE7_4_LOC);
		  	if (ramtype == GE_5) print_GU1_loc(GU1_GE7_5_LOC);
		  	if (ramtype == GE_6) print_GU1_loc(GU1_GE7_6_LOC);
		  	if (ramtype == GE_7) print_GU1_loc(GU1_GE7_7_LOC);
			break;
		default:
			printf("Board id 12-15 undefined\n");
			break;
		}

		  if (ramtype == SHRAM_TYPE) {
		    if ((rdval & 0xff) != (pattern & 0xff)) 
			{
			if (GBL_GRVers < 4)
				print_GR2_loc(GE7_SRAM0_LOC);
			else if (GBL_GRVers == 5) 
				print_GU1_loc(GU1_GE7_SRAM0_LOC);
			else if ((GBL_GRVers > 5) && (GBL_GRVers <= 8 ))
				print_GU1_loc(GU1_004_GE7_SRAM0_LOC);
			else {
#if IP22
				if (is_indyelan())
					print_GR3_loc(GR3_GE7_SRAM0_LOC_INDY);
				else
#endif
					print_GR3_loc(GR3_GE7_SRAM0_LOC);
				}
			}
				     
		    if ((rdval & 0xff00) != (pattern & 0xff00)) 
			{
			if (GBL_GRVers < 4)
				print_GR2_loc(GE7_SRAM1_LOC);
			else if (GBL_GRVers == 5) 
				print_GU1_loc(GU1_GE7_SRAM1_LOC);
			else if ((GBL_GRVers > 5) && (GBL_GRVers <= 8 ))
				print_GU1_loc(GU1_004_GE7_SRAM1_LOC);
			else {
#if IP22
				if (is_indyelan())
					print_GR3_loc(GR3_GE7_SRAM1_LOC_INDY);
				else
#endif
					print_GR3_loc(GR3_GE7_SRAM1_LOC);
				}
			}

		    if ((rdval & 0xff0000) != (pattern & 0xff0000)) 
			{
			if (GBL_GRVers < 4)
				print_GR2_loc(GE7_SRAM2_LOC);
			else if (GBL_GRVers == 5) 
				print_GU1_loc(GU1_GE7_SRAM2_LOC);
			else if ((GBL_GRVers > 5) && (GBL_GRVers <= 8 ))
				print_GU1_loc(GU1_004_GE7_SRAM2_LOC);
			else {
#if IP22
				if (is_indyelan())
					print_GR3_loc(GR3_GE7_SRAM2_LOC_INDY);
				else
#endif
					print_GR3_loc(GR3_GE7_SRAM2_LOC);
				}
			}
		    if ((rdval & 0xff000000) != (pattern & 0xff000000)) 
			{
			if (GBL_GRVers < 4)
				print_GR2_loc(GE7_SRAM3_LOC);
			else if (GBL_GRVers == 5) 
				print_GU1_loc(GU1_GE7_SRAM3_LOC);
			else if ((GBL_GRVers > 5) && (GBL_GRVers <= 8 ))
				print_GU1_loc(GU1_004_GE7_SRAM3_LOC);
			else {
#if IP22
				if (is_indyelan())
					print_GR3_loc(GR3_GE7_SRAM3_LOC_INDY);
				else
#endif
					print_GR3_loc(GR3_GE7_SRAM3_LOC);
		  		}
			}
	      	}
		error++;
	    }
	}
	return error;
}

/*
 *	Flood memory with pattern, then read back.
 */
int
checkpat(volatile uint *array, unsigned int pattern, int ramtype,
	 int offset, int size)
{
	unsigned int i,rdval;
	int error;
	error = 0;

	msg_printf(VRB,"Checking with pattern %#08x\n", pattern);
	for (i = offset; i < offset+size;i++){ 
		array[i] = pattern;
	}
	for (i = offset; i < offset+size;i++){ 
		rdval = array[i];
		error += compdata(rdval,pattern,ramtype,i);
	}
	return error;
}

/*
 *	Flood FIFO with pattern, then read back.
 */
int
FIFOcheckpat(unsigned int  pattern)
{
	unsigned int  i,offset,rddata;
	int error;
	error = 0;


	msg_printf(VRB,"Checking FIFO with pattern %#08x\n", pattern);
	for (offset = 0; offset < RAMinfo[FIFO_TYPE].size; offset += FIFO_LIMIT) { 
		for (i = offset; i < offset+FIFO_LIMIT;i++){ 
			base->fifo[i] = pattern;
		}
		for (i = offset; i < offset+FIFO_LIMIT;i++){ 
			rddata = base->fifo[i];

			if (rddata != pattern) {
				msg_printf(ERR,"FIFO:	expected %#x, returned %#x\n",
					 pattern, rddata);
				switch(GBL_GRVers) {
					case 0:
					case 1:
					case 2:
					case 3:
				    		print_GR2_loc(HQ2_LOC);
						break;
					case 4:
					case 9:
					case 10:
					case 11:
#if IP22
						if (is_indyelan())
				    			print_GR3_loc(GR3_HQ2_LOC_INDY);
						else
#endif
				    			print_GR3_loc(GR3_HQ2_LOC);
						break;
					case 5:
					case 6:
					case 7:
					case 8:
				    		print_GU1_loc(GU1_HQ2_LOC);
						break;
					default:
						printf("Board id 12-15 undefined\n");
						break;
				}
				error++;				
			}
		}
	}
	return error;
}

/*
 *	Flood GE7URAM with pattern, then read back.
 */
GE7URAMcheckpat(unsigned int pattern)
{
	unsigned int i;
	int error;
	error = 0;

	msg_printf(VRB,"Checking GE7URAM with pattern %#08x\n", pattern);
	for (i = 0;i < RAMinfo[GE7URAM_TYPE].size; i++) { 
		GE7URAMwr(pattern,i);
	}

	/*Read values*/
	for (i = 0;i < RAMinfo[GE7URAM_TYPE].size; i++) { 
		error += GE7URAMrdcomp(pattern,i); 
	}
	return error;
}


/*
 *	Checking for high and low data bus bit faults.	
 *	Flood memory with incrementing pattern, then read back.
 *	Flood memory with one's complement of incrementing pattern, then read back.
 */
int
inc_complement(volatile uint *array, int ramtype, int offset, int size,
	       unsigned int pattern)
{

	unsigned int i,rdval,invert,inc_pattern;
	int error;
	error = 0;

   for (invert =0; invert < 2;invert++) {
	if (!invert) { 
        	msg_printf(VRB,"Checking %s with incrementing pattern\n",RAMinfo[ramtype].name);
		for (i =  offset,inc_pattern = pattern; i < size + offset;i++,inc_pattern++){ 
			array[i] = inc_pattern;
		}
	/*Read back and validate*/
		for (i =  offset,inc_pattern = pattern; i < size + offset;i++,inc_pattern++){ 
			rdval =  array[i];
			error += compdata(rdval,inc_pattern,ramtype,i);
		}
	}
	else {
        	msg_printf(VRB,"Checking %s with one's complement of incrementing pattern\n",RAMinfo[ramtype].name);
		for (i = offset,inc_pattern = pattern; i < size + offset;i++,inc_pattern++){ 
			array[i] = ~inc_pattern;
		}

	/*Read back and validate*/
		for (i =  offset,inc_pattern = pattern; i < size + offset;i++,inc_pattern++){ 
			rdval = array[i];
			error += compdata(rdval,~inc_pattern,ramtype,i);
		}
        }
   }
   return error;
}

/*
 *	Checking for high and low data bus bit faults.	
 *	Flood memory with incrementing pattern, then read back.
 *	Flood memory with one's complement of incrementing pattern, then read back.
 */
int
FIFOinc_complement(void)
{

	unsigned int i,rdval,invert,offset;
	int error;
	error = 0;

   	for (invert =0; invert < 2;invert++) {
	   if (!invert) { 
        	msg_printf(VRB,"Checking FIFO with incrementing pattern\n");
   		for (offset = 0; offset < RAMinfo[FIFO_TYPE].size; offset += FIFO_LIMIT) {
			for (i =  offset; i < FIFO_LIMIT+offset;i++){ 
				base->fifo[i] = i;
			}
			/*Read back and validate*/
			for (i =  offset; i < FIFO_LIMIT+offset;i++){ 
				rdval = base->fifo[i];
				if (rdval != i) {
					msg_printf(ERR,"FIFO:	expected %#x, returned %#x\n", 			
					i, rdval);
				switch(GBL_GRVers) {
					case 0:
					case 1:
					case 2:
					case 3:
				    		print_GR2_loc(HQ2_LOC);
						break;
					case 4:
					case 9:
					case 10:
					case 11:
#if IP22
						if (is_indyelan())
				    			print_GR3_loc(GR3_HQ2_LOC_INDY);
						else
#endif
				    			print_GR3_loc(GR3_HQ2_LOC);
						break;
					case 5:
					case 6:
					case 7:
					case 8:
				    		print_GU1_loc(GU1_HQ2_LOC);
						break;
					default:
						printf("Board id 12-15 undefined\n");
						break;
				}
				error++;
				}
			}
		}
	   }
	   else {
        	msg_printf(VRB,"Checking FIFO with one's complement of incrementing pattern\n");
   		for (offset = 0; offset < RAMinfo[FIFO_TYPE].size; offset += FIFO_LIMIT) {
			for (i =  offset; i < FIFO_LIMIT+offset;i++){ 
				base->fifo[i] = ~i;
		   	}

	/*Read back and validate*/
			for (i =  offset; i < FIFO_LIMIT+offset;i++){ 
				rdval = base->fifo[i];
				if (rdval != ~i) {
					msg_printf(ERR,"FIFO:	expected %#x, returned %#x\n", 
						~i, rdval);
				switch(GBL_GRVers) {
					case 0:
					case 1:
					case 2:
					case 3:
				    		print_GR2_loc(HQ2_LOC);
						break;
					case 4:
					case 9:
					case 10:
					case 11:
#if IP22
						if (is_indyelan())
				    			print_GR3_loc(GR3_HQ2_LOC_INDY);
						else
#endif
				    			print_GR3_loc(GR3_HQ2_LOC);
						break;
					case 5:
					case 6:
					case 7:
					case 8:
				    		print_GU1_loc(GU1_HQ2_LOC);
						break;
					default:
						printf("Board id 12-15 undefined\n");
						break;
				}
				error++;
				}
		   	}
		}
           }
   }
   return error;
}

/*
 *	Checking for high and low data bus bit faults.	
 *	Flood memory with incrementing pattern, then read back.
 *	Flood memory with one's complement of incrementing pattern, then read back.
 */
int
GE7URAMinc_complement(void)
{

	unsigned int i,invert;
	int error;
	error = 0;

   for (invert =0; invert < 2;invert++) {
	if (!invert) { 
        	msg_printf(VRB,"Checking GE7URAM with incrementing pattern\n");
		for (i =  0; i < RAMinfo[GE7URAM_TYPE].size;i++){ 
			GE7URAMwr(i,i);
		}
	/*Read back and validate*/
		for (i =  0; i < RAMinfo[GE7URAM_TYPE].size;i++){ 
			error += GE7URAMrdcomp(i,i);
		}
	}
	else {
        	msg_printf(VRB,"Checking GE7URAMwith one's complement of incrementing pattern\n");
		for (i = 0; i < RAMinfo[GE7URAM_TYPE].size;i++){ 
			GE7URAMwr(~i,i);
		}

	/*Read back and validate*/
		for (i =  0; i < RAMinfo[GE7URAM_TYPE].size;i++){ 
			error += GE7URAMrdcomp(~i,i);
		}
        }
   }
   return error;
}


/* 
 *	Flood memory with each cell's memory mapped address, 
 *	then read back and validate.
 */
int
celladdr(volatile uint *array, int ramtype, int offset, int size)
{

	unsigned int  i,rdval;
	int error;
	error = 0;

	msg_printf(DBG,"Checking %s by flooding memory with each cell's address\n",RAMinfo[ramtype].name);
		for (i= offset; i < size + offset;i++){ 
			array[i] = (unsigned int)(unsigned long) &(array[i]);
		}

	/*Read memory and validate */
		for (i = offset; i < size + offset;i++){ 
			rdval = array[i];
			error += compdata(rdval,array[i],ramtype,i);
		}
	return error;
}

/* 
 *	Flood memory with each cell's memory mapped address, 
 *	then read back and validate.
 */
__psint_t FIFOcelladdr(void)
{

	unsigned int i,offset;
	unsigned int rdval;
	int error;
	error = 0;

	msg_printf(DBG,"Checking FIFO by flooding memory with each cell's address\n");
   	for (offset =0; offset < RAMinfo[FIFO_TYPE].size; offset += FIFO_LIMIT) {
		for (i=offset; i < offset+FIFO_LIMIT;i++){ 
			base->fifo[i] = KDM_TO_MCPHYS(&(base->fifo[i]));
		}

	/*Read memory and validate */
		for (i = offset; i < offset+FIFO_LIMIT;i++){ 
			rdval = base->fifo[i];
			if (rdval != (unsigned long) &(base->fifo[i])) {
				msg_printf(ERR,"FIFO:	expected %#x, returned %#x\n",
					 (unsigned long) &(base->fifo[i]), rdval);
			if (GBL_GRVers < 4)
				    	print_GR2_loc(HQ2_LOC);
			else if ((GBL_GRVers > 4) && (GBL_GRVers <= 8 ))
				    		print_GU1_loc(GU1_HQ2_LOC);
			else {
#if IP22
						if (is_indyelan())
				    			print_GR3_loc(GR3_HQ2_LOC_INDY);
						else
#endif
				    			print_GR3_loc(GR3_HQ2_LOC);
			}
			error++;				
			}	

		}
	}
	return error;
}
/* 
 *	Flood memory with each cell's memory mapped address, 
 *	then read back and validate.
 */
int
GE7URAMcelladdr(void)
{

	unsigned int i;
	int error;
	error = 0;

	msg_printf(DBG,"Checking GE7 URAM by flooding memory with each cell's address\n");
		for (i= 0; i < RAMinfo[GE7URAM_TYPE].size;i++){ 
			GE7URAMwr(KDM_TO_MCPHYS(&(base->fifo[i])),i);
		}

	/*Read memory and validate */
		for (i = 0; i < RAMinfo[GE7URAM_TYPE].size;i++){ 
			error += GE7URAMrdcomp(KDM_TO_MCPHYS(&(base->fifo[i])),i);	
		}
	return error;
}

/*
 * Foreground/background: Check address bus for bit faults.
 * Flood memory with zero. 
 * Write memory-mapped address to first cell,  then read back entire memory. 
 * All but one cell  should have zeros,  and that one cell has its 
 *	address stored as data.
 * Repeat for following cells. 
 */

int
fgbg(volatile unsigned int *array, int ramtype, int size)
{

	unsigned int i,j;
	unsigned int rdval;
	int error;
	error = 0;

	msg_printf(DBG,"Checking %s with fgbg test -- clearing RAM to 0x0, then writing (0xffffffff) to one location\n",RAMinfo[ramtype].name);
	for (j= 0; j < size;j++){ 
		msg_printf(VRB,".");
		for (i= 0; i < size; i++){ 
			array[i] = 0;
		}
		array[j] = 0xffffffff;

		/*Read and compare*/
		for (i = 0; i < size; i++){ 
			rdval = array[i];
			if (i == j) 
			    error += compdata(rdval,array[j],ramtype,i);
			 else 
			    error += compdata(rdval,0,ramtype,i);
		}
	}
	msg_printf(SUM,"\n");

	msg_printf(DBG,"Checking %s with fgbg test -- clearing RAM to 0xffffffff, then writing (0x0) to one location\n",RAMinfo[ramtype].name);
	for (j= 0; j < size;j++){ 
		msg_printf(VRB,".");
		for (i= 0; i < size; i++){ 
			array[i] = 0xffffffff;
		}
		array[j] = 0x0;

		/*Read and compare*/

		for (i = 0; i < size; i++){ 
			rdval = array[i];
			if( i != j)
			    error += compdata(rdval,array[i],ramtype,i);
			 else 
			    error += compdata(rdval,0,ramtype,i);
		}
	}
	msg_printf(SUM,"\n");
	return error;
}

/*
 * Foreground/background: Check address bus for bit faults.
 * Flood memory with zero. 
 * Write offset address to first cell,  then read back entire memory. 
 * All but one cell should have zeros,  and that one cell has its 
 * 	address stored as data.
 * Repeat for following cells. 
 */

int
GE7URAMfgbg(void)
{

	unsigned int i,j;
	int error = 0;

	msg_printf(DBG,"Checking GE7URAM with fgbg test -- clearing RAM to 0x0, then writing (0xffffffff) to one location\n");
	for (j= 0; j <  RAMinfo[GE7URAM_TYPE].size;j++){ 

		for (i= 0; i < RAMinfo[GE7URAM_TYPE].size;i++){ 
			GE7URAMwr(0,i);
		}
		GE7URAMwr(0xffffffff,j);

		/*Read and compare*/

		for (i = 0; i < RAMinfo[GE7URAM_TYPE].size;i++){ 
			if (i == j) {
				error += GE7URAMrdcomp(0xffffffff,j);
			} else {
				error += GE7URAMrdcomp(0,i);
			}
		}

	}
	msg_printf(DBG,"Checking GE7URAM with fgbg test -- clearing RAM to (0xffffffff), then writing (0x0) to one location\n");
	for (j= 0; j <  RAMinfo[GE7URAM_TYPE].size;j++){ 
		for (i= 0; i < RAMinfo[GE7URAM_TYPE].size;i++){ 
			GE7URAMwr(0xffffffff,i);
		}
		GE7URAMwr(0,j);

		/*Read and compare*/
		for (i = 0; i < RAMinfo[GE7URAM_TYPE].size;i++){ 
			if (i == j) {
				error += GE7URAMrdcomp(0,j);
			} else {
				error += GE7URAMrdcomp(0xffffffff,i);
			}
		}
	}
	return error;
}

int
gr2_wrRAM (int argc, char *argv[])
{
        unsigned int i, ramtype;
	unsigned int offset,pattern,size,rdcomp,scope;
	unsigned int rdval;
        

        if (argc < 4)
        {
		msg_printf(SUM, "%s writes to range\n", argv[0]);
		msg_printf(SUM, "usage: %s RAMtype hex_offset hex_pattern  <size> <scope = 1> <read and compare =  1>\n", argv[0]);
		RAMmenu();
			 
                return -1;
        }

        ramtype = atoi(*(++argv));
	if (ramtype > ALLRAM) {
		msg_printf(ERR, "Incorrect 2nd argument, should be RAMtype\n");
		return -1;
	}
		
        atohex(*(++argv), (int *)&offset);
        atohex(*(++argv), (int *)&pattern);

        if (argc >= 5)  
		size = atoi(*(++argv));
	else size = 1;

        if (argc >= 6)  
		scope= atoi(*(++argv));
	else scope = 0;

        if (argc >= 7)  
		rdcomp = atoi(*(++argv));
	else rdcomp = 0;	/*FALSE*/

   for (i=0; i < size ; i++,offset++) {
	switch(ramtype) { 
		case SHRAM_TYPE:
                        msg_printf(VRB,"Loop#%d, Writing SHRAM %#x:	%#x --- \n",i,offset,pattern);
			if (scope){
                                while (1) {
					base->shram[offset] = pattern;
                                	msg_printf(VRB,".");
                                }
                        }
                        else
				base->shram[offset] = pattern;
			if (rdcomp) {
				rdval = base->shram[offset];
                		if (rdval != pattern) {
                        		msg_printf(ERR,"	returned %#x\n", rdval);
                		} else
                        		msg_printf(VRB,"	returned %#x\n", rdval);
			}
			msg_printf(VRB,"\n");
	
                        break;
		case HQ2URAM_TYPE:
                        msg_printf(VRB,"Loop#%d, Writing HQ2 URAM %#x:	%#x --- \n",i,offset,pattern & 0xffffff);
			if (scope){
                                while (1) {
					base->hqucode[offset] = pattern & 0xffffff;
                                	msg_printf(VRB,".");
                                }
                        } else
				base->hqucode[offset] = pattern & 0xffffff;
			if (rdcomp) {
				rdval = base->hqucode[offset];
                		if ((rdval & 0xffffff) != (pattern & 0xffffff)) {
                        		msg_printf(ERR,"	returned %#x\n", rdval & 0xffffff);
                		} else
                        		msg_printf(VRB,"	returned %#x\n", rdval & 0xffffff);
			}
			msg_printf(VRB,"\n");
                        break;
		case FIFO_TYPE:
                        msg_printf(VRB,"Loop#%d, Writing FIFO %#x:	%#x ---\n",i,offset,pattern);
			if (scope){
                                while (1) {
					base->fifo[offset] = pattern;
                                	msg_printf(VRB,".");
                                }
                        } else
				base->fifo[offset] = pattern;

			if (rdcomp) {
				rdval = base->fifo[offset];
                		if (rdval != pattern) {
                        		msg_printf(ERR,"	returned FIFO %#x\n", rdval);
                		} else
                        		msg_printf(VRB,"	returned FIFO %#x\n", rdval);

			}
			msg_printf(VRB,"\n");
                        break;
		case GE7URAM_TYPE:
                        msg_printf(VRB,"Loop#%d, Writing GE7 URAM %#x:	%#x ---\n",i,offset,pattern);
			if (scope){
                                while (1) {
					GE7URAMwr(pattern,offset);
                                	msg_printf(VRB,".");
                                }
                        }
                        else
				GE7URAMwr(pattern,offset);
			if (rdcomp) {
				GE7URAMrdcomp(pattern,offset);
			}
                        break;
                case GE_0:
                case GE_1:
                case GE_2:
                case GE_3:
                case GE_4:
                case GE_5:
                case GE_6:
                case GE_7:
			numge = Gr2ProbeGEs(base);
			if ((ramtype +1) > numge) return 0;
                        msg_printf(VRB,"Loop #%d, Writing GE_%d RAM0 %#x:	%#x --- \n",i, ramtype,offset,pattern);
			if (scope){	
				while (1) {
				base->ge[ramtype].ram0[offset] = pattern;
				msg_printf(VRB,".");
				}
			}
			else 
				base->ge[ramtype].ram0[offset] = pattern;

			if (rdcomp) {
				rdval = base->ge[ramtype].ram0[offset];
                		if (rdval != pattern) {
                        		msg_printf(ERR,"	returned %#x\n", rdval);
                		} else
                        		msg_printf(VRB,"	returned %#x\n", rdval);
			}
			msg_printf(VRB,"\n");
                        break;
		case ALLRAM:
			msg_printf(ERR,"Can only write to one RAM at a time.\n");
			return -1;
	}
    }
	msg_printf(VRB,"\n");
    return 0;
}
#define HEX 0
#define FLT 1 
void
printRAM(volatile uint *array, int size, int offset, int format,
	 int across, int scope)
{
	unsigned int rdval;
        int i,location;

	for (i=offset,location = 0; i < offset+size;i++,location++) {

		/* Stop scrolling after 16 lines*/
                if (((location+across) % (across *16)) == 0){
                	msg_printf(SUM, "\nPress any key to continue. Ctl-C to exit routine. \n");
                       if (getchar() == 0x03) {
                       		return;
                       }
                }


		/*Start new line after certain number of data across*/ 
                if (location == 0)  
                	msg_printf(SUM, "%#x:  ",i);
		else if (((location+across) % across) == 0) 
                	msg_printf(SUM, "\n%#x:  ",i);
		if (scope){
			while (1) {
				rdval = array[i];
				msg_printf(SUM,".");
			}
		}
		else rdval = array[i];

                if (format == HEX){
                	msg_printf(SUM, "%#08x        ",rdval);
                }
                else if (format == FLT){
               		msg_printf(SUM, "%g   ",rdval)
;
         	}
         }
         msg_printf(SUM, "\n");
}

void
printGE7URAM(int size, int offset, int format, int scope)
{
	int i,location;
	unsigned int rddata0, rddata1,rddata2, rddata3,rduload;
	unsigned int bit_21, bits25_0_toHQ;


       msg_printf(SUM, "	[25:0] to HQ	[102:96]	[95:64] 	[63:32]		[31:0]\n");

	for (i=offset,location=0; i < offset+size;i++,location++) {

		/* Stop scrolling after 16 lines*/
                if (((location+12) % 11) == 0){
                	msg_printf(SUM, "\nPress any key to continue. Ctl-C to exit routine. \n");
                       if (getchar() == 0x03) {
                       		return;
                       }
                }
		if (scope) {

                        while (1) {
        			base->hq.gepc = i;
        			rduload = base->hq.ge7loaducode;
        			rduload &= 0x3dfffff;
        			rddata0 = base->ge[0].ram0[248];
        			rddata1 = base->ge[0].ram0[249];
        			rddata2 = base->ge[0].ram0[250];
        			rddata3 = base->ge[0].ram0[251];
				rddata3 &= 0x7f;
                                msg_printf(SUM,".");    
                        }
                } else  {
        		base->hq.gepc = i;
        		rduload = base->hq.ge7loaducode;
        		rduload &= 0x3dfffff;
        		rddata0 = base->ge[0].ram0[248];
        		rddata1 = base->ge[0].ram0[249];
        		rddata2 = base->ge[0].ram0[250];
        		rddata3 = base->ge[0].ram0[251];
			rddata3 &= 0x7f;
		}
		bit_21 = (rddata2 & 0x2000000) >> 4; /*From Bit 89 of GE7UCODE*/

		bits25_0_toHQ =  bit_21 | rduload; 

                msg_printf(SUM, "\n%#x: ",i);

                if (format == HEX){
                	msg_printf(SUM, "%#08x     %#08x     %#08x     %#08x     %#08x\n",
			bits25_0_toHQ,rddata3, rddata2, rddata1,rddata0); 
		}
                else if (format == FLT){
                	msg_printf(SUM, "%g     %g     %g     %g     %g\n",
			bits25_0_toHQ,rddata3, rddata2, rddata1,rddata0);
         	}
         }
}

gr2_rdRAM(int argc, char *argv[])
{

        int ramtype, format, across, loop;
	unsigned int scope, size, offset;
	int i,RAMid;

        if (argc < 2)
        {
                msg_printf(SUM, "usage: %s RAMtype <hex_offset> <size> <scope = 1> <format (hex default=0) or flt=1 > <repeat count> <#data across screen(default = 4)>\n", argv[0]);
		RAMmenu();	
                return -1;
        }

        ramtype = atoi(*(++argv));
	if (ramtype > ALLRAM) {
                msg_printf(ERR, "Incorrect 2nd argument, should be RAMtype\n");
                return -1;
        }

	if (argc >= 3)  
        	atohex(*(++argv), (int *)&offset);
	else offset= 0;

	if (argc >= 4)  
        	atohex(*(++argv), (int *)&size);
	else size = 0;

	if (argc >= 5)  
        	atohex(*(++argv), (int *)&scope);
	else scope = 0;

	if (argc >= 6)  
        	format = atoi(*(++argv));
	else format = HEX;

	if (argc >= 7)  
        	loop = atoi(*(++argv));
	else loop = 1;

	if (argc >= 8)  
        	across = atoi(*(++argv));
	else across = 4;

	for (i=0;i< loop; i++) {
	if (ramtype == ALLRAM)
                RAMid = 0;
        else
                RAMid = ramtype;
	for (;RAMid < ramtype+1;RAMid++) { 

		if (offset >= RAMinfo[RAMid].size ){
			msg_printf(ERR,"%s has only %#x memory locations. Offset %#x is too large\n",
			RAMinfo[RAMid].name,RAMinfo[RAMid].size,offset);
			return -1;
		} 
		if (size <= 0 )
			size = RAMinfo[RAMid].size;
		if (offset+size > (RAMinfo[RAMid].size)) {
			size = RAMinfo[RAMid].size - offset;
			msg_printf(SUM,"For size given, would read past memory address for %s.\n",RAMinfo[RAMid].name);
			msg_printf(SUM,"Number of locations to be read is now  %d for this RAM\n", size);
		}
	
        switch (RAMid) {
		case SHRAM_TYPE:
			msg_printf(SUM, "\n\nSHRAM::	Loop %d\n",i);
			printRAM(base->shram, size, offset, format, across,
				 scope);
                        break;
		case HQ2URAM_TYPE:
			msg_printf(SUM, "\n\nHQ2 URAM::	Loop %d\n",i);
			printRAM(base->hqucode, size, offset, format, across,
				 scope);
                        break;
		case FIFO_TYPE:
			msg_printf(SUM, "\n\nFIFO::	Loop %d\n",i);
			printRAM(base->fifo, FIFO_LIMIT, offset, format,
				 across,scope);
                        break;
		case GE7URAM_TYPE:
			msg_printf(SUM, "\n\nGE7 URAM::	Loop %d\n",i);
			printGE7URAM(size,offset,format,scope);
                        break;
		case GE_0:
		case GE_1:
		case GE_2:
		case GE_3:
		case GE_4:
		case GE_5:
		case GE_6:
		case GE_7:
			numge = Gr2ProbeGEs(base);
			if ((ramtype +1) > numge) return 0;
			msg_printf(SUM, "\n\nGE_%d RAM0::	Loop %d\n",RAMid,i);
			printRAM(base->ge[RAMid].ram0, size, offset, format,
				 across, scope);
                        break;
		case ALLRAM:	
			break;
                default:
                        msg_printf(ERR,"RAM type not defined\n");
                        return -1;
                }
	}
	} /*End repeat loop*/
	return 0;
}

int gr2_testallRAM(void);

gr2_testRAM(int argc, char *argv[])
{
	unsigned int pattern,testtype;
	unsigned int inc_pattern, offset;
	volatile unsigned int rdval;
        int ramtype, repeat, size;
        int i, j, RAMid, loop, error;
	unsigned int *ptr1;
	unsigned char mask;

	ptr1 = (unsigned int *)PHYS_TO_K1(0x1f000004);
        

	if (argc < 2) {
        		msg_printf(SUM, "usage: %s ramtype <repeat factor > <testtype> <hex_offset> <size> <pattern for inc_complement test>\n",argv[0]);
			RAMmenu();
			msg_printf(SUM,"**4 writes, 4 reads to SHRAM = %d\n",WRRD4_SHRAM);
			msg_printf(SUM,"**4 writes, 4 reads,no comparing  = %d\n",WRRD4_NOCOMP);
			msg_printf(SUM,"**1 write, 1 read to shram[1]= %d\n",WRRD1_SHRAM_1);
			testmenu();
			return -1;
	}

	gr2_boardvers();
        ramtype = atoi(*(++argv));
	if (argc >= 3) {
        	repeat = atoi(*(++argv));
	} else repeat = 1;
	if (argc >= 4) {
        	testtype = atoi(*(++argv));
	} else testtype = ALLTEST;
	if (argc >= 5) { 
		atohex(*(++argv), (int *)&offset);
	} else offset = 0;
	if (argc >= 6) {
        	size = atoi(*(++argv));
	} else size = 0;
	if (argc >= 7) {
		atohex(*(++argv), (int *)&inc_pattern);
	} else inc_pattern = 0;

	/* If all RAMS to be tested, loop thru all RAMS in switch stmt.*/

	for (i = 0; i < repeat; i++) {
			RAMid = ramtype;
	   for (;RAMid < ramtype+1; RAMid++) {
			if (RAMid > GE_7) {
				if (size <= 0) {
					size = RAMinfo[RAMid].size;
				} 
				if ((offset+size) >  RAMinfo[RAMid].size)  
					size =  RAMinfo[RAMid].size - offset;

			} else  {  /*RAM0 of all GE7 */
				if (size <= 0) {
					size = 128;	/*Can only check front buffer*/
				} 
				if ((offset+size) >  128)  
					size =  128 - offset;
			}
		switch(RAMid) {

		case SHRAM_TYPE:
			error = 0;
			msg_printf(SUM,"Testing SHRAM:\n");
			if ((testtype == ALLTEST) || (testtype == CHECKPAT)) {
				/* Fixed patterns: 0 0x55555555, 0xaaaaaaaa, 0xffffffff */
				for (loop = pattern = 0; loop < 4;loop++, pattern +=0x55555555){
					msg_printf(DBG, "Writing %#x to SHRAM ---\n",pattern);
					error += checkpat(base->shram,pattern,RAMid,offset,size);
				}
				/* Walking one test */
				for (loop = 0, pattern = 1; loop < 32;loop++, pattern <<= 1){
					msg_printf(DBG, "Writing %#x to SHRAM ---\n",pattern);
					error += checkpat(base->shram,pattern,RAMid,offset,size);
				}
			}
			if ((testtype == ALLTEST) || (testtype == INC_COMPLEMENT)) {
				error += inc_complement(base->shram,RAMid,offset,size,inc_pattern);
			}
			if ((testtype == ALLTEST) || (testtype == CELL_ADDR)) {
				error += celladdr(base->shram,RAMid,offset,size);
			}

			if (!error)
				msg_printf(SUM,"SHRAM tests PASSED\n"); 
			else 
				msg_printf(SUM,"SHRAM tests FAILED - %d errors\n",error); 
		break; /*end SHRAM_TYPE*/


		case HQ2URAM_TYPE:
			msg_printf(SUM,"Testing HQ2 URAM\n");
			error = 0;
			if ((testtype == ALLTEST) || (testtype == CHECKPAT)) {
				/* Fixed patterns */
				for (loop = pattern = 0; loop < 4;loop++, pattern +=0x55555555){
					msg_printf(DBG, "Writing %#x to HQ2URAM ---\n",pattern & 0xffffff);
					error += checkpat(base->hqucode,pattern & 0xffffff,RAMid,offset,size);
				}
				/* Walking one */
				for (loop = 0, pattern = 1; loop < 32;loop++, pattern <<= 1){
					msg_printf(DBG, "Writing %#x to HQ2URAM ---\n",pattern & 0xffffff);
					error += checkpat(base->hqucode,pattern & 0xffffff,RAMid,offset,size);
				}
			}
			if ((testtype == ALLTEST) || (testtype == INC_COMPLEMENT)) {
				error += inc_complement(base->hqucode,RAMid,offset,size,inc_pattern);
			}
			if ((testtype == ALLTEST) || (testtype == CELL_ADDR)) {
				error += celladdr(base->hqucode,RAMid,offset,size);
			}
			if (!error)
				msg_printf(SUM,"HQ2 URAM tests PASSED\n"); 
			else 
				msg_printf(SUM,"HQ2 URAM tests FAILED - %d errors\n",error); 

		break; /*end HQ2URAM*/

		case FIFO_TYPE:
			msg_printf(SUM,"Resetting graphics and writing 4 dummy values into HQ2 fifo\n");

			/*
			 * RESET the graphics board and STALL the ucode.
         		 */
#ifdef IP20
			gr2_reset();
#else
			mask = (base == GR2_GFX_BASE) ? PCON_SG_RESET_N :
							PCON_S0_RESET_N ;
			*(volatile unsigned char *)PHYS_TO_K1(PORT_CONFIG) &=
				~mask;
			flushbus();
			DELAY(10);
			*(volatile unsigned char *)PHYS_TO_K1(PORT_CONFIG) |=
				mask;
#endif
        		flushbus();
			for (j=0;j< 4;j++) {
                		base->fifo[0] = 0;
        		}

			msg_printf(SUM,"Testing FIFO \n");
			error = 0;
			
			msg_printf(VRB,"Always using offset 0, and checking entire FIFO \"address range\"\n");
			if ((testtype == ALLTEST) || (testtype == CHECKPAT)) {
				/* Fixed patterns */
				for (loop = pattern = 0; loop < 4;loop++, pattern +=0x55555555){
					msg_printf(DBG, "Writing %#x to FIFO --- \n",pattern);
					error += FIFOcheckpat(pattern);
				}
				/* Walking one */
				for (loop = 0, pattern=1; loop < 32;loop++, pattern <<= 1){
					msg_printf(DBG, "Writing %#x to FIFO --- \n",pattern);
					error += FIFOcheckpat(pattern);
				}
			}
			if ((testtype == ALLTEST) || (testtype == INC_COMPLEMENT)) {
			
			msg_printf(VRB,"For the FIFO incrementing & complement pattern test, user can't change the test parameters\n");
				error += FIFOinc_complement();
			}
			if ((testtype == ALLTEST) || (testtype == CELL_ADDR)) {
				error += FIFOcelladdr();
			}
			if (!error)
				msg_printf(SUM,"FIFO tests PASSED\n"); 
			else 
				msg_printf(SUM,"FIFO tests FAILED - %d errors\n",error); 

		break; /*end FIFO*/

		case GE7URAM_TYPE:
			msg_printf(SUM,"Resetting the graphics before running this test\n");
			/*
			 * RESET the graphics board and STALL the ucode.
         		 */
#ifdef IP20
			gr2_reset();
#else
			mask = (base == GR2_GFX_BASE) ? PCON_SG_RESET_N :
							PCON_S0_RESET_N ;
			*(volatile unsigned char *)PHYS_TO_K1(PORT_CONFIG) &=
				~mask;
        		flushbus();
        		DELAY(10);
			*(volatile unsigned char *)PHYS_TO_K1(PORT_CONFIG) |=
				mask;
#endif
        		flushbus();
			msg_printf(SUM,"Testing GE7URAM\n");
			error = 0;
			msg_printf(VRB,"Always using offset 0, and writing to entire GE7URAM\n");
			if ((testtype == ALLTEST) || (testtype == CHECKPAT)) {
				/* Fixed patterns */
				for (loop = pattern = 0; loop < 4;loop++, pattern +=0x55555555){
					msg_printf(DBG, "Writing %#x to GE7URAM --- \n",pattern);
					error += GE7URAMcheckpat(pattern);
				}
				/* Walking one  */
				for (loop = 0, pattern = 1; loop < 32;loop++, pattern <<= 1){
					msg_printf(DBG, "Writing %#x to GE7URAM --- \n",pattern);
					error += GE7URAMcheckpat(pattern);
				}
			}

			if ((testtype == ALLTEST) || (testtype == INC_COMPLEMENT)) {
				msg_printf(VRB,"For the GE7URAM incrementing & complement pattern test, user can't change the test parameters\n");
				error += GE7URAMinc_complement();
			}
			if ((testtype == ALLTEST) || (testtype == CELL_ADDR)) {
			error += GE7URAMcelladdr();
			}
			if (!error)
				msg_printf(SUM,"GE7 URAM tests PASSED\n"); 
			else 
				msg_printf(SUM,"GE7 URAM tests FAILED - %d errors\n",error); 

		break; /*end GE7URAM*/

		case GE_0:
		case GE_1:
		case GE_2:
		case GE_3:
		case GE_4:
		case GE_5:
		case GE_6:
		case GE_7:
			numge = Gr2ProbeGEs(base);
			if ((ramtype+1) > numge) return 0;
			msg_printf(SUM,"Only can test front buffer GE_%d RAM0\n",RAMid);
			error = 0;
	
			if ((testtype == ALLTEST) || (testtype == CHECKPAT)) {
				/* Fixed patterns */
				for (loop = pattern = 0; loop < 4;loop++, pattern +=0x55555555){
					msg_printf(DBG, "Writing %#x to GE_%d RAM0 --- \n",pattern,RAMid);
					error += checkpat(base->ge[RAMid].ram0,pattern,RAMid,offset,size);
				}
				/* Walking one */
				for (loop = 0, pattern = 1; loop < 32;loop++, pattern <<= 1){
					msg_printf(DBG, "Writing %#x to GE_%d RAM0 --- \n",pattern,RAMid);
					error += checkpat(base->ge[RAMid].ram0,pattern,RAMid,offset,size);
				}
			}
			if ((testtype == ALLTEST) || (testtype == INC_COMPLEMENT)) {
				error += inc_complement(base->ge[RAMid].ram0,RAMid,offset,size,inc_pattern);
			}
	
			if ((testtype == ALLTEST) || (testtype == CELL_ADDR)) {
				error += celladdr(base->ge[RAMid].ram0,RAMid,offset,size);
			}
	
			if (!error)
				msg_printf(SUM,"GE_%d RAM0 tests PASSED\n",RAMid); 
			else 
				msg_printf(SUM,"GE_%d RAM0 tests FAILED - %d errors\n",RAMid,error); 
		break;
		case ALLRAM:
			msg_printf(SUM,"ALLRAM test.... \n",RAMid);
		if ((error=gr2_testallRAM()) == 0)
			msg_printf(VRB,"AllRAM test passed...\n");
		else 
			msg_printf(VRB,"ALLRAM test failed...\n");
		break;
		case WRRD4_NOCOMP:
                        msg_printf(SUM,"4 writes/4 reads of 0xffffffff to SHRAM:\n");
                        msg_printf(SUM,"No counting of errors. No comparing.\n");
                        base->shram[0] = 0xffffffff;
                        base->shram[1] = 0xffffffff;
                        base->shram[2] = 0xffffffff;
                        base->shram[3] = 0xffffffff;

                        rdval = base->shram[0];
                        rdval = base->shram[1];
                        rdval = base->shram[2];
                        rdval = base->shram[3];

                break; 
		case WRRD4_SHRAM:
		error = 0;
                        msg_printf(SUM,"Testing SHRAM:\n");
                        msg_printf(DBG, "Writing %#x to SHRAM ---\n",0xffffffff);
                        error += checkpat(base->shram,0xffffffff,RAMid,0,4);

                        if (!error)
                                msg_printf(SUM,"SHRAM tests PASSED\n");
                        else
                                msg_printf(SUM,"SHRAM tests FAILED - %d errors\n",error);
                break; 
		case WRRD1_SHRAM_1:
{
		*ptr1 = 0xffffffff;
		rdval = *ptr1;
}
                break; 

		} /*end switch*/
	      if (!error) return(0); 
	   }
	}
	return error;
}


/*ARGSUSED*/
/* Long test*/
int
gr2_fgbgRAM(int argc, char *argv[])
{
#ifdef MFG_USED
	int ramtype, repeat;
        int i, RAMid, error;

        if (argc < 2) {
                        msg_printf(SUM, "usage: %s ramtype <repeat> \n",argv[0]);
                        msg_printf(SUM, "IMPT: For large RAMS (SHRAM and FIFO), run this test overnight\n");
                        msg_printf(SUM, "and set report = 2 to report only ERR msgs.\n");
                        msg_printf(SUM, "Otherwise the dots printed to the screen will slow down the tests\n");
			 RAMmenu();
                        return -1;
        }

        ramtype = atoi(*(++argv));
        if (argc >=3) {
                repeat = atoi(*(++argv));
        }
        else repeat = 1;



	
	for (i=0; i < repeat; i++) {
		if (ramtype == ALLRAM)
                	RAMid = 0;
        	else
                	RAMid = ramtype;
           for (;RAMid < ramtype+1; RAMid++) {
		switch (RAMid) {
			case SHRAM_TYPE:
                        	msg_printf(SUM,"Testing SHRAM with fgbg test\n");
				error = fgbg(base->shram,RAMid,RAMinfo[RAMid].size);

				if (!error)
                        		msg_printf(SUM,"SHRAM fgbg test PASSED\n");
                		else
                        		msg_printf(SUM,"SHRAM fgbg test FAILED - %d errors\n",error);
			break;

			case HQ2URAM_TYPE:
                        	msg_printf(SUM,"Testing HQ2 URAM with fgbg test\n");
				error = fgbg(base->hqucode,RAMid,RAMinfo[RAMid].size);
				if (!error)
                        		msg_printf(SUM,"HQ2 URAM fgbg test PASSED\n");
                		else
                        		msg_printf(SUM,"HQ2 URAM fgbg test FAILED - %d errors\n",error);
			break;

			case FIFO_TYPE:
                        	msg_printf(SUM,"Don't use fgbg test on FIFO\n");
			break;

			case GE7URAM_TYPE:
                        	msg_printf(SUM,"Testing GE7 URAM with fgbg test\n");
				error = GE7URAMfgbg();
				if (!error)
                        	msg_printf(SUM,"GE7 URAM fgbg test PASSED\n");
                	else
                        	msg_printf(SUM,"GE7 URAM fgbg test FAILED - %d errors\n",error);
			break;

			case GE_0:
			case GE_1:
			case GE_2:
			case GE_3:
			case GE_4:
			case GE_5:
			case GE_6:
			case GE_7:

                        	msg_printf(SUM,"Testing GE_%d RAM0 with fgbg test\n",RAMid);
				error = fgbg(base->ge[RAMid].ram0,RAMid,128); /*Can only check front buffer*/
				if (!error)
                        		msg_printf(SUM,"GE_%d RAM0 fgbg test PASSED\n",RAMid);
			 	else
                        		msg_printf(SUM,"GE_%d RAM0 fgbg test FAILED - %d errors\n",RAMid,error);
			break;
		
			case ALLRAM:
			break;
		}
		if (!error) return(0);
	    }
	}
#else
msg_printf(SUM, "%s is available in manufacturing scripts only\n", argv[0]);
return 0;
#endif /* MFG_USED */
}


/*ARGSUSED*/
int
gr2_rdram12(int argc, char *argv[])
{
#ifdef MFG_USED
    int	    geid, ramid, adr, val, cnt;
    register	    i;
    register	    n;

    if (argc == 1) {
	msg_printf(SUM, "Usage: rdram12 geid(0-7), [ramid(1-2)], [adr(0-63)], [cnt(1-64)]\n");
	return(-1);
    }

    if (argc == 2) {	/* dump GE */
	geid = atoi(*(++argv));
	if ((geid < 0) || (geid > 7)) return(-1);

	for(ramid = 1; ramid <= 2; ramid++) {
	    base->ge[geid].ram0[0xFD] = ramid;	
	    for (adr = 0; adr < 64; adr += 4) {
		if (adr == 32) msg_printf(SUM, "\n");
		msg_printf(SUM, "\nRAM%d[%2d] = ", ramid, adr);
		for(i=0; i<4; i++) {
		    val = base->ge[geid].ram0[adr+i];
		    msg_printf(SUM, "0x%-8x,  ", val);
		}
	    }
	    base->ge[geid].ram0[0xFD] = 0;	
	}
    }
    else if (argc == 3) {		/* dump only the specified RAM */
	geid = atoi(*(++argv));
	ramid = atoi(*(++argv));
	if ((geid < 0) || (geid > 7)) return(-1);
	if ((ramid < 1) || (ramid > 2)) return(-1);
	base->ge[geid].ram0[0xFD] = ramid;	
	for (adr = 0; adr < 64; adr += 4) {
	    if (adr == 32) msg_printf(SUM, "\n");
	    msg_printf(SUM, "\nRAM%d[%2d] = ", ramid, adr);
	    for(i=0; i<4; i++) {
		val = base->ge[geid].ram0[adr+i];
		msg_printf(SUM, "0x%-8x,  ", val);
	    }
	}
	base->ge[geid].ram0[0xFD] = 0;	
    }
    else if (argc == 4) {		/* dump only specified location */
	geid = atoi(*(++argv));
	ramid = atoi(*(++argv));
	adr = atoi(*(++argv));
	if ((geid < 0) || (geid > 7)) return(-1);
	if ((ramid < 1) || (ramid > 2)) return(-1);
	if ((adr < 0) || (adr > 63)) return(-1);
	base->ge[geid].ram0[0xFD] = ramid;	
	val = base->ge[geid].ram0[adr];
	msg_printf(SUM, "RAM%d[%d] = %d, 0x%x\n", ramid, adr, val,
						      val);
	base->ge[geid].ram0[0xFD] = 0;	
    }
    else if (argc == 5) {		/* dump count locations from adr */
	geid = atoi(*(++argv));
	ramid = atoi(*(++argv));
	adr = atoi(*(++argv));
	cnt = atoi(*(++argv));
	if ((geid < 0) || (geid > 7)) return(-1);
	if ((ramid < 1) || (ramid > 2)) return(-1);
	if ((adr < 0) || ((adr+cnt) > 63)) return(-1);
	base->ge[geid].ram0[0xFD] = ramid;	
	for (; cnt > 0; cnt -= 4) {
	    msg_printf(SUM, "\nRAM%d[%2d] = ", ramid, adr);
	    for(i=0; i<4; i++) {
		if (i > cnt) break;
		val = base->ge[geid].ram0[adr++];
		msg_printf(SUM, "0x%-8x, ", val);
	    }
	}
	base->ge[geid].ram0[0xFD] = 0;	
    }
    msg_printf(SUM, "\n");
#else
msg_printf(SUM, "%s is available in manufacturing scripts only\n", argv[0]);
#endif /* MFG_USED */
    return 0;
}

int
gr2_testallRAM(void)
{
	int errors;
	int RAMid, loop,pattern;

	errors =0;

	/* Test Shram ram */

	msg_printf(VRB,"Testing SHRAM:\n"); 

	RAMid = SHRAM_TYPE;
	for (loop = pattern = 0; loop < 4;loop++, pattern +=0x55555555)
		errors += checkpat(base->shram,pattern,RAMid,0,RAMinfo[RAMid].size);

/*	errors += inc_complement(base->hqucode,RAMid,0,RAMinfo[RAMid].size,0); */

/*	errors += celladdr(base->shram,RAMid,0,RAMinfo[RAMid].size); */

	if (errors == 0) msg_printf(VRB,"SHRAM test passed...\n");
	else msg_printf(VRB,"SHRAM test failed...\n");	

	return errors;

}
