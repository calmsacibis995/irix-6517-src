
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1991, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include "ide_msg.h"
#include "sys/gr2hw.h"
#include "sys/param.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "diag_glob.h"
#include "diagcmds.h"
#include "gr2.h"
#include "dma.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "gr2.h"

extern struct gr2_hw *base;
extern int GBL_VMAVers;
extern int GBL_VMBVers;
extern int GBL_VMCVers;

#define	DMAREAD		0
#define	DMAWRITE	1

#define	PIXPACK(r,g,b)	(r | (g << 8) | (b << 16))

#define NumberOf(x) (sizeof(x) / sizeof((x)[0]))
static int numge;

static int vert[][2] = {
    100, 100,
    100, 150,
    100, 200,
    100, 250,
    100, 300,
    100, 350,
    100, 400,
    100, 450
};

static int col[][3] = {
    255, 0, 0,
    0, 255, 0,
    0, 0, 255,
    255, 255, 0,
    255, 0, 255,
    0, 255, 255, 
    255, 255, 255,
    150, 150, 150,
};

static int seedrom[128] = { 
	0x7F,0x7E,0x7C,0x7A,0x78,0x76,0x75,0x73,
	0x71,0x6F,0x6D,0x6C,0x6A,0x68,0x67,0x65,
	0x64,0x62,0x60,0x5F,0x5D,0x5C,0x5A,0x59,
	0x58,0x56,0x55,0x53,0x52,0x51,0x4F,0x4E,
	0x4D,0x4C,0x4A,0x49,0x48,0x47,0x45,0x44,
	0x43,0x42,0x41,0x40,0x3F,0x3D,0x3C,0x3B,
	0x3A,0x39,0x38,0x37,0x36,0x35,0x34,0x33,
	0x32,0x31,0x30,0x2F,0x2E,0x2D,0x2C,0x2C,
	0x2B,0x2A,0x29,0x28,0x27,0x26,0x25,0x25,
	0x24,0x23,0x22,0x21,0x21,0x20,0x1F,0x1E,
	0x1E,0x1D,0x1C,0x1B,0x1B,0x1A,0x19,0x18,
	0x18,0x17,0x16,0x16,0x15,0x14,0x14,0x13,
	0x12,0x12,0x11,0x10,0x10,0x0F,0x0E,0x0E,
	0x0D,0x0D,0x0C,0x0B,0x0B,0x0A,0x0A,0x09,
	0x09,0x08,0x07,0x07,0x06,0x06,0x05,0x05,
	0x04,0x04,0x03,0x03,0x02,0x02,0x01,0x01
};


static int sqrom[128] = {
	0x35, 0x33, 0x32, 0x30, 0x2F, 0x2E, 0x2D, 0x2B, 
	0x2A, 0x29, 0x28, 0x27, 0x26, 0x25, 0x23, 0x22, 
	0x21, 0x20, 0x1F, 0x1E, 0x1E, 0x1D, 0x1C, 0x1B, 
	0x1A, 0x19, 0x18, 0x17, 0x16, 0x16, 0x15, 0x14, 
	0x13, 0x13, 0x12, 0x11, 0x10, 0x10, 0xF, 0xE, 
	0xE, 0xD, 0xC, 0xB, 0xB, 0xA, 0xA, 0x9, 
	0x8, 0x8, 0x7, 0x7, 0x6, 0x5, 0x5, 0x4, 
	0x4, 0x3, 0x3, 0x2, 0x2, 0x1, 0x1, 0x0, 
	0x7F, 0x7E, 0x7C, 0x7A, 0x78, 0x76, 0x74, 0x73, 
	0x71, 0x6F, 0x6E, 0x6C, 0x6A, 0x69, 0x67, 0x66, 
	0x64, 0x63, 0x62, 0x60, 0x5F, 0x5E, 0x5C, 0x5B, 
	0x5A, 0x59, 0x57, 0x56, 0x55, 0x54, 0x53, 0x52, 
	0x51, 0x4F, 0x4E, 0x4D, 0x4C, 0x4B, 0x4A, 0x49, 
	0x48, 0x47, 0x46, 0x45, 0x45, 0x44, 0x43, 0x42, 
	0x41, 0x40, 0x3F, 0x3E, 0x3E, 0x3D, 0x3C, 0x3B, 
	0x3A, 0x3A, 0x39, 0x38, 0x37, 0x37, 0x36, 0x35, 
};

int	gebus_data[] = {
	0x12345678, 0x9ABCDEF0, 0x0FEDCBA9, 0x87654321, 
/*XXX
	0xff00ff00, 0xee11ee11, 0xdd22dd22, 0xcc33cc33, 
*/
	0xbb44bb44, 0xaa55aa55, 0x99669966, 0x88778877,
	0x77887788, 0x66996699, 0x55aa55aa, 0x44bb44bb,
	0x33cc33cc, 0x22dd22dd, 0x11ee11ee, 0x00ff00ff
};

static int diag_finish(int delay);
void ClearStatus(int);

/**************************************************************************
*
*    Diagnostic routines to test GE7.
*    These routines can only be called after the microcode has been 
*    downloaded and HQ2 diagnostics have been performed.
*
**************************************************************************/

