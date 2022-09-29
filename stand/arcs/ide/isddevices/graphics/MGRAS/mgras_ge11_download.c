
#include "uif.h"
#include "libsk.h"
#include "libsc.h"
#include "sys/mgrashw.h"
#include "cp_ge_sim_tokens.h"
#include <math.h>
#include <arcs/io.h>
#include <arcs/errno.h>

#include "mgras_diag.h"
#include "parser.h"
#include "mgras_hq3.h"
#include "ide_msg.h"

static cmd_id = 0;

static char *on[] = { NULL, "1" };
static char *off[] = { NULL, "0" };

#if (MFG_USED && defined(IP30))
__uint32_t MgrasDownloadGE11Ucode(void);
__uint32_t _MgrasDownloadGE11UcodeFromFile(register mgras_hw *mgbase);
__uint32_t _MgrasReadDiagUcode(register mgras_hw *mgbase, int len);

char *bp, *pserver, *ge_ucode_from_disk;
char *ucodeBuf;
char str[300];
__uint32_t MgrasDownloadGE11Ucode(void)
{
    __uint32_t errors = 0;

    errors += _MgrasDownloadGE11UcodeFromFile(mgbase);
    return(errors);
}

__uint32_t _MgrasDownloadGE11UcodeFromFile(register mgras_hw *mgbase)
{
    __uint32_t led, cmd, __temp;
    __uint32_t i, j;
    __uint32_t errors;
    __uint32_t addr;
    __uint64_t rc, fd, cc, first_flag = 0;
    __uint32_t word0_hex, word1_hex, word2_hex;
    __uint32_t first_word0_hex, first_word1_hex, first_word2_hex;
    __uint32_t last_word0_hex, last_word1_hex, last_word2_hex;
    char buf[20], word0[10], word1[10], word2[10];

    /* get a large buffer and initialize with 0xff's */
    ucodeBuf = (char *)get_chunk(300000);
    for (i = 0; i < 300000; i+=10) {
	strncpy(&(ucodeBuf[i]), "ffffffffff", 10);
    }

    errors = 0;	
    str[0] = '\0' ;
    ge_ucode_from_disk = getenv("ge_ucode_from_disk");
    if ((ge_ucode_from_disk != NULL) && (strcmp(ge_ucode_from_disk, "1") == 0)) {
	/* load ucode from the disk */
	strcat(str, "dksc(0,1,0)/stand/");
    } else {
	/* load ucode from a server */
    	pserver = getenv("mfgserver");
    	strcat(str, "bootp()");
    	if (pserver) {
    		strcat(str, pserver);
		strcat(str, ":");
    	}
    }
    strcat(str, "mgras_ge.ucode");
    msg_printf(VRB, "The file is %s\n", str);

#ifdef DEBUG
	msg_printf(DBG, "Before polling status reg\n");
#endif

	HQ3_POLL_WORD(0x0, 0x1, STATUS_ADDR);

#ifdef DEBUG
	msg_printf(DBG, "After polling status reg\n");
#endif

	GE11_DIAG_ON();

    	/* Set DIAG_MODE register in GE */
    	GE11_DIAG_WR_PTR(GE11_UCODE_BASE);

	if ((rc = Open ((CHAR *)str, OpenReadOnly, &fd)) != ESUCCESS) {
		msg_printf(ERR, "Unable to open the GE UCODE FILE\n");
		errors++;
		REPORT_PASS_OR_FAIL((&GE11_err[GE11_UCODE_SERVER]), errors);
	}
	bp = ucodeBuf;
	j = 0;
	msg_printf(VRB, "The GE ucode file is opened\n");

	while (((rc = Read(fd, bp, 10000, &cc))==ESUCCESS) && cc) {
		j++;
		bp += 10000;
		msg_printf(DBG, "Number of chunks %d; rc %d; cc %d\n", 
			j, rc, cc);
	}

	msg_printf(VRB, "The GE ucode file is read into memory\n");

	bp = ucodeBuf;
	j = 0;
	while (1) {
	    word0[0] = word1[0] = word2[0] = '\0';
	    strncpy(word2, bp, 2);
	    bp += 2;
	    strncpy(word1, bp, 8);
	    bp += 8;
	    strncpy(word0, bp, 8);
	    bp += 9;
	    word0[8] = word1[8] = word2[2] = '\0';
	    if ( (!strncasecmp(word0, "ffffffff", 8)) && 
		 (!strncasecmp(word1, "ffffffff", 8)) &&
	         (!strncasecmp(word2, "ff", 2)) ) {
		msg_printf(VRB, "End of ucode reached\n");
		 break;
	    }
	    atohex(word0, (int *)&word0_hex);
	    atohex(word1, (int *)&word1_hex);
	    atohex(word2, (int *)&word2_hex);

#if 0
	    if (((word0_hex & 0xffffffff) == 0xffffffff) && 
		((word1_hex & 0xffffffff) == 0xffffffff) && 
		((word2_hex & 0xff) == 0xff)) {
		msg_printf(VRB, "End of ucode reached\n");
		break;
	    }
#endif
	    if (!first_flag) {
		first_word0_hex = word0_hex;
		first_word1_hex = word1_hex;
		first_word2_hex = word2_hex;
		first_flag = 1;
	    } else if (word0_hex != 0) {
		last_word0_hex = word0_hex;
		last_word1_hex = word1_hex;
		last_word2_hex = word2_hex;
	    }

    	    GE11_DIAG_WR_IND(word0_hex, GE11_UCODE_MASK);
	    DELAY(10);
    	    GE11_DIAG_WR_IND(word1_hex, GE11_UCODE_MASK);
	    DELAY(10);
    	    GE11_DIAG_WR_IND(word2_hex, GE11_UCODE_MASK);
	    DELAY(10);
	    if (j%2048 == 0) 
	    {	
		led ? setled(2,off) : setled(2,on);
		led = ~led & 0x1;
		msg_printf(VRB,".");
	    msg_printf(DBG, "word0_hex 0x%x; word1_hex 0x%x; word2_hex 0x%x\n",
		word0_hex, word1_hex, word2_hex);
	    }
	    j++;
    	}

	Close(fd);

	msg_printf(VRB, "The last UCODE DATA: 0x%x; 0x%x; 0x%x, Size = %d\n",
		last_word0_hex, last_word1_hex, last_word2_hex,j);

	msg_printf(VRB, "\nUcode write complete\n");
	DELAY(5000);

	/* read back and compare */
	errors = _MgrasReadDiagUcode(mgbase,j);
#if 0
	if (errors) {
		msg_printf(VRB, "Ucode Down load error\n");
		REPORT_PASS_OR_FAIL((&GE11_err[GE11_UCODE_DNLOAD]), errors);
	}
#endif		
	msg_printf(VRB, "\nUcode readback complete\n");

	/* re-load the microcode at addr = 0 to reset the pc */
	addr = 0;
	HQ3_PIO_WR(ge11_diag_a_addr, (GE11_UCODE_BASE | (addr << 2)), ~0x0);

    	HQ3_PIO_WR(ge11_diag_d_addr, first_word0_hex, ~0x0);
    	HQ3_PIO_WR(ge11_diag_d_addr, first_word1_hex, ~0x0);
    	HQ3_PIO_WR(ge11_diag_d_addr, first_word2_hex, ~0x0);

	GE11_DIAG_OFF();

	msg_printf(VRB,"\nUcode downloaded\n");

#if 0
cmd = 0x000a0704;
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, 0x1, ~0x0);
HQ3_POLL_WORD(0x1, 0x1, STATUS_ADDR);
HQ3_PIO_RD(STATUS_ADDR, ~0x0, __temp);
#endif