#ifdef MFG_USED
gr2_ge7test(int argc, char *argv[])
{
    int errors=0;

    /* Find out how many GEs installed */
    numge = Gr2ProbeGEs(base);

    if (gr2_ShramTest() != 0) {
	msg_printf(ERR, "GE7 Shared Ram test Failed\n");
	errors++;
    }
    else msg_printf(VRB, "GE7 Shared Ram test PASSED......\n");

    if (gr2_Ram12Test() != 0) {
	msg_printf(ERR, "GE7 internal Ram12 test Failed\n");
	errors++;
    }
    else msg_printf(VRB, "GE7 internal Rom12 test PASSED.....\n");
    if (gr2_SeedRomTest() != 0) {
	msg_printf(ERR, "GE7 Inverse Seed Rom test Failed\n");
	errors++;
    }
    else msg_printf(VRB, "GE7 Inverse Seed Rom test PASSED.....\n");

    if (gr2_SqRomTest() != 0) {
	msg_printf(ERR, "GE7 Square Root Rom test Failed\n");
	errors++;
    }
    else msg_printf(VRB, "GE7 Square Root Rom test PASSED.....\n");

    if (gr2_GeBusTest() != 0) {
	msg_printf(ERR, "GE7 gebus test Failed\n");
	errors++;
    }
    else msg_printf(VRB, "GE7 gebus test PASSED.....\n");

    if (gr2_GeFifoTest() != 0) {
	msg_printf(ERR, "GE7 Fifo test Failed\n");
	errors++;
    }
    else msg_printf(VRB, "GE7 Fifo test PASSED.....\n");

    if (gr2_FloatOpsTest() != 0) {
	msg_printf(ERR, "GE7 Floating point test Failed\n");
	errors++;
    }
   else  msg_printf(VRB, "GE7 floating point test PASSED.....\n");

    if (gr2_SpfCondTest() != 0) {
	msg_printf(ERR, "GE7  SPF mode test Failed\n");
	errors++;
    }
   else  msg_printf(VRB, "GE7 SPF mode test PASSED.....\n");

    if (gr2_FpuCondTest() != 0) {
	msg_printf(ERR, "GE7  FPU condition codes test Failed\n");
	errors++;
    }
   else  msg_printf(VRB, "GE7 FPU condition codes test PASSED.....\n");

  if (errors != 0) msg_printf(VRB, "GE7 test failed.....\n"); 
  else  msg_printf(VRB, "GE7 test PASSED.....\n");
  return errors;
}
#endif /* MFG_USED */


/***************************************************************************
*   Routine: ShramTest
*
*   Purpose: Tests the data path between GE7s and shared RAM.
*
*   History: 03/08/91 : Original Version. Vimal Parikh
*
*   Notes: The test sends a command to the Ge7. Each GE7 writes its unique
*	   id to shared ram location GE_STATUS. 
*
****************************************************************************/

int
gr2_ShramTest(void)
{
    register int i;
    int	adr, val;
    int	err;

    /* Find out how many GEs installed */
    numge = Gr2ProbeGEs(base);

    /* clear the shared RAM results area */
    ClearStatus(10);

    /* send command */
    msg_printf(DBG, "Shram Test : Sending command to GE7....\n");
    base->fifo[DIAG_GESHRAM] = 0;

    if (diag_finish(100) < 0) return(-1);

    /* now check the shared RAM for ids of all GE's */
    msg_printf(DBG, "Shram Test : Checking results....\n");
    adr = GE_STATUS + MAXCONS;
    err = 0;

/*    printf("ADR is -> %x\n", adr); */

    for(i=0; i<numge; i++) {
	val = base->shram[adr+i];
	if (val != i) {
	    msg_printf(ERR, "Shram test failed for GE7 #%d\n", i);
	    msg_printf(ERR, "Expected value = %d, actual = 0x%x\n", i, val);
	    err++;
	}
    }

    if (err == 0)
        msg_printf(VRB, "shram test PASSED.....\n");
    return(err);
}
    
/***************************************************************************
*   Routine: Ram12Test
*
*   Purpose: Tests the internal RAMs and registers of each GE7.
*
*   History: 03/08/91 : Original Version. Vimal Parikh
*
*   Notes: Each GE7 tests its own internal RAM by writing halftone patterns
*	   to each location of RAM1, RAM2 and Register file. At the end, it
*	   writes the results to shared RAM beginning from GE_STATUS. Each 
*	   GE7 writes 6 results (for RAM1, RAM2, Regfile repeated for two
*	   halftone patterns (0xA5A5A5A5 and 0x5A5A5A5A).
*
****************************************************************************/

int
gr2_Ram12Test(void)
{
    register int i;
    register int adr, err;

    /* Find out how many GEs installed */
    numge = Gr2ProbeGEs(base);

    ClearStatus(50);

    /* send command */
    base->fifo[DIAG_GERAM12] = 0;
    msg_printf(DBG, "GE7 Ram12 Test : Sending command to GE7....\n");
    /* wait for it to finish */
    if (diag_finish(1000) < 0) return(-1);
    /* now check the shared RAM for results from all GE's */
    msg_printf(DBG, "Ram12 Test : Checking results....\n");
    adr = GE_STATUS + MAXCONS;
    err = 0;

    for(i=0; i<numge; i++) {
	if (base->shram[adr+0] == 0) {
	   msg_printf(ERR, "Ram12 Test : Ram1 test failed for GE7 #%d\n", i);
	   err++;
	}
	if (base->shram[adr+1] == 0) {
	   msg_printf(ERR, "Ram12 Test : Ram1 test failed for GE7 #%d\n", i);
	   err++;
	}
	if (base->shram[adr+2] == 0) {
	   msg_printf(ERR, "Ram12 Test : Ram2 test failed for GE7 #%d\n", i);
	   err++;
	}
	if (base->shram[adr+3] == 0) {
	   msg_printf(ERR, "Ram12 Test : Ram2 test failed for GE7 #%d\n", i);
	   err++;
	}
	if (base->shram[adr+4] == 0) {
	   msg_printf(ERR, "Ram12 Test : RegFile test failed for GE7 #%d\n", i);
	   err++;
	}
	if (base->shram[adr+5] == 0) {
	   msg_printf(ERR, "Ram12 Test : RegFile test failed for GE7 #%d\n", i);
	   err++;
	}

	adr += 6;
    }
    if (err == 0)
        msg_printf(VRB, "ram12 test PASSED.....\n");
    return(err);
}