HQ3_PIO_WR(HQ3_CONFIG_ADDR, 0x58a9, HQ3_CONFIG_MASK);

HQ3_PIO_WR(GIO_CONFIG_ADDR, 0x765b104f, GIO_CONFIG_MASK);

HQ3_PIO_RD(HQ3_CONFIG_ADDR, HQ3_CONFIG_MASK, __temp);

HQ3_PIO_RD(GIO_CONFIG_ADDR, GIO_CONFIG_MASK, __temp);

    REPORT_PASS_OR_FAIL((&GE11_err[GE11_UCODE_DNLOAD]), errors);
}

__uint32_t _MgrasReadDiagUcode(register mgras_hw *mgbase, int len)
{
	__uint32_t	errors, i, led;
	__uint32_t	ucode0, ucode1;
	__uint32_t	uword0, uword1, uword2;
        char buf[20], word0[10], word1[10], word2[10];
	
	GE11_DIAG_WR_PTR(GE11_UCODE_BASE);
	errors =0;

	for (i = 0 ; i < len; i++)  
	{
	    if ((i&0x1) == 0) {
		GE11_DIAG_WR_PTR((1 << 31) | 0x2);
		DELAY(5);
                MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT | MGRAS_BC1_FORCE_DCB_STAT_BIT); 
		DELAY(5);
	        MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT);
	   }													  

	   /* msg_printf(1,"R1"); */
	   ucode0 = GE11_Diag_read(GE11_UCODE_MASK);
	  /* msg_printf(1,"R2 "); */
	   ucode1 = GE11_Diag_read(GE11_UCODE_MASK);

	   /* 8 + 8 + 2 + 1 (`\n`) = 19 bytes */
    	   bp = ucodeBuf + 19*i;
	   word0[0] = word1[0] = word2[0] = '\0';
           strncpy(word2, bp, 2);
           bp += 2;
           strncpy(word1, bp, 8);
           bp += 8;
           strncpy(word0, bp, 8);
           bp += 9;
           word0[8] = word1[8] = word2[2] = '\0';
           atohex(word0, (int *)&uword0);
           atohex(word1, (int *)&uword1);
           atohex(word2, (int *)&uword2);
 

	   if ((i % 2048) == 0)
	   {
		led ? setled(2,off) : setled(2,on);
		led = ~led & 0x1;
		msg_printf(VRB, ".");
	   }			

	   /* Compare data */
	   if ((i&0x1) ==  0)   {
	   	COMPARE("GE Ucode download readback pass 1- word #1", i, uword0, ucode0, GE11_UCODE_MASK, errors);
		COMPARE("GE Ucode download readback pass 1- word #3", i, uword2, ucode1, GE11_UCODE2_MASK, errors);
	   }
	   else {
	   	COMPARE("GE Ucode download readback pass 1- word #2", i, uword1, ucode0, GE11_UCODE_MASK, errors);
		COMPARE("GE Ucode download readback pass 1- word #3", i, uword2, ucode1, GE11_UCODE2_MASK, errors);
	   }
	}

	msg_printf(VRB,"\nFirst pass ucode read complete\n");
	DELAY(100);

        GE11_DIAG_WR_PTR((GE11_UCODE_BASE + 4));


        /* Second Pass */
	for (i = 1 ; i < len; i++)
	{
	    if ((i&0x1) == 1) {
		GE11_DIAG_WR_PTR((1 << 31) | 0x2);
		DELAY(5);
                MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT | MGRAS_BC1_FORCE_DCB_STAT_BIT);
		DELAY(5);
	        MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT);
	    }


	   ucode0 = GE11_Diag_read(GE11_UCODE_MASK);
	   ucode1 = GE11_Diag_read(GE11_UCODE_MASK);

	   /* 8 + 8 + 2 + 1 (`\n`) = 19 bytes */
    	   bp = ucodeBuf + 19*i;

	   word0[0] = word1[0] = word2[0] = '\0';
           strncpy(word2, bp, 2);
           bp += 2;
           strncpy(word1, bp, 8);
           bp += 8;
           strncpy(word0, bp, 8);
           bp += 9;
           word0[8] = word1[8] = word2[2] = '\0';
           atohex(word0, (int *)&uword0);
           atohex(word1, (int *)&uword1);
           atohex(word2, (int *)&uword2);

	   if ((i % 2048) == 0)
	   {
		   led ? setled(2,off) : setled(2,on);
		   led = ~led & 0x1;
		   msg_printf(VRB, ".");
	   }

	   if ((i & 0x1) == 0)  { 
	   	COMPARE("HQ3 to GE Ucode download readback pass 2- word #2", i, uword1, ucode0, GE11_UCODE_MASK, errors);
	   }
	   else {
	   	COMPARE("HQ3 to GE Ucode download readback pass 2- word #1", i, uword0, ucode0, GE11_UCODE_MASK, errors);
		COMPARE("HQ3 to GE Ucode download readback pass 2- word #3", i, uword2, ucode1, GE11_UCODE2_MASK, errors);
	   }

	}

	/* check  if any data is left over */
	/* following  read may time-out but its ok. It will flush any left over data that is not read in the loop */
	ucode0 = GE11_Diag_read(GE11_UCODE_MASK);
	ucode1 = GE11_Diag_read(GE11_UCODE_MASK);
	return errors;
}
#else
#include "mgras_ge.diag"

__uint32_t _MgrasReadDiagUcode(register mgras_hw *mgbase, ge_ucode *UcodeTable, int len)
{
	__uint32_t	errors, i, led;
	__uint32_t	ucode0, ucode1;
	
	GE11_DIAG_WR_PTR(GE11_UCODE_BASE);
	errors =0;

	for (i = 0 ; i < len; i++)  
	{
	    if ((i&0x1) == 0) {
		GE11_DIAG_WR_PTR((1 << 31) | 0x2);
		DELAY(5);
                MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT | MGRAS_BC1_FORCE_DCB_STAT_BIT); 
		DELAY(5);
	        MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT);
	   }													  

	   /* msg_printf(1,"R1"); */
	   ucode0 = GE11_Diag_read(GE11_UCODE_MASK);
	  /* msg_printf(1,"R2 "); */
	   ucode1 = GE11_Diag_read(GE11_UCODE_MASK);


	   if ((i % 2048) == 0)
	   {
		led ? setled(2,off) : setled(2,on);
		led = ~led & 0x1;
		msg_printf(VRB, ".");
	   }			

	   /* Compare data */
	   if ((i&0x1) ==  0)   {
	   	COMPARE("GE Ucode download readback pass 1- word #1", i, UcodeTable[i].uword0, ucode0, GE11_UCODE_MASK, errors);
		COMPARE("GE Ucode download readback pass 1- word #3", i, UcodeTable[i].uword2, ucode1, GE11_UCODE2_MASK, errors);
	   }
	   else {
	   	COMPARE("GE Ucode download readback pass 1- word #2", i, UcodeTable[i].uword1, ucode0, GE11_UCODE_MASK, errors);
		COMPARE("GE Ucode download readback pass 1- word #3", i, UcodeTable[i].uword2, ucode1, GE11_UCODE2_MASK, errors);
	   }
	}

	msg_printf(VRB,"\nFirst pass ucode read complete\n");
	DELAY(100);

        GE11_DIAG_WR_PTR((GE11_UCODE_BASE + 4));
	for (i = 1 ; i < len; i++)
	{
	    if ((i&0x1) == 1) {
		GE11_DIAG_WR_PTR((1 << 31) | 0x2);
		DELAY(5);
                MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT | MGRAS_BC1_FORCE_DCB_STAT_BIT);
		DELAY(5);
	        MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT);
	    }


	   ucode0 = GE11_Diag_read(GE11_UCODE_MASK);
	   ucode1 = GE11_Diag_read(GE11_UCODE_MASK);


	   if ((i % 2048) == 0)
	   {
		   led ? setled(2,off) : setled(2,on);
		   led = ~led & 0x1;
		   msg_printf(VRB, ".");
	   }

	   if ((i & 0x1) == 0)  { 
	   	COMPARE("HQ3 to GE Ucode download readback pass 2- word #2", i, UcodeTable[i].uword1, ucode0, GE11_UCODE_MASK, errors);
	   }
	   else {
	   	COMPARE("HQ3 to GE Ucode download readback pass 2- word #1", i, UcodeTable[i].uword0, ucode0, GE11_UCODE_MASK, errors);
		COMPARE("HQ3 to GE Ucode download readback pass 2- word #3", i, UcodeTable[i].uword2, ucode1, GE11_UCODE2_MASK, errors);
	   }

	}

	/* check  if any data is left over */
	/* following  read may time-out but its ok. It will flush any left over data that is not read in the loop */
	ucode0 = GE11_Diag_read(GE11_UCODE_MASK);
	ucode1 = GE11_Diag_read(GE11_UCODE_MASK);
	return errors;
}
__uint32_t _MgrasDownloadGE11Ucode(register mgras_hw *mgbase, ge_ucode *UcodeTable, int len);
__uint32_t MgrasDownloadGE11Ucode(void);