/***************************************************************************
*   Routine: SeedRom Test
*
*   Purpose: Tests the internal seed ROM for each GE7.
*
*   History: 03/08/91 : Original Version. Vimal Parikh
*
*   Notes: Each Ge7 has a 128 word long seed ROM for divide look-up. This
*	   test commands each GE7 to dump its seed rom to shared RAM where
*	   the host can verify it.
*
****************************************************************************/

int
gr2_SeedRomTest(void)
{
    register	i, j, val, adr;
    register	err;

    /* Find out how many GEs installed */
    numge = Gr2ProbeGEs(base);


    ClearStatus(128*8);

    /* send command */
    msg_printf(DBG, "GE7 SeedRom Test : Sending command to GE7....\n");
    base->fifo[DIAG_SEEDROM] = 0;

    /* wait for it to finish */
    if (diag_finish(2000) < 0) return(-1);

    /* now check the shared RAM for ids of all GE's */
    msg_printf(DBG, "GE7 SeedRom Test : Checking results....\n");
    adr = GE_STATUS + MAXCONS;
    err = 0;

    for(i=0; i<numge; i++, adr += NumberOf(seedrom)) {
	for(j = 0; j < NumberOf(seedrom); j++) {
	    val = base->shram[adr+j];
	    if (val != seedrom[j]) {
		msg_printf(ERR, "GE7 SeedRom Test: Expected = %d, actual = %d\n", 
			   				seedrom[j], val);
		err++;
	    }
	}

	if (err > 0) msg_printf(ERR, "GE7 SeedRom Test: Failed for GE7 #%d\n", i);
    }

    if (err == 0)
        msg_printf(VRB, "seedrom test PASSED.....\n");
    return(err);
}
    
/***************************************************************************
*   Routine: SqromTest
*
*   Purpose: Tests the Internal Square Root rom of GE7.
*
*   History: 03/08/91 : Original Version. Vimal Parikh
*
*   Notes: Each Ge7 has a 128 word long seed ROM for square root look-up. 
*	   This test commands each GE7 to dump its seed rom to shared RAM 
*	   where the host can verify it.
*
****************************************************************************/

int
gr2_SqRomTest(void)
{
    register	i, j, val, adr;
    register	err;


    /* Find out how many GEs installed */
    numge = Gr2ProbeGEs(base);

    ClearStatus(128*8);

    /* send command */
    msg_printf(DBG, "SqRom Test : Sending command to GE7....\n");
    base->fifo[DIAG_SQROM] = 0;

    /* wait for it to finish */
    if (diag_finish(2000) < 0) return(-1);

    /* now check the shared RAM for ids of all GE's */
    msg_printf(DBG, "GE7 SqRom Test : Checking results....\n");
    adr = GE_STATUS + MAXCONS;
    err = 0;

    for(i=0; i<numge; i++, adr += NumberOf(sqrom)) {
	for(j=0; j < NumberOf(sqrom); j++) {
	    val = base->shram[adr+j];
	    if (val != sqrom[j]) {
		msg_printf(ERR, " GE7 SqRom Test: Expected = %d, actual = %d\n", 
			   				sqrom[j], val);
		err++;
	    }
	}

	if (err > 0) msg_printf(ERR, "GE7 SqRom Test: Failed for GE7 #%d\n", i);
    }

    if (err == 0)
        msg_printf(VRB, "sqrom test PASSED.....\n");
    return(err);
}
    
/***************************************************************************
*   Routine: GeBus
*
*   Purpose: Tests the data path between neighbour GE7s.
*
*   History: 03/08/91 : Original Version. Vimal Parikh
*
*   Notes: Each GE7 writes its own id to its neighbour. The all GE7's dump
*	   that id to shared ram location beginning from GE_STATUS. 
*
****************************************************************************/

int
gr2_GeBusTest(void)
{
    register	i, j;
    register unsigned int val;
    int adr;
    int err;

    /* Find out how many GEs installed */
    numge = Gr2ProbeGEs(base);

    /* clear the shared RAM results area */
    ClearStatus(40);

    /* send command */
    msg_printf(DBG, "GE7 GeBus Test : Sending command to GE7....\n");
    for (i=0; i<10; i++)
	base->fifo[DIAG_GEBUS] = gebus_data[i];

    /* wait for it to finish */
    if (diag_finish(1000) < 0) return(-1);

    /* now check the shared RAM for ids of all GE's */
    msg_printf(DBG, "GE7 GeBus Test : Checking results....\n");
    adr = GE_STATUS + MAXCONS;
    err = 0;

    for (j=0; j<numge; j++) {
	msg_printf(DBG, "GE7 GeBus: Checking GE7 #%d\n", j);
	for(i=0; i<10; i++) {
	    val = base->shram[adr+i];
	    if (val != gebus_data[i]) break;
	}
	if (i != 10) {
	    msg_printf(ERR, "GE7 GeBus test failed for GE7 #%d\n", j);
	    msg_printf(ERR, "Expected value = 0x%x, actual = 0x%x\n",
			     gebus_data[i], val);
	    err++;
	}	
	adr += 10;
    }
    if (err == 0)
	msg_printf(VRB, "gebus test PASSED.....\n");
    return(err);
}
    
/***************************************************************************
*   Routine: GeFifoTest
*
*   Purpose: Tests the data path between GE7s and RE3.
*
*   History: 03/08/91 : Original Version. Vimal Parikh
*
*   Notes: The test sends commands to each GE7 to draw a single 10 pixel 
*	   wide line. Each GE7 gets a different color and location. The
*	   host then reads back the pixel values from framebuffer to 
*	   verify the operation.
*
****************************************************************************/

int
gr2_GeFifoTest(void)
{
    register i, j, val, actual;
    int err, n, pixmask;
    int pixbuf[20];

    /* Find out how many GEs installed */
    numge = Gr2ProbeGEs(base);

    /* send command */
    msg_printf(DBG, "Ge7Fifo Test : Sending command to GE7....\n");
    gr2_boardvers();
    if (GBL_VMAVers != 3) pixmask = 0xff;
    if (GBL_VMBVers != 3) pixmask |= 0xff00;
    if (GBL_VMCVers != 3) pixmask |= 0xff0000;

    for(i=0; i<numge; i++) {
	base->fifo[DIAG_GEFIFO] = vert[i][0];
	base->fifo[DIAG_GEFIFO] = vert[i][1];
	base->fifo[DIAG_GEFIFO] = col[i][0];
	base->fifo[DIAG_GEFIFO] = col[i][1];
	base->fifo[DIAG_GEFIFO] = col[i][2];
	}

    /* wait for it to finish */
    if (diag_finish(1000) < 0) return(-1);
    /* now check the frame buffer for results */
    msg_printf(DBG, "Ge7Fifo Test : Checking results....\n");
    err = 0;

    base->fifo[DIAG_READSOURCE] = 0;  /* make sure reading from bit-plane */
    for (i=0; i <20; i++) pixbuf[i] = 0; 

    for(i=0; i<numge; i++) {
	n = pix_dma(base, VDMA_GTOH, vert[i][0], vert[i][1], vert[i][0]+10, 
		    vert[i][1],pixbuf);
	if (n < 0) {
	  msg_printf(ERR,"Ge7fifo - DMA read back data failed\n");
	  return(-1);
	  }
	val = PIXPACK(col[i][0], col[i][1], col[i][2]) & pixmask;

	for(j=0; j<10; j++) {
	    actual = (*(int *) (K0_TO_K1(&pixbuf[j]))) & pixmask;
	    if (val != actual) {
		msg_printf(ERR, "Ge7Fifo: Expected = 0x%x, actual = 0x%x\n", 
						val, actual);
		err++;
	    }
	}

	if (err > 0) msg_printf(ERR, "Ge7Fifo Test Failed for GE7 #%d\n", i);
    }

    return(err);
}
    
/***************************************************************************
*   Routine: GeSeqTest
*
*   Purpose: Tests GE7 sequencer commands.
*
*   History: 04/01/91:  Original Version. Vimal Parikh
*
*   Notes: The results of the test are in RAM0 locations 0-10, 127 and 
* 	   also in sharam(GE_STATUS). A -1 indicates failure of the test.
*
****************************************************************************/

int
gr2_GeSeqTest(void)
{
    register int i, j;
    int	adr;
    int	err;

    /* Find out how many GEs installed */
    numge = Gr2ProbeGEs(base);

    /* clear the shared RAM results area */
    ClearStatus(10);

    /* send command */
    msg_printf(DBG, "Ge7Seq Test : Sending command to GE7....\n");
    base->fifo[DIAG_GESEQ] = 0;

    /* wait for it to finish */
    if (diag_finish(1000) < 0) return(-1);

    /* now check the RAM0 and shared RAM for ids of all GE's */
    msg_printf(DBG, "Ge7Seq Test : Checking results....\n");
    adr = GE_STATUS + MAXCONS;
    err = 0;

    for(i=0; i<numge; i++) {
	/* first check all values in RAM0 */
	for(j=0; j<9; j++) {
	    if (base->ge[i].ram0[j] == -1) {
		msg_printf(ERR, "Ge7Seq failed for GE7 #%d, Ram0_addr = %d\n", 
				 i, j);
		err++;
	    }
	}

	/* check the shared RAM value */
	if (base->shram[adr+i] != 1) {
	    msg_printf(ERR, "Ge7Seq failed for GE7 #%d, shram_addr = %d\n",
		       i, adr+i);
	    err++;
	}
    }

    return(err);
}
    

/***************************************************************************
*   Routine: GeFpuCondTest
*
*   Purpose: Tests GE7 Fpu condition codes.
*
*   History: 07/26/91:  Original Version. Vimal Parikh
*
*   Notes: The results of the test are in RAM0 locations beginning from
* 	   GE_STATUS. The status codes are compared against expected values
*	   and if they do not match, then an error is issued.
*
****************************************************************************/