__uint32_t MgrasDownloadGE11Ucode(void)
{
    __uint32_t errors;

    errors = _MgrasDownloadGE11Ucode(mgbase, ge_table,
			(sizeof(ge_table) / sizeof(ge_table[0])));
    return(errors);
}

__uint32_t _MgrasDownloadGE11Ucode(register mgras_hw *mgbase, ge_ucode *UcodeTable, int len)
{
    __uint32_t led, cmd, __temp;
    __uint32_t j;
    __uint32_t errors;
    __uint32_t addr;
    
    errors = 0;	

cmd = 0x000a0704;
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, 0x0, ~0x0);

#ifdef DEBUG
	msg_printf(DBG, "Before polling status reg\n");
#endif

	HQ3_POLL_WORD(0x0, 0x1, STATUS_ADDR);

#ifdef DEBUG
	msg_printf(DBG, "After polling status reg\n");
#endif

	GE11_DIAG_ON();

    	/* Set DIAG_MODE register in GE */
    	GE11_DIAG_WR_PTR(GE11_UCODE_BASE);
	msg_printf(DBG,"ucode len = %d\n", len);

	/* the addresses in the UcodeTable are contiguous */
    	for (j = 0; j < len;j++) {
    	    GE11_DIAG_WR_IND(UcodeTable[j].uword0, GE11_UCODE_MASK);
	    DELAY(10);
    	    GE11_DIAG_WR_IND(UcodeTable[j].uword1, GE11_UCODE_MASK);
	    DELAY(10);
    	    GE11_DIAG_WR_IND(UcodeTable[j].uword2, GE11_UCODE_MASK);
	    DELAY(10);
	    if (j%2048 == 0) 
	    {	
		led ? setled(2,off) : setled(2,on);
		led = ~led & 0x1;
		msg_printf(VRB,".");
	    }
    	}

	msg_printf(VRB, "\nUcode write complete\n");
	DELAY(5000);

	/* read back and compare */
	errors = _MgrasReadDiagUcode(mgbase, UcodeTable, len);

	if (errors) {
		msg_printf(VRB, "Ucode Down load error\n");
		return errors;
	}
		
	msg_printf(VRB, "\nUcode readback complete\n");

	/* re-load the microcode at addr = 0 to reset the pc */
	addr = 0;
	HQ3_PIO_WR(ge11_diag_a_addr, (GE11_UCODE_BASE | (addr << 2)), ~0x0);

    	HQ3_PIO_WR(ge11_diag_d_addr, UcodeTable[0].uword0, ~0x0);
    	HQ3_PIO_WR(ge11_diag_d_addr, UcodeTable[0].uword1, ~0x0);
    	HQ3_PIO_WR(ge11_diag_d_addr, UcodeTable[0].uword2, ~0x0);

	GE11_DIAG_OFF();

	msg_printf(VRB,"\nUcode downloaded\n");

#if 0
cmd = 0x000a0704;
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, 0x1, ~0x0);
HQ3_POLL_WORD(0x1, 0x1, STATUS_ADDR);
HQ3_PIO_RD(STATUS_ADDR, ~0x0, __temp);
#endif

HQ3_PIO_WR(HQ3_CONFIG_ADDR, 0x58a9, HQ3_CONFIG_MASK);

HQ3_PIO_WR(GIO_CONFIG_ADDR, 0x765b104f, GIO_CONFIG_MASK);

HQ3_PIO_RD(HQ3_CONFIG_ADDR, HQ3_CONFIG_MASK, __temp);

HQ3_PIO_RD(GIO_CONFIG_ADDR, GIO_CONFIG_MASK, __temp);

    return errors;	
}
#endif

void
mgras_ucode_exec(void)
{
	GE11_DIAG_ON();
	HQ3_PIO_WR(ge11_diag_a_addr, OUTPUT_FIFO_CNTL, 0x7fffffff);
	HQ3_PIO_WR(ge11_diag_d_addr, (OFIFO_WM << 1) & ~DIAG_MODE, ~0x0);

	HQ3_PIO_WR(ge11_diag_a_addr, DIAG_SEL, 0x7fffffff);
	HQ3_PIO_WR(ge11_diag_d_addr, (GE11_DIAGSEL_INPUT| BIT(4)), ~0x0);

	HQ3_PIO_WR(ge11_diag_a_addr, EXECUTION_CONTROL, 0x7fffffff);
	HQ3_PIO_WR(ge11_diag_d_addr, 0x1, ~0x0);

	/* Restore GIO_CONFIG & HQ3_CONFIG */
	HQ3_DIAG_OFF();
}