char	opstr[][10] = {
     "", "", "", "FADD", "FPASSA", "FNEGA", "FPASSB", "FNEGB", "FSUB", "FRSUB",
     "FMIN", "FMAX", "FLOATA", "FLOATB", "FIXA", "FIXB", "IMPY", "FMPY", 
	 "MPASSA", "MPASSB",
     "FABSA", "", "FABSB",  "", "", "", "", "", "", "", 
     "FMINABS", "FMAXABS", "NOTA", "AND", "OR", "XOR", "ANDNOTB", "ANDNOTA", 
	 "ORNOTB",  "ORNOTA",  
     "IADD", "ISUB", "IRSUB", "", "SLRREG", "SARREG", "SLLREG", "", "IMIN", 
	 "IMINABS", 
     "IMAX", "IMAXABS", "IPASSA", "INEGA", "IABSA", "IPASSB", "INEGB", "IABSB",
	 "", "",
     "SLRBR", "SARBR", "SLLBR",
};

char	condstr[][6] = {"", "zero", "neg", "", "pass"};

int
gr2_FpuCondTest(void)
{
    register i, j, cnt, val;
    int adr;
    int err;

    /* Find out how many GEs installed */
    numge = Gr2ProbeGEs(base);

    err = 0;

    for(i=0; i<numge; i++) {
	/* clear the shared RAM results area */
	ClearStatus(64);

	/* send command */
	msg_printf(DBG, "GE7 Fpucond Test : Sending command to GE7....\n");
	base->fifo[DIAG_FPUCOND] = 0;

	/* wait for it to finish */
	if (diag_finish(1000) < 0) return(-1);

	/* now check the RAM0 and shared RAM for ids of all GE's */
	msg_printf(DBG, "Ge7 FPU condition Test : Checking results....\n");
	adr = GE_STATUS + MAXCONS;

	cnt = base->shram[adr++];

	/* if cnt > 0 then print out all errors */
	for(j=0; j<cnt; j++) {
	    val = base->shram[adr++];			
	    msg_printf(ERR, "GE7[%d], Fpu Cond: %s did not set %s flag.\n", 
			    i, opstr[val & 0xff], condstr[val >> 8]);
	    err++;
	}
    }

    return(err);
}
    
/***************************************************************************
*   Routine: gr2_SpfCondTest
*
*   Purpose: Tests GE7 SPF modes.
*
*   History: 07/26/91:  Original Version. Vimal Parikh
*
*   Notes: The results of the test are in RAM0 locations beginning from
* 	   GE_STATUS. The status codes are compared against expected values
*	   and if they do not match, then an error is issued.
*
****************************************************************************/

int	qsam_expected[] = {1,2,3,4,2,1,4,3};
int	tsam_expected[] = {1,5,6, 1,6,5, 5,1,6, 5,6,1, 6,1,5, 6,5,1,
			   3,5,6, 3,6,5, 5,3,6, 5,6,3, 6,3,5, 6,5,3};
int	vpsam1_expected[] = {1,1,2,2};
int	vpsam2_expected[] = {2,1};

int
gr2_SpfCondTest(void)
{
    register	i, j, val;
    long	adr;
    int		err = 0;

    /* Find out how many GEs installed */
    numge = Gr2ProbeGEs(base);

    for(i=0; i<numge; i++) {
	/* clear the shared RAM results area */
	ClearStatus(100);

	/* send command */
	msg_printf(DBG, "GE7 SpfCond Test : Sending command to GE7....\n");
	base->fifo[DIAG_SPFCOND] = 0;

	/* wait for it to finish */
	if (diag_finish(1000) < 0) return(-1);

	/* now check the RAM0 and shared RAM for ids of all GE's */
	msg_printf(DBG, "GE7 SPF cond. Test : Checking results....\n");
	adr = GE_STATUS + MAXCONS;

	/* check the qsam test results */
	for(j=0; j < NumberOf(qsam_expected); j++) {
	    val = base->shram[adr++];
	    if (val != qsam_expected[j]) {
		msg_printf(ERR, "GE7[%d]: SPF cond. QSAM failed on %d value",
			   i, j);
		err++;
	    }
	}

	/* check the tsam test results */
	for(j=0; j < NumberOf(tsam_expected); j++) {
	    val = base->shram[adr++];
	    if (val != tsam_expected[j]) {
		msg_printf(ERR, "GE7[%d]: SPF cond. TSAM failed on %d value",
			   i, j);
		err++;
	    }
	}

    }
    
    /* check the vpsam test results */
    adr = GE_STATUS + MAXCONS + 50;
    for(i=0; i<numge; i++) {
	for(j=0; j < NumberOf(vpsam1_expected); j++) {
	    val = base->shram[adr++];
	    if (val != vpsam1_expected[j]) {
		msg_printf(ERR, "GE7[%d]: SPF cond. VPSAM1 failed on %d value",
			   i, j);
		err++;
	    }
	}
    }

    adr = GE_STATUS + MAXCONS + 70;
    for(i=0; i<numge; i++) {
	for(j=0; j < NumberOf(vpsam2_expected); j++) {
	    val = base->shram[adr++];
	    if (val != vpsam2_expected[j]) {
		msg_printf(ERR, "GE7[%d]: SPF cond. VPSAM2 failed on %d value",
			   i, j);
		err++;
	    }
	}
    }

    return(err);
}
    