int
mgras_ucode_cmd(void)
{
   __uint32_t i, actual; 
   
   msg_printf(VRB,"ucode_cmd\n");

   HQ3_PIO_RD(STATUS_ADDR, ~0x0, actual);
   if ((actual & CFIFO_HW_BIT) != 0  ) {
	msg_printf(VRB, "Fifo full\n");
	return 1;
   }

 
  /* Send token */
   HQ3_PIO_WR(CFIFO1_ADDR, (CP_PASS_THROUGH_GE_WRAM << 8 | (31*4) ) , HQ3_CFIFO_MASK);

   /* Then, send GE command token and data  */
   HQ3_PIO_WR(CFIFO1_ADDR, 30 , HQ3_CFIFO_MASK);

   /* Send a walking 1 pattern */
   for (i = 0; i < 30; i++) {
      HQ3_PIO_WR(CFIFO1_ADDR, i, HQ3_CFIFO_MASK);
   }

   HQ3_PIO_WR(CFIFO1_ADDR, (CP_PASS_THROUGH_GE_FIFO << 8 | (2*4) ) , HQ3_CFIFO_MASK);

   /* Then, send GE command token and data  */
   HQ3_PIO_WR(CFIFO1_ADDR, 1 , HQ3_CFIFO_MASK);

   /* Send a walking 1 pattern */
   HQ3_PIO_WR(CFIFO1_ADDR, 0xc000|cmd_id, HQ3_CFIFO_MASK);
   cmd_id++;

#if 0
   /* read back and compare */
   for (i=0; i < 32*6; i++) {
	actual = GE11_0_read();
	COMPARE("GE11 Commands test", i, i/6, actual, ~0);
   }
#endif

/*   msg_printf(VRB, "GE11 ucode cmd test completed\n"); */

   return 0;
}



/*
 * HQ->GE->RE bus test.
 */
int
mgras_hgr_cmd(void)
{
   __uint32_t i, actual, errors = 0; 
   

   HQ3_PIO_RD(STATUS_ADDR, ~0x0, actual);
   if ((actual & CFIFO_HW_BIT) != 0  ) {
	msg_printf(VRB, "Fifo full\n");
	return -1;
   }

   GE11_ROUTE_GE_2_HQ();

  /* Send token */
   HQ3_PIO_WR(CFIFO1_ADDR, (CP_PASS_THROUGH_GE_WRAM << 8 | (19*4) ) , HQ3_CFIFO_MASK);

   /* Then, send GE command token and data  */
   HQ3_PIO_WR(CFIFO1_ADDR, 18 , HQ3_CFIFO_MASK);

   for (i = 0; i < 18; i++) {
      HQ3_PIO_WR(CFIFO1_ADDR, i, HQ3_CFIFO_MASK);
   }

   HQ3_PIO_WR(CFIFO1_ADDR, (CP_PASS_THROUGH_GE_FIFO << 8 | (2*4) ) , HQ3_CFIFO_MASK);

   /* Then, send GE command token and data  */
   HQ3_PIO_WR(CFIFO1_ADDR, 1 , HQ3_CFIFO_MASK);

   /* Send a walking 1 pattern */
   HQ3_PIO_WR(CFIFO1_ADDR, 0xc000|cmd_id, HQ3_CFIFO_MASK);
   cmd_id++;

   /* read back and compare */
   for (i=0; i < 18; i++) {
	MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT | MGRAS_BC1_FORCE_DCB_STAT_BIT); 
	DELAY(5);
	MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT);
	actual = GE11_Diag_read(~0);
	COMPARE("GE11 hqr_cmd", i, i, actual, ~0, errors);
   }

   GE11_ROUTE_GE_2_RE(); 
   msg_printf(VRB, "GE11 hgr_cmd completed\n"); 

   return 0;
}