/***************************************************************************
*   Routine: ClearStatus
*
*   Purpose: Clears the result area of shared RAM.
*
*   History: 03/08/91 : Original Version. Vimal Parikh
*
*   Notes: This utility is used for clearing shared RAM locations to 0.
*
****************************************************************************/

void
ClearStatus(int cnt)
{
    register int i, adr;

    adr = GE_STATUS + MAXCONS;
    for(i=0; i<cnt; i++) 
	base->shram[adr++] = 0;
}


/***************************************************************************
*   Routine: diag_finish
*
*   Purpose: waits for GE7 to finish the command.
*
*   History: 03/14/91 : Original Version.
*
*   Notes: 
*
****************************************************************************/

static int
diag_finish(int delay)
{
    base->hq.fin2 = 0;
    base->fifo[DIAG_FINISH2] = 0;

    DELAY(delay);
    if ((base->hq.version & 0x2 )== 0) {
	msg_printf(DBG, "GE7 not responding \n");
	return(-1);
    }
    base->hq.fin2 = 0;
    return(0);
}

/***************************************************************************
*   Routine: ge_FloatOpsTest
*
*   Purpose: Tests integer/arithmetic operations on GE7.
*
*   History: 08/15/91 : Original Version. Ragu
*
*   Notes: Do the arithmetic operations sequentially on all GE's and compare the results.
*
****************************************************************************/

#define		IADD	1
#define		ISUB	2
#define		IMPY	3
#define		FADD	4
#define		FSUB	5
#define		FMPY	6

/* Following floats in internal IEEE format  are stored in Floats[]

 0.512348,
 983.0098234,
 1586.9889434,
 11090.3453411,
 245801090.340394,
 -0.512348,
 -78.120098,
 -983.0098234,
 -1586.9889434,
 -801090.009823,
*/

static char * Float_strings[] = {
		"0.512348",
		"983.0098234",
		"1586.9889434",
  		"11090.3453411",
   		"245801090.340394",
    		"-0.512348",
     		"-78.120098",
      		"-983.0098234",
       		"-1586.9889434",
		"-801090.009823",
		};

static int   Floats [] =  {
	0x3f03293d,0x4475c0a1,0x44c65fa5,0x462d4962,0x4d6a6a08,
	0xbf03293d,0xc29c3d7d,0xc475c0a1,0xc4c65fa5,0xc9439420
	};

static int Ints [] = { 8,78,983,1586,11090,801090,245801090 };

#define	NOFLOATOPS	NumberOf(Floats)
#define	NOINTOPS	NumberOf(Ints)

static int FADDResults [][NOFLOATOPS] = {
	
{0x3f83293d,0x4475e16b,0x44c6700a,0x462d4b6f,0x4d6a6a08,
0,0xc29b372b,0xc4759fd7,0xc4c64f40,0xc9439418,},
{0x4475e16b,0x44f5c0a1,0x45209ffb,0x463ca56c,0x4d6a6a45,
0x44759fd7,0x446238f1,0,0xc416fea9,0xc94356b0,},
{0x44c6700a,0x45209ffb,0x45465fa5,0x46461557,0x4d6a6a6b,
0x44c64f40,0x44bc9bcd,0x4416fea9,0,0xc94330f0,},
{0x462d4b6f,0x463ca56c,0x46461557,0x46ad4962,0x4d6a6cbd,
0x462d4755,0x462c10e7,0x461ded58,0x46147d6d,0xc940defa,},
{0x4d6a6a08,0x4d6a6a45,0x4d6a6a6b,0x4d6a6cbd,0x4dea6a08,
0x4d6a6a08,0x4d6a6a03,0x4d6a69cb,0x4d6a69a5,0x4d69a674,},
{0,0x44759fd7,0x44c64f40,0x462d4755,0x4d6a6a08,
0xbf83293d,0xc29d43cf,0xc475e16b,0xc4c6700a,0xc9439428,},
{0xc29b372b,0x446238f1,0x44bc9bcd,0x462c10e7,0x4d6a6a03,
0xc29d43cf,0xc31c3d7d,0xc484a428,0xc4d0237d,0xc9439902,},
{0xc4759fd7,0,0x4416fea9,0x461ded58,0x4d6a69cb,0xc475e16b,
0xc484a428,0xc4f5c0a1,0xc5209ffb,0xc943d190,},
{0xc4c64f40,0xc416fea9,0,0x46147d6d,0x4d6a69a5,0xc4c6700a,
0xc4d0237d,0xc5209ffb,0xc5465fa5,0xc943f750,},
{0xc9439418,0xc94356b0,0xc94330f0,0xc940defa,0x4d69a674,
0xc9439428,0xc9439902,0xc943d190,0xc943f750,0xc9c39420,},
};

static int FSUBResults [][NOFLOATOPS] = {

{0,0xc4759fd7,0xc4c64f40,0xc62d4755,0xcd6a6a08,
0x3f83293d,0x429d43cf,0x4475e16b,0x44c6700a,0x49439428,},
{0x44759fd7,0,0xc416fea9,0xc61ded58,0xcd6a69cb,
0x4475e16b,0x4484a428,0x44f5c0a1,0x45209ffb,0x4943d190,},
{0x44c64f40,0x4416fea9,0,0xc6147d6d,0xcd6a69a5,
0x44c6700a,0x44d0237d,0x45209ffb,0x45465fa5,0x4943f750,},
{0x462d4755,0x461ded58,0x46147d6d,0,0xcd6a6753,
0x462d4b6f,0x462e81dd,0x463ca56c,0x46461557,0x49464946,},
{0x4d6a6a08,0x4d6a69cb,0x4d6a69a5,0x4d6a6753,0,
0x4d6a6a08,0x4d6a6a0d,0x4d6a6a45,0x4d6a6a6b,0x4d6b2d9c,},
{0xbf83293d,0xc475e16b,0xc4c6700a,0xc62d4b6f,0xcd6a6a08,
0,0x429b372b,0x44759fd7,0x44c64f40,0x49439418,},
{0xc29d43cf,0xc484a428,0xc4d0237d,0xc62e81dd,0xcd6a6a0d,
0xc29b372b,0,0x446238f1,0x44bc9bcd,0x49438f3e,},
{0xc475e16b,0xc4f5c0a1,0xc5209ffb,0xc63ca56c,0xcd6a6a45,
0xc4759fd7,0xc46238f1,0,0x4416fea9,0x494356b0,},
{0xc4c6700a,0xc5209ffb,0xc5465fa5,0xc6461557,0xcd6a6a6b,
0xc4c64f40,0xc4bc9bcd,0xc416fea9,0,0x494330f0,},
{0xc9439428,0xc943d190,0xc943f750,0xc9464946,0xcd6b2d9c,
0xc9439418,0xc9438f3e,0xc94356b0,0xc94330f0,0,},
};

static int FMPYResults [][NOFLOATOPS] = {

{0x3e866676,0x43fbd252,0x444b45cc,0x45b190ee,0x4cf0340a,
0xbe866676,0xc2201944,0xc3fbd252,0xc44b45cc,0xc8c8689b,},
{0x43fbd252,0x496bea45,0x49be6ecd,0x4b26599f,0x526107dd,
0xc3fbd252,0xc795fc69,0xc96bea45,0xc9be6ecd,0xce3bbfed,},
{0x444b45cc,0x49be6ecd,0x4a19b817,0x4b864780,0x52b5a597,
0xc44b45cc,0xc7f223dc,0xc9be6ecd,0xca19b817,0xce978da3,},
{0x45b190ee,0x4b26599f,0x4b864780,0x4cea9887,0x541eacd9,
0xc5b190ee,0xc95384ae,0xcb26599f,0xcb864780,0xd004632a,},
{0x4cf0340a,0x526107dd,0x52b5a597,0x541eacd9,0x5b56a603,
0xccf0340a,0xd08f10eb,0xd26107dd,0xd2b5a597,0xd7331667,},
{0xbe866676,0xc3fbd252,0xc44b45cc,0xc5b190ee,0xccf0340a,
0x3e866676,0x42201944,0x43fbd252,0x444b45cc,0x48c8689b,},
{0xc2201944,0xc795fc69,0xc7f223dc,0xc95384ae,0xd08f10eb,
0x42201944,0x45beb5fe,0x4795fc69,0x47f223dc,0x4c6eba7b,},
{0xc3fbd252,0xc96bea45,0xc9be6ecd,0xcb26599f,0xd26107dd,
0x43fbd252,0x4795fc69,0x496bea45,0x49be6ecd,0x4e3bbfed,},
{0xc44b45cc,0xc9be6ecd,0xca19b817,0xcb864780,0xd2b5a597,
0x444b45cc,0x47f223dc,0x49be6ecd,0x4a19b817,0x4e978da3,},
{0xc8c8689b,0xce3bbfed,0xce978da3,0xd004632a,0xd7331667,
0x48c8689b,0x4c6eba7b,0x4e3bbfed,0x4e978da3,0x53156afe,},
};

static int abs(int x)
{
	return ( x > 0 ? x: -x);
}

/* The tests are carried out for all GE7s present. But the error report 
   is printed only for given GE7
*/
int
gr2_FloatOpsTest(int argc, char *argv[])
{


    int i, j, n, val, adr, op1,op2;
    register	err;
    char * op1_str, * op2_str ;
    int ge_num , ge_num_given = 0 ;

    /* Find out how many GEs installed */
    numge = Gr2ProbeGEs(base);


    if ( argc > 1 ) {
      ge_num = atoi(argv[1]) ;
      if ( ge_num >= 0 &&  ge_num < numge ) 
	ge_num_given = 1 ;
      
    }

    err = 0;



    for( i= 0 ; i < 2*NOINTOPS; i++ )
    {
     for( j= 0 ; j < 2*NOINTOPS ; j++ )
     {

	op1 = i < NOINTOPS ? Ints[i] : -Ints[i-NOINTOPS] ;
	op2 = j < NOINTOPS ? Ints[j] : -Ints[j-NOINTOPS] ;

    	ClearStatus(4*4);
    	adr = GE_STATUS + MAXCONS;

    	msg_printf(DBG, "Floating point Test IADD\n");
    	base->fifo[DIAG_FLOATOPS] = op1 ;
    	base->fifo[DIAG_FLOATOPS] = op2 ;
    	base->fifo[DIAG_FLOATOPS] = IADD ;

    	if (diag_finish(100) < 0) return(-1);

    	for(n=0; n< numge ;n++) {
	 if ((val=base->shram[adr++]) != (op1 +op2)) {
	 err ++;
	 if ( !ge_num_given || (ge_num_given && ( n == ge_num )))
	   msg_printf(ERR, "Float pt.Test IADD(op1 = %d,op2 = %d) GE %d:Expected = %#x,actual = %#x \n", 
			op1,op2,n,(op1+op2),val);
	 }
	}

    	ClearStatus(4*4);
    	adr = GE_STATUS + MAXCONS;

    	msg_printf(DBG, "Floating point Test ISUB\n");
    	base->fifo[DIAG_FLOATOPS] = op1 ;
    	base->fifo[DIAG_FLOATOPS] = op2 ;
    	base->fifo[DIAG_FLOATOPS] = ISUB ;

    	if (diag_finish(100) < 0) return(-1);

    	for(n=0; n< numge ;n++) {
	 if ((val=base->shram[adr++]) != (op1 -op2)) {
	 err ++;
	 if ( !ge_num_given || (ge_num_given && ( n == ge_num )))
	   msg_printf(ERR, "Float pt.Test ISUB(op1 = %d,op2 = %d) GE %d:Expected = %#x,actual = %#x \n", 
			op1,op2,n,(op1-op2),val);
	 }
	}

    	ClearStatus(4*4);
    	adr = GE_STATUS + MAXCONS;

    	msg_printf(DBG, "Floating point Test IMPY\n");
    	base->fifo[DIAG_FLOATOPS] = op1 ;
    	base->fifo[DIAG_FLOATOPS] = op2 ;
    	base->fifo[DIAG_FLOATOPS] = IMPY ;

    	if (diag_finish(100) < 0) return(-1);

    	for(n=0; n< numge ;n++) {
	 if ((val=(base->shram[adr++]&0xffffff)) != ((op1*op2)&0xffffff)){
	 err ++ ;
	 if ( !ge_num_given || (ge_num_given && ( n == ge_num )))
	   msg_printf(ERR, "Float pt.Test IMPY(op1 = %#x,op2 = %#x) GE %d:Expected = %#x,actual = %#x \n", 
			op1,op2,n,((op1*op2)&0xffffff),val);
	 }
	}

     } /* end of operand 2 loop */
    } /* end of operand 1 loop */
	

    for( i= 0 ; i < NOFLOATOPS; i++ )
    {
     for( j= 0 ; j < NOFLOATOPS ; j++ )
     {

	op1 = Floats[i] ;
	op2 = Floats[j] ;
	op1_str = Float_strings[i];
	op2_str = Float_strings[j];

    	ClearStatus(4*4);
    	adr = GE_STATUS + MAXCONS;

    	msg_printf(DBG, "Floating point Test FADD\n");
    	base->fifo[DIAG_FLOATOPS] = op1 ;
    	base->fifo[DIAG_FLOATOPS] = op2 ;
    	base->fifo[DIAG_FLOATOPS] = FADD ;

    	if (diag_finish(100) < 0) return(-1);

    	for(n=0; n< numge ;n++) {
	 if (abs((val=base->shram[adr++]) - FADDResults[i][j]) > 1) {
	 err ++;
	 if ( !ge_num_given || (ge_num_given && ( n == ge_num )))
	   msg_printf(ERR, "Float pt.Test FADD(op1 = %s,op2 = %s) GE %d:Expected = %#x,actual = %#x \n", 
			op1_str,op2_str,n,FADDResults[i][j],val);
	 }
	}

    	ClearStatus(4*4);
    	adr = GE_STATUS + MAXCONS;

    	msg_printf(DBG, "Floating point Test FSUB\n");
    	base->fifo[DIAG_FLOATOPS] = op1 ;
    	base->fifo[DIAG_FLOATOPS] = op2 ;
    	base->fifo[DIAG_FLOATOPS] = FSUB ;

    	if (diag_finish(100) < 0) return(-1);

    	for(n=0; n< numge ;n++) {
	 if (abs((val=base->shram[adr++]) - FSUBResults[i][j]) > 1) {
	 err ++;
	 if ( !ge_num_given || (ge_num_given && ( n == ge_num )))
	   msg_printf(ERR, "Float pt.Test FSUB(op1 = %s,op2 = %s) GE %d:Expected = %#x,actual = %#x \n", 
			op1_str,op2_str,n,FSUBResults[i][j],val);
	 }
	}

    	ClearStatus(4*4);
    	adr = GE_STATUS + MAXCONS;

    	msg_printf(DBG, "Floating point Test FMPY\n");
    	base->fifo[DIAG_FLOATOPS] = op1 ;
    	base->fifo[DIAG_FLOATOPS] = op2 ;
    	base->fifo[DIAG_FLOATOPS] = FMPY ;

    	if (diag_finish(100) < 0) return(-1);

    	for(n=0; n< numge ;n++) {
	 if (abs((val=base->shram[adr++]) - FMPYResults[i][j]) > 1 ) {
	 err ++;
	 if ( !ge_num_given || (ge_num_given && ( n == ge_num )))
	   /* If floating format is needed for op1 and op2 use op1_str & op2_str */
	   msg_printf(ERR, "Float pt.Test FMPY(op1 = %#x,op2 = %#x) GE %d:Expected = %#x,actual = %#x \n", 
			op1,op2,n,FMPYResults[i][j],val);
	 
	 }
	}

     } /* end of operand 2 loop */
    } /* end of operand 1 loop */

    if (err == 0)
        msg_printf(VRB, "gefloat test PASSED.....\n");
    return err ;	
}
