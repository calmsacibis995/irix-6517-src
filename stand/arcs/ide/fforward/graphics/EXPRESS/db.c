
#define GR2_DB		1

#include "sys/gr2hw.h"
#include "ide_msg.h"
#include "vidtimdiag.h"
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsc.h"
#include "uif.h"

extern struct gr2_hw *base;
			
#define TRUE 1
#define FALSE 0
#include "xlogo32.h"
#ifdef MFG_USED
#include "box.h"
#endif
#define CLOCK64		0x64	
#define CLOCK64B	0x64b	
#define CLOCK15		0x15	
#define CLOCK107	0x107	
#define CLOCK107B	0x107b	
#define CLOCK132	0x132

#define GEN_ON	0xa6	
#define GEN_OFF	0xa5	
#define DISABLE_INTR	0x3	/* and disable VC1 function */
#define ENABLE_INTR	0x3c	/* enable VC1 function */

#define CURSLST_BASE	0x400
#define FRTABLE_BASE	0x800
#define CURSFRMT_BASE	0x900
#define DIDFRTABLE_BASE	0x4000
#define MAXFRTABLESIZE 	0x900	/* 1024 lines * 2 = 0x800, want this constant
				    to be greater than 0x800, else the DID 
				    table will overlap with the frametable*/ 
#define DIDLINETABLE_BASE (DIDFRTABLE_BASE+MAXFRTABLESIZE)
#define VIDTIM_H60	1	/* High resolution 60 HZ */
#define VIDTIM_H72	4	/* High resolution 72 HZ */
#define VIDTIM_X	20	/* VB1.1 timing table*/	

#define NUMPIX_LINE	1025  /* To write last 4 pixels*/
#define NUMPIX_LINEHI	1279  /* SW workaround*/

#define EXPRESS_EXPHI	0x0000 /* High resolution monitor */
#define EXPRESS_EXPLO	0x0500 /* Low resolution monitor */
#define EXPRESS_INTERLACED 0x2000 
#define BASE_CBLANK	0x1500 /* Low resolution monitor */

#define CURS_BASE	0x3500
#define CURS_NORMAL	0x0000
#define CURS_CROSSHAIR	0x0300

#define CURS_SPECIAL	0x0100
#define CURS_NUMROW	0x0022	/* Number of rows in box special cursor pattern */

#define CURS_ULX 	0x01f4 /* X location of cursor upper left corner */
#define CURS_ULY 	0x01f4	/* Y location of cursor upper left corner */


#define  DUMMYDID  	0x80

/* table for fast clock */
    unsigned char clk64[] = {      0xc4,0x00,0x10,0x20,0x32,0x40,0x5c,0x66,0x70,0x80,0x91,0xa5,0xb6,0xd1,0xe0,0xf0,0xc4} ;

    unsigned char clk64B[] = {     0xc4,0x0f,0x15,0x20,0x30,0x44,0x51,0x60,0x70,0x80,0x91,0xa5,0xb6,0xd1,0xe0,0xf0,0xc4} ;
/* table for slow clock */
      unsigned char clk15[] = {    0xc4,0x00,0x10,0x2e,0x31,0x40,0x5c,0x66,0x70,0x80,0x93,0xa5,0xb6,0xd1,0xe0,0xf0,0xc4} ; 

      unsigned char clk107[] = {   0xc4, 0x0c,0x15,0x26,0x33,0x46,0x5d,0x66,0x70,0x80,0x91,0xa5,0xb6,0xd1,0xe0,0xf0,0xc4} ; 


extern unsigned char clk107B[17]; 

int hires;
int userltable = FALSE;
int Ltvidtim;

extern unsigned char ltableH60[392];
extern unsigned char frtableH60[52];
extern unsigned char ltableH72[244];
extern unsigned char frtableH72[50];
extern unsigned char PLLclk132[7];
extern unsigned char PLLclk107[7];

unsigned char frametable[1024*2];
unsigned char ltableuser[DUMMYDID + 0x60]; 

unsigned char ltable2col[] = {  0x0,0x1,0x0,0x0, 
                                0x0,0x1,0x0,0x1,
                                0x0,0x1,0x0,0x2,
                                0x0,0x1,0x0,0x3,
                                0x0,0x1,0x0,0x4,
                                0x0,0x1,0x0,0x5,
                                0x0,0x1,0x0,0x6,
                                0x0,0x1,0x0,0x7,
                                0x0,0x1,0x0,0x8,
                                0x0,0x1,0x0,0x9,
                                0x0,0x1,0x0,0xa,
                                0x0,0x1,0x0,0xb,
                                0x0,0x1,0x0,0xc,
                                0x0,0x1,0x0,0xd,
                                0x0,0x1,0x0,0xe,
                                0x0,0x1,0x0,0xf,
                                0x0,0x1,0x0,0x10,
                                0x0,0x1,0x0,0x11,
                                0x0,0x1,0x0,0x12,
                                0x0,0x1,0x0,0x13, 
                                0x0,0x1,0x0,0x14,
                                0x0,0x1,0x0,0x15,
                                0x0,0x1,0x0,0x16,
                                0x0,0x1,0x0,0x17,
                                0x0,0x1,0x0,0x18,
                                0x0,0x1,0x0,0x19,
                                0x0,0x1,0x0,0x1a,
                                0x0,0x1,0x0,0x1b,
                                0x0,0x1,0x0,0x1c,
                                0x0,0x1,0x0,0x1d,
                                0x0,0x1,0x0,0x1e,
                                0x0,0x1,0x0,0x1f,
                                0x0,0x5,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0 };


void
Gr2VC1LoadSRAM(
    unsigned short *data,
    unsigned int addr,
    unsigned int length )
{
    int i;

    base->vc1.addrhi = addr >> 8;
    base->vc1.addrlo = addr & 0x00ff;
    for ( i = 0; i < length; i ++  )
    		base->vc1.sram = data[i];
    return;
}

gr2_genlock(int argc, char *argv[])
{
	
	int setting,i;
        unsigned char sysctl;

	
	if (argc < 2)
	{
		msg_printf(ERR, "usage: %s <0 for OFF, 1 for ON>\n", argv[0]);
		return -1;
	}
	argv++;
	setting = atoi(*argv);

	if (setting == 1) {
		/* 0x3c is when video timing is on */
		sysctl = base->vc1.sysctl; 
		base->vc1.sysctl = sysctl & ~GR2_VC1_GENSYNC; 
#if IP22 || IP26 || IP28
		/* Enable genlock and choose NTSC/PAL resolution */
		base->bdvers.rd0 = 0xc2; /* gr2_videoclk */
		base->bdvers.rd1 = 0x05; /* gr2_wrconfig */
#else
        	base->clock.write = GEN_ON;
#endif
		i=15;
		while (i--)
		    ;	
		base->vc1.sysctl = sysctl | GR2_VC1_GENSYNC; /* 0x7c*/ 
        }
	else {	
#if IP22 || IP26 || IP28
                /* Disable genlock and keep NTSC/PAL resolution */
                base->bdvers.rd0 = 0xc7;
                base->bdvers.rd1 = 0x05;
#else
        	base->clock.write = GEN_OFF;
#endif
		sysctl = base->vc1.sysctl & ~GR2_VC1_GENSYNC; 
	}
	return 0;
}

gr2_wrclock(int argc, char *argv[])
{
	int numbytes,i,byte;
	
	
    if (argc < 2) {
    	msg_printf(ERR, "usage: %s #bytes byte0 byte1 etc.\n", argv[0]);
	return -1;
    }

    argv++;
    numbytes = atoi(*argv);

   if (argc < (numbytes + 2)) {
    	msg_printf(ERR, "usage:#bytes byte0 byte1 etc.\n");
	return -1;
   }

    for (i=0; i < numbytes; i++) {
    	argv++;
    	atob(*argv, &byte);
	base->clock.write  = byte & 0xff;
    }
    return 0;
}

gr2_wrPLLclock(int argc, char *argv[])
{
	int i,j, PLLbyte;
	
	
    if (argc < 2) {
    	msg_printf(ERR, "usage: %s byte0 byte1 etc.\n", argv[0]);
	return -1;
    }

   if (argc < (7 + 1)) {
    	msg_printf(ERR, "usage: byte0 byte1 etc.\n");
	return -1;
   }
   for (i=0; i< 7;i++ )
   {
    	argv++;
    	atob(*argv, &PLLbyte);
	for (j=0;j<8;j++) {

		if((i == 6) && (j ==7)) {
               		base->clock.write = (PLLbyte & 0x1) | 0x2;
		}
		else 
               	 	base->clock.write = PLLbyte & 0x1;

                PLLbyte = PLLbyte >> 1;
	}
    }

    return 0;
}
gr2_initclock(int argc, char *argv[])
{
    int i,j,speed,rd0,PLLbyte;
    
    if (argc > 1) {
	    argv++;
	    atob(*argv, &speed);
	    if (argc > 2)
		    msg_printf(VRB, "usage: %s <15, 64, 64b, 107, 107b>\n", argv[0]);
    } else
	    speed = CLOCK107B;

    switch (speed) {
	case CLOCK64:
    	msg_printf(DBG, "Clock set to 64 MHz\n");
        break;

        case CLOCK64B:
    	msg_printf(DBG, "Clock set to 64b\n");
        break;

        case CLOCK15:
    	msg_printf(DBG, "Clock set to 15 MHz\n");
        break;

        case CLOCK107:
    	msg_printf(DBG, "Clock set to 107\n");
        break;

        case CLOCK132:
    	msg_printf(DBG, "Clock set to 132\n");
        break;

        case CLOCK107B:
	default:
    	msg_printf(DBG, "Clock set to 107b - default\n");
        break;

    }

    /*Get board version # */
  rd0 = base->bdvers.rd0;
  if (( ~rd0 & 0x0f) >= 4) {
    for (i=0; i< 7;i++ )
    {
	switch(speed) {
	case CLOCK132:
		PLLbyte = PLLclk132[i];
	break;

	case CLOCK107:
	default:
		PLLbyte = PLLclk107[i];
	break;
    	}
	for (j=0;j<8;j++) {
		if((i == 6) && (j ==7)) {
               		base->clock.write = (PLLbyte & 0x1) | 0x2;
		}
		else 
               		base->clock.write = PLLbyte & 0x1;
                PLLbyte = PLLbyte >> 1;
	}
    }
  } else {

    for (i=0; i< sizeof(clk64); i++ )
    {
	switch(speed) {
	case CLOCK64:
		base->clock.write = clk64[i];
	break;
	case CLOCK64B:
		base->clock.write = clk64B[i];
	break;
	case CLOCK15:
		base->clock.write = clk15[i];
	break;

	case CLOCK107:
		base->clock.write = clk107[i];
	break;

	case CLOCK107B:
		base->clock.write = clk107B[i];
	break;

	case CLOCK132:
    	base->bdvers.rd0  = 2; /* 132Mhz crystal */
	i = sizeof(clk64);
	break;

	}
    }
  }
    return 0;
}

gr2_loadtim(int argc, char *argv[])
{
        if (argc < 2)
        {
                msg_printf(DBG, "Can change video timing with argument %d=h60\n",
                        VIDTIM_H60);
	msg_printf(DBG,"%d = H72, %d=VB1.1 timing table\n", VIDTIM_H72, VIDTIM_X);
		return -1;
        }
        else 
		atob(argv[1], &Ltvidtim);
/*
 * - Write VC1 SYS_CTL register - Disable, and black out display
 */

    base->vc1.sysctl = DISABLE_INTR;

/*
 * - Load Video Timing Table into SRAM
 */

    switch(Ltvidtim) {
    case VIDTIM_H60:
    hires = TRUE;
    Gr2VC1LoadSRAM((unsigned short *)ltableH60, 0, sizeof(ltableH60)/sizeof(short) );
    Gr2VC1LoadSRAM((unsigned short *)frtableH60, FRTABLE_BASE, sizeof(frtableH60)/sizeof(short) );
    break;

    case VIDTIM_H72:
    hires = TRUE;
    Gr2VC1LoadSRAM((unsigned short *)ltableH72, 0, sizeof(ltableH72)/sizeof(short) );
    Gr2VC1LoadSRAM((unsigned short *)frtableH72, FRTABLE_BASE, sizeof(frtableH72)/sizeof(short) );
    break;

    case VIDTIM_X:
    hires = TRUE;
    Gr2VC1LoadSRAM((unsigned short *)ltableX, 0, sizeof(ltableX)/sizeof(short) );
    Gr2VC1LoadSRAM((unsigned short *)frtableX, FRTABLE_BASE, sizeof(frtableX)/sizeof(short) );
    break;

    default:
    msg_printf(ERR,"Timing table %d not defined.\n", Ltvidtim);
    return -1;
    }
	base->vc1.sysctl = 0x003c  & 0xff;   /*Enable VC1 */ 

    return 0;
}

#ifdef NOTUSED
gr2_setdid(int argc, char *argv[])
{
	int i, linecount,index,arg_xpos,xpos, arg_did,did;
	
	int linestart;


	if (argc < 2) {
	msg_printf(DBG, "usage: %s (how many DID for entry 0) <second xpos in base 10> <did#> .. <xposn> <did#> <same format for args to enter split frame>\n", argv[0]); 
		return -1;
	}
	argv++;	
	linecount =  atoi(*argv);

	base->vc1.sysctl = DISABLE_INTR;

	if (linecount <= 0) { 
		msg_printf(ERR,"Need more than 0 DIDs.\n");
		return -1;
	}
	ltableuser[0] = (linecount & 0xff00) >> 8;
	ltableuser[1] = linecount & 0xff;
	ltableuser[2] = 0;
	ltableuser[3] = 0;

	for (i=1;i< linecount; i++) {
	argv++;	
	arg_xpos = atoi(*argv);
	argv++;	
	arg_did = atoi(*argv);

	xpos = (arg_xpos & 0x7f8) >> 3; 
	did = ((arg_xpos  & 0x07) << 5) | (arg_did & 0x1f) ;
		
	ltableuser[2+(i*2)] = xpos;
	ltableuser[2+(i*2)+1] = did;
	}

	linestart = 2+(linecount*2);

	index = 2+((linecount-1)*2);
	while (argc > index) 
	{
	msg_printf(DBG,"Index for next entry: %#04x = %d\n", linestart, linestart);
	argv++;
	linecount =  atoi(*argv);

	index += 1+ ((linecount-1)*2);
	if (index > argc) {
		msg_printf(ERR, "Too many arguments\n");
		return -1;
	}

        ltableuser[linestart] = (linecount & 0xff00) >> 8;
        ltableuser[linestart+1] = linecount & 0xff;

        ltableuser[2+linestart] = 0;
        ltableuser[3+linestart] = 0;

        for (i=1;i< linecount; i++) {
        argv++;
        arg_xpos = atoi(*argv);
        argv++;
        arg_did = atoi(*argv);

	xpos = (arg_xpos & 0x7f8) >> 3;
        did = (arg_did & 0x1f) | ((arg_xpos  & 0x07) << 5);

        ltableuser[2+(i*2)+linestart] = xpos;
        ltableuser[2+(i*2)+1+linestart] = did;
         }
	linestart +=  2+(linecount*2);
	} 

	base->vc1.sysctl = 0x003c  & 0xff;   /*Enable VC1 */
	return 0;
}
#endif

#ifdef MFG_USED
gr2_loadrest(int argc, char *argv[])
{
	int i, curs_mode, curs_numrow,patnum;
	int numychanges,split;
	
	
	msg_printf(DBG, "DIFFERENT usage: %s <Never split - type 0>  <userltable? 0=FALSE, 1=TRUE> <Y0lines> <Y1lines> ...\n", argv[0]); 

	if (argc >= 2 ) {
		argv++;
		split = atoi(*argv);
	}
	else split = FALSE;
	if (argc >= 3 ) {
		argv++;
		userltable = atoi(*argv);
	}
	else userltable = FALSE;

	numychanges = argc - 3;

		curs_mode = CURS_NORMAL;
		curs_numrow = CURS_NUMROW; 
		patnum = 0;  /* for default */

/*
 * - Load DID Table into SRAM   (if needed)
 */
{
    int j,org_numdid,ynew,yold;
    int GR2_DID_index; /*Index into the DID table*/
    int ftablex; /*Index for address of DID entry on n-1 line of frametable*/
 
    if (!hires) {

    GR2_DID_index = 0x00;
    for(i=ynew=org_numdid =0; numychanges > 0 ;numychanges--)
    {
	yold = ynew;
	argv++;
	ynew = atoi(*argv);
        for(j = 0; (j < (ynew-yold)*2) && (i+j < 768*2); j+=2) {
        	frametable[i+j] = 0x49;
        	frametable[i+j+1] = GR2_DID_index;
       	}
/*Update pointer into frame table*/
 	i += (ynew-yold)*2;
	msg_printf(DBG,"ynew = %d, DID index = %d, i = %d,\n", ynew, GR2_DID_index, i); 

/* Update address of next entry in  DID table*/
	if (i < 768 *2) {
	org_numdid = ((ltableuser[GR2_DID_index]) << 8) | ltableuser[GR2_DID_index+1];
	GR2_DID_index += 2+(org_numdid*2);
	}
	else
		break;
    }
    for(; i < 768*2; i+= 2)
    {
        	frametable[i] = 0x49;
        	frametable[i+1] = GR2_DID_index;
    }

/* At the last line of the lusertable, increment DID count by 4, and add
 * 4 pairs of dummy xloc and ids.
 */ 
	/*For the n-1 entry, find the DID entry offset into the DID table*/
	/*NOTE: Since only looking at lower byte, assuming offset <= 0xff */
		switch (Ltvidtim) {

                default: /*Low res*/
                ftablex =  frametable[767*2+1] ;
                break;

		}

	/* Increment the original DID  count by 4 */
		org_numdid = ((ltableuser[ftablex]) << 8) | ltableuser[ftablex+1];
		ltableuser[DUMMYDID] = ((4 + org_numdid) & 0xff00) >> 8 ; 
		ltableuser[DUMMYDID+1] = (4 + org_numdid) & 0xff;

		for (i = 0; i < org_numdid * 2; i+=2) {
	/*Copy the DID  xloc and DID # */
			ltableuser[(DUMMYDID+2)+i] = ltableuser[ftablex+2+i];
			ltableuser[(DUMMYDID+2)+i+1] = ltableuser[ftablex+2+i+1];
		}
		for (j =0; j < 4*2; j+=2) {
			ltableuser[((DUMMYDID+2)+i)+j] = 0;
			ltableuser[((DUMMYDID+2)+i)+j+1] = 0;
		}
	switch (Ltvidtim) {
	default:
		frametable[767*2] = 0x49;
    		frametable[767*2+1] = DUMMYDID; 
		break;
	}

    Gr2VC1LoadSRAM((unsigned short *)frametable, DIDFRTABLE_BASE, 768 );
    msg_printf(DBG, "Using a low res DID frame and line table.\n");
    if (userltable) 
    	Gr2VC1LoadSRAM((unsigned short *)ltableuser,DIDFRTABLE_BASE+MAXFRTABLESIZE, sizeof(ltableuser) /sizeof(short));
    else
    	Gr2VC1LoadSRAM((unsigned short *)ltable2col,DIDFRTABLE_BASE+MAXFRTABLESIZE, sizeof(ltable2col)/sizeof(short) );
    

    } else { /*HI-RES */

	GR2_DID_index = 0x00;
    for(i=ynew=org_numdid =0; numychanges > 0 ;numychanges--)
    {
        yold = ynew;
        argv++;
        ynew = atoi(*argv);
        for(j = 0; (j < (ynew-yold)*2) && (i+j < 1024*2); j+=2) {
                frametable[i+j] = 0x49;
                frametable[i+j+1] = GR2_DID_index;
        }
/*Update pointer into frame table*/
        i += (ynew-yold)*2;
        msg_printf(DBG,"ynew = %d, DID index = %d, i = %d,\n", ynew, GR2_DID_index,
i);

/* Update address of next entry in  DID table*/
        if (i < 1024 *2) {
        org_numdid = ((ltableuser[GR2_DID_index]) << 8) | ltableuser[GR2_DID_index+1];
        GR2_DID_index += 2+(org_numdid*2);
        }
        else
                break;
    }
    for(; i < 1024*2; i+= 2)
    {
                frametable[i] = 0x49;
                frametable[i+1] = GR2_DID_index;
    }
	
/* At the last line of the lusertable, increment DID count by 4, 
 * copy  dids and add 4 pairs of dummy xloc and ids.
 */
	ftablex = (frametable[1023*2] << 8) | frametable[1023*2+1];

        /* Increment the original DID  count by 4 */
             org_numdid = (ltableuser[ftablex] << 8) | ltableuser[ftablex+1];
                ltableuser[DUMMYDID] = ((4 + org_numdid) & 0xff00) >> 8 ;
                ltableuser[DUMMYDID+1] = (4 + org_numdid) & 0xff;

                for (i = 0; i < org_numdid * 2; i+=2) {
        /*Copy the DID  xloc and DID # */
                        ltableuser[(DUMMYDID+2)+i] = ltableuser[ftablex+2+i];
                        ltableuser[(DUMMYDID+2)+i+1] = ltableuser[ftablex+2+i+1];
                }
                for (j =0; j < 4*2; j+=2) {
                        ltableuser[((DUMMYDID+2)+i)+j] = 0;
                        ltableuser[((DUMMYDID+2)+i)+j+1] = 0;
                }

	/* 
	 *	Reassign frametable ptr  to new location in linetable.
	 */ 
    frametable[1023*2] = ((DIDLINETABLE_BASE + DUMMYDID) & 0xff00) >> 8;
    frametable[1023*2+1] = (DIDLINETABLE_BASE + DUMMYDID) & 0xff;

    Gr2VC1LoadSRAM((unsigned short *)frametable, DIDFRTABLE_BASE, sizeof(frametable)/sizeof(short));
    msg_printf(DBG, "Using a hi res DID frame and line table.\n");
    if (userltable)
    	Gr2VC1LoadSRAM((unsigned short *)ltableuser, DIDFRTABLE_BASE+MAXFRTABLESIZE,sizeof(ltableuser)/sizeof(short) );
    else 
	Gr2VC1LoadSRAM((unsigned short *)ltable2col, DIDFRTABLE_BASE+MAXFRTABLESIZE, sizeof(ltable2col) /sizeof(short));
    }
}
/*
 * - Write VC1 GR2_VID_EP, GR2_VID_ENCODE(0x15) register
 */
{
    base->vc1.addrhi = (GR2_VID_EP >> 8) & 0xff;
    base->vc1.addrlo = GR2_VID_EP;
    base->vc1.cmd0 = FRTABLE_BASE;



    base->vc1.addrhi = (GR2_VID_ENAB >> 8) & 0xff;
    base->vc1.addrlo = GR2_VID_ENAB;
/*SW workaround, should be able to just write one byte at 0x44 */

	switch(Ltvidtim) {

	case VIDTIM_H60: /* High resolution monitor*/
	case VIDTIM_H72: /* High resolution monitor*/
	case VIDTIM_X: /* VB1.1 */
    	base->vc1.cmd0 = EXPRESS_EXPHI;
	break;

	default: /*Low resolution*/
                base->vc1.cmd0 = EXPRESS_EXPLO;
		break;
	}

}


/*
 * - Write VC1 WID_EP,WID_END_EP,WID_HOR_DIV & WID_HOR_MOD registers
 */

{
    base->vc1.addrhi = (GR2_DID_EP >> 8) & 0xff;
    base->vc1.addrlo = GR2_DID_EP & 0xff;

    base->vc1.cmd0 = DIDFRTABLE_BASE;

/* Where is the end of the frametable? Depends on # of lines per frame*/
	switch(Ltvidtim) {
		case VIDTIM_H72: /*High res*/
		case VIDTIM_H60: /*High res*/
		case VIDTIM_X: /*High res*/
    		base->vc1.cmd0 = DIDFRTABLE_BASE + 1024*2;
    		base->vc1.cmd0 = ((NUMPIX_LINEHI/5) << 8) | (NUMPIX_LINEHI%5) ;    
		break;

		default: /*Low res*/
    		base->vc1.cmd0 = DIDFRTABLE_BASE + 768*2;
    		base->vc1.cmd0 = ((NUMPIX_LINE/5) << 8) | (NUMPIX_LINE%5);
		break;
    	}

    base->vc1.addrhi = (GR2_BLKOUT_EVEN >> 8) & 0xff;
    base->vc1.addrlo = GR2_BLKOUT_EVEN & 0xff;

    base->vc1.cmd0 = 0xffff;

}

/*
 * - Write VC1 DID mode registers (32x32) (if needed)
 */
{
    int j;
    base->vc1.addrhi = 0;
    base->vc1.addrlo = 0;

	for (i=0x0, j=0;i < 0x40;i+=2, j += 0x10) {
        base->vc1.xmapmode = ((0x3 + j)  & 0xff00) >> 8;
        base->vc1.xmapmode = 0x3 + j;
	}
	for (j = 0;i < 0x80;i+=2, j += 0x10) {
        base->vc1.xmapmode = ((0x3 + j)  & 0xff00) >> 8;
        base->vc1.xmapmode = 0x3 + j;
	}
}


/* load cursor */
{
	
    unsigned char buf[256];
    int numpix, numrow;

    for ( i = 0; i < 128; i++ )
    {
        unsigned char tmp;

        tmp = xlogo32_bits[i];
	/* Reverse bits */
        buf[i] =
                ( ( tmp >> 7 )  & 1 ) |
                ( ( tmp >> 5 )  & 2 ) |
                ( ( tmp >> 3 )  & 4 ) |
                ( ( tmp >> 1 )  & 8 ) |
                ( ( tmp << 1 )  & 0x10 ) |
                ( ( tmp << 3 )  & 0x20 ) |
                ( ( tmp << 5 )  & 0x40 ) |
                ( ( tmp << 7 )  & 0x80 ) ;
        buf[i+128] = 0;
    }


    if ((curs_mode & CURS_SPECIAL) == CURS_SPECIAL )
    {
	numpix = curs_mode & 0xff; 
	if (patnum == 2) {

        msg_printf(DBG, "Size of special cursor pattern: %d bytes by %d rows, total %d BYTES  \n",
                        numpix, curs_numrow, sizeof(pat2));
                Gr2VC1LoadSRAM((unsigned short *)pat2, CURS_BASE, sizeof(pat2)/sizeof(short) );
        }
	else if (patnum == 1) {

	msg_printf(DBG, "Size of special cursor pattern: %d bytes by %d rows, total %d BYTES  \n", 
numpix, curs_numrow, sizeof(pat1));	
		Gr2VC1LoadSRAM((unsigned short *)pat1, CURS_BASE, sizeof(pat1)/sizeof(short));
	}
	else {
	msg_printf(DBG, "Size of special cursor pattern: %d bytes by %d rows, total %d BYTES  \n", 
			numpix, curs_numrow, sizeof(pat0));	
		Gr2VC1LoadSRAM((unsigned short *)pat0, CURS_BASE, sizeof(pat0)/sizeof(short));
	}
    }
    else
    	Gr2VC1LoadSRAM((unsigned short *)buf, CURS_BASE, sizeof(buf)/sizeof(short) );

    base->vc1.addrhi = 0x00;
    base->vc1.addrlo = GR2_CUR_EP;

    base->vc1.cmd0 = CURS_BASE;

    base->vc1.cmd0 = CURS_ULX;

    base->vc1.cmd0 = CURS_ULY;

    base->vc1.cmd0 = curs_mode; /* GR2_CUR_BX byte count for special pattern*/ 

    if ((curs_mode & CURS_SPECIAL) == CURS_SPECIAL )
    {
	curs_numrow &= 0x7ff; 		/* GR2_CUR_LY, 11 bits */
    	base->vc1.cmd0 = curs_numrow; 	
    }

}

	 base->vc1.sysctl = 0x003c  & 0xff;   /*Enable VC1 */
}
#endif /* MFG_USED */

gr2_initVC1(int argc, char *argv[])
{
    int i, vidtim,framesize,screenwidth, interlaced;
    unsigned char version;

    userltable = FALSE;

    msg_printf(DBG, "usage: %s <vidtim>\n", argv[0]);
    msg_printf(DBG, "Can change video timing with argument(%d=h60 default, %d = h72, %d = VB1.1)\n",
			VIDTIM_H60, VIDTIM_H72,VIDTIM_X); 
		
    if (argc >= 2) {
	atob(argv[1], &vidtim);
    } else 
    	vidtim = VIDTIM_H60;

/*
 * - Write VC1 SYS_CTL register - Disable, and black out display		
 */


/*
 * - Load Video Timing Table into SRAM		
 */

{
    switch(vidtim) {
    case VIDTIM_H60: 
    interlaced = FALSE;
    screenwidth = 1279; 
    framesize = 1024*2;
    Gr2VC1LoadSRAM((unsigned short *)ltableH60, 0, sizeof(ltableH60)/sizeof(short) );
    Gr2VC1LoadSRAM((unsigned short *)frtableH60, FRTABLE_BASE, sizeof(frtableH60)/sizeof(short) );
    break;

    case VIDTIM_H72: 
    interlaced = FALSE;
    screenwidth = 1279; 
    framesize = 1024*2;
    Gr2VC1LoadSRAM((unsigned short *)ltableH72, 0, sizeof(ltableH72)/sizeof(short) );
    Gr2VC1LoadSRAM((unsigned short *)frtableH72, FRTABLE_BASE, sizeof(frtableH72)/sizeof(short) );
    break;

    case VIDTIM_X:
    interlaced = FALSE;
    screenwidth = 1279;
    framesize = 1024*2; 
    Gr2VC1LoadSRAM((unsigned short *)ltableX, 0, sizeof(ltableX)/sizeof(short));
    Gr2VC1LoadSRAM((unsigned short *)frtableX, FRTABLE_BASE, sizeof(frtableX)/sizeof(short) );
    break;

    default:
    msg_printf(ERR,"Timing table %d is not defined.\n",vidtim);
    return -1;
    }

}
     /*Read version register*/

     base->vc1.addrhi = (5 >> 8) & 0xff;
     base->vc1.addrlo = 5;

     version = base->vc1.testreg;
     version &= 0x07;
     msg_printf(DBG, "VC1 Chip Revison %d\n", version);
/*
 * - Load DID Table into SRAM	(if needed)	
 */
{

    for( i = 0; i < framesize; i+=2 )
    {
	frametable[i] = 0x49;
	frametable[i+1] = 0 ;
    }
    if (interlaced) { 
    	frametable[(framesize-4)] = 0x49;
    	frametable[(framesize-4)+1] = DUMMYDID; /* 2nd to last visible line*/ 
    }

    frametable[(framesize-2)] = 0x49;
    frametable[(framesize-2)+1] = DUMMYDID; /* Last visible line */ 

    Gr2VC1LoadSRAM((unsigned short *)frametable, DIDFRTABLE_BASE, framesize/(int)sizeof(short));
    Gr2VC1LoadSRAM((unsigned short *)ltable2col, DIDFRTABLE_BASE+MAXFRTABLESIZE, sizeof(ltable2col)/sizeof(short) );

}

/*
 * - Write VC1 GR2_VID_EP, GR2_VID_ENCODE(0x15) register 				
 */

{
    base->vc1.addrhi = (GR2_VID_EP >> 8) & 0xff;
    base->vc1.addrlo = GR2_VID_EP;   

    base->vc1.cmd0 = FRTABLE_BASE;

    base->vc1.addrhi = (GR2_VID_ENAB >> 8) & 0xff;
    base->vc1.addrlo = GR2_VID_ENAB;   

/*SW workaround, should be able to write one byte at addr 0x40 */

    if (!interlaced)
    	base->vc1.cmd0 = EXPRESS_EXPHI;
    else
    	base->vc1.cmd0 = EXPRESS_INTERLACED;
}


/*
 * - Write VC1 WID_EP,WID_END_EP,WID_HOR_DIV & WID_HOR_MOD registers 
 */

{
    base->vc1.addrhi = (GR2_DID_EP >> 8) & 0xff;
    base->vc1.addrlo = GR2_DID_EP & 0xff;   

    base->vc1.cmd0 = DIDFRTABLE_BASE; 

    base->vc1.cmd0 = DIDFRTABLE_BASE + framesize;

    base->vc1.cmd0 = ((screenwidth/5) << 8) | (screenwidth%5);

    base->vc1.addrhi = (GR2_BLKOUT_EVEN >> 8) & 0xff;
    base->vc1.addrlo = GR2_BLKOUT_EVEN & 0xff;   

    base->vc1.cmd0 = 0x0101;  /* Get color from index 1 into colormap*/
 				/* for both odd and even XMAP*/
				/* Choice of colormap set in mode registers*/ 

}

/* load cursor */
{
    unsigned char buf[256];

    for ( i = 0; i < 128; i++ )
    {
	unsigned char tmp;

        tmp = xlogo32_bits[i];
	buf[i] =
		( ( tmp >> 7 )  & 1 ) |
		( ( tmp >> 5 )  & 2 ) |
		( ( tmp >> 3 )  & 4 ) |
		( ( tmp >> 1 )  & 8 ) |
		( ( tmp << 1 )  & 0x10 ) |
		( ( tmp << 3 )  & 0x20 ) |
		( ( tmp << 5 )  & 0x40 ) |
		( ( tmp << 7 )  & 0x80 ) ;
        buf[i+128] = 0;
    }

    Gr2VC1LoadSRAM((unsigned short *)buf, CURS_BASE, sizeof(buf)/sizeof(short) );

    base->vc1.addrhi = 0x00;
    base->vc1.addrlo = GR2_CUR_EP;   

/* Entry point for cursor pattern */   
    base->vc1.cmd0 = CURS_BASE;
	
/*upper left corner, xcoord */
    base->vc1.cmd0 = 0x0240;

/*upper left corner, ycoord */
    base->vc1.cmd0 = 0x0240;

/* default: normal cursor mode */
/*GR2_CUR_BX byte count for special pattern */
    base->vc1.cmd0 = 0x00 ;  
}

/*
 * - Write VC1 SYS_CTL register - Enable VC1 function bit 
 */

{
    
    base->vc1.sysctl = 0x003c  & 0xff; 
}
    return 0;
}

#ifdef NOTUSED
/* Milt Tinkoff's code */

int gr2_bouncedid(int argc, char *argv[])
{
    int i;
    int x = 0;
    int y = 1;
    int dx = 1;
    int dy = 1;
    int xsize, ysize;
    int intr;
    int hires,screenwidth,screenlength;
    int frntporch_start,print_opt;
	int lastlindex,lindex;
    unsigned long lncount;

    if (argc < 3) {
	printf("usage: %s xsize ysize <hires=1(default),lores=0> <intr><print to slow down changes>\n", argv[0]);
	return -1;
    }
	xsize = atoi(*(++argv));
	ysize = atoi(*(++argv));
    if (argc >=4){
	hires = atoi(*(++argv));
    } else hires = 1;
    
    if (argc >=5){
	intr = atoi(*(++argv));
    } else intr = 0;
    if (argc >= 6){
	print_opt = atoi(*(++argv));
    } else print_opt = 0;

    if (hires) {
	screenwidth = 1279;
	screenlength = 1024;
	frntporch_start = 1059;
    }
    else {
	screenwidth = 1024;
	screenlength = 768;
	frntporch_start = 807;
    }

	if (print_opt)
		printf("xsize = %d  ysize  = %d intr = %d print =%d, screenlength = %d\n", 
		xsize,ysize,intr,print_opt,screenlength);
    ltableuser[0] = 0x0;
    ltableuser[1] = 0x2;
    ltableuser[2] = (0 & 0x7f8) >> 3;
    ltableuser[3] = ((0 & 0x007) << 5) | (0 & 0x1f);
    ltableuser[4] = (1 & 0x7f8) >> 3;
    ltableuser[5] = ((1 & 0x007) << 5) | (0 & 0x1f);

    ltableuser[6] = 0x0;
    ltableuser[7] = 0x6;
    ltableuser[8] = (0 & 0x7f8) >> 3;
    ltableuser[9] = ((0 & 0x007) << 5) | (0 & 0x1f);
    ltableuser[10] = (1 & 0x7f8) >> 3;
    ltableuser[11] = ((1 & 0x007) << 5) | (0 & 0x1f);
    ltableuser[12] = 0x0;
    ltableuser[13] = 0x0;
    ltableuser[14] = 0x0;
    ltableuser[15] = 0x0;
    ltableuser[16] = 0x0;
    ltableuser[17] = 0x0;
    ltableuser[18] = 0x0;
    ltableuser[19] = 0x0;

    for (i = 0; i < 2*(screenlength-1); i += 2) {
	frametable[i] = (DIDLINETABLE_BASE) >> 8;
	frametable[i+1] = (DIDLINETABLE_BASE) & 0xff;
    }
    frametable[(screenlength-1)*2] = (DIDLINETABLE_BASE+6) >> 8;
    frametable[(screenlength-1)*2+1] = (DIDLINETABLE_BASE+6) & 0xff;
    
/*Refresh screen*/
	Gr2VC1LoadSRAM((unsigned short *)frametable, DIDFRTABLE_BASE, screenlength);
    	Gr2VC1LoadSRAM((unsigned short *)ltableuser, DIDFRTABLE_BASE+MAXFRTABLESIZE, sizeof(ltableuser)/sizeof(short));

    while(1) {
	if (intr && (y > 656))
		getchar();
	x += dx;
	y += dy;
	if (x < 1) {
	    x = 2;
	    dx = -dx;
	} else if (x+xsize > (screenwidth-1)) {
	    x = (screenwidth-1)-xsize;
	    dx = -dx;
	}
	if (y < 1) {
	    y = 2;
	    dy = -dy;
	} else if (y+ysize > (screenlength-3)) {
		getchar();
	    y = (screenlength-3) - ysize;
	    dy = -dy;
	}
	ltableuser[20+0] = 0x0;
	ltableuser[20+1] = 0x3;
	ltableuser[20+2] = (0 & 0x7f8) >> 3;
	ltableuser[20+3] = ((0 & 0x007) << 5) | (0 & 0x1f);
	ltableuser[20+4] = (x & 0x7f8) >> 3;
	ltableuser[20+5] = ((x & 0x007) << 5) | (1 & 0x1f);
	ltableuser[20+6] = (x+xsize & 0x7f8) >> 3;
	ltableuser[20+7] = ((x+xsize & 0x007) << 5) | (0 & 0x1f);

	if (dy == 1) {
	    frametable[2*(y-1)] = DIDLINETABLE_BASE >> 8;
	    frametable[2*(y-1)+1] = DIDLINETABLE_BASE & 0xff;

	    frametable[2*(y+ysize-1)] = (DIDLINETABLE_BASE+20) >> 8;
	    frametable[2*(y+ysize-1)+1] = (DIDLINETABLE_BASE+20) & 0xff;

	if ((y > 655) && print_opt){
	
		msg_printf(DBG,"y= %d, ysize = %d, x = %d\n",
				y,ysize,x);
		lastlindex = (frametable[(screenlength-1)*2] << 8) | frametable[((screenlength-1)*2)+1];
		msg_printf(DBG,"frametable[screenlength-1] = 0x%x\n", lastlindex);

		for(i=0;i < 15;i++) {
		lindex  = (frametable[(y+ysize-1+i)*2] << 8) | frametable[((y+ysize-1+i)*2)+1];
		msg_printf(DBG,"(y+ysize-1)= %d : frametable[(y+ysize-1)] = 0x%x\n",
			 y+ysize-1+i,lindex);
		}
		for(i=0;i < 18;i+=2) {
			msg_printf(DBG,"ltableuser[%d] = 0x%x	ltableuser[%d] = 0x%x\n",i,ltableuser[i],i+1,ltableuser[i+1]);
		}

	}

	    do {
		base->vc1.addrhi = 0x00;
		base->vc1.addrlo = GR2_CUR_YC;
		lncount = base->vc1.cmd0;
	    } while ((lncount < frntporch_start) && (lncount > 34));
	    if (print_opt)
	    	printf(".");

	    Gr2VC1LoadSRAM((unsigned short *)frametable+2*(y-1), DIDFRTABLE_BASE+2*(y-1), 1);
	    Gr2VC1LoadSRAM((unsigned short *)frametable+2*(y+ysize-1), DIDFRTABLE_BASE+2*(y+ysize-1), 1);
	    Gr2VC1LoadSRAM((unsigned short *)ltableuser+20, DIDLINETABLE_BASE+20, 4);
	if (print_opt && (y > 655)) {
	base->vc1.addrhi = (0x4000 + 2*(y+ysize-1)) >> 8;
        base->vc1.addrlo = (0x4000 + 2*(y+ysize-1)) & 0x00ff;
        for ( i = 0; i < 30; i +=2  )
        {
		register long data;
                data = base->vc1.sram;

		msg_printf(DBG,"frametable[%d] = 0x%x\n",i,data);
        }
	}

	    if((y > 670) && print_opt)
		printf("y=%d\n",y);

	} else {
	    frametable[2*y] = (DIDLINETABLE_BASE+20) >> 8;
	    frametable[2*y+1] = (DIDLINETABLE_BASE+20) & 0xff;

	    frametable[2*(y+ysize)] = DIDLINETABLE_BASE >> 8;
	    frametable[2*(y+ysize)+1] = DIDLINETABLE_BASE & 0xff;

	    do {
		base->vc1.addrhi = 0x00;
		base->vc1.addrlo = GR2_CUR_YC;
		lncount = base->vc1.cmd0;
	    } while ((lncount > frntporch_start ) && (lncount > 34));

	    Gr2VC1LoadSRAM((unsigned short *)frametable+2*y, DIDFRTABLE_BASE+2*y, 1);
	    Gr2VC1LoadSRAM((unsigned short *)frametable+2*(y+ysize), DIDFRTABLE_BASE+2*(y+ysize), 1);

	    Gr2VC1LoadSRAM((unsigned short *)ltableuser+20, DIDLINETABLE_BASE+20, 4);
	}
	if (print_opt)
		printf(".");
    }

}


gr2_ChangeColorMode(int argc, char *argv[])
{
        unsigned short frtableDID[1024];
        register i,did_addr;
        register     SmallMon,colormode;


#define CI_8BIT		0	/*Corresponds to DID#*/
#define RGB_24BIT	1

#define ADDR_8BITCI	0x4900+(CI_8BIT*16) 	
#define ADDR_24BITRGB	0x4900+(RGB_24BIT*16)	
	argv++;
	colormode = atoi(*argv);

	SmallMon = 0;

        if (SmallMon) {
                /*
                 * Load DID table
                 */
                /*XXX found out where defines the X,Y resolution */
                /*XXX REPLACE 768 later */
                /*XXX Need modify, if VC1 supports line repeat count */
		/*XXX FULL SCREEN DIDS */ 
			switch(colormode){
			case CI_8BIT:
                        	did_addr = ADDR_8BITCI;
				break;
			 case RGB_24BIT:
				did_addr = ADDR_24BITRGB;
				break;
			}
                for (i=0; i<767; i++) {
                	frtableDID[i] = did_addr;
		}

                /* HACK, bug in VC1.  4 dummy DID's needed. */
                frtableDID[i] = did_addr + 4;

                Gr2VC1LoadSRAM((unsigned short *)frtableDID, 0x4000, 768);
        }
	else {
                /*XXX found out where defines the X,Y resolution */
                /*XXX Need modify, if VC1 supports line repeat count */
		/*XXX FULL SCREEN DIDS */ 
			switch(colormode){
			case CI_8BIT:
                        	did_addr = ADDR_8BITCI;
				break;
			 case RGB_24BIT:
				did_addr = ADDR_24BITRGB;
				break;
			}
                for (i=0; i<1024; i++) {
                	frtableDID[i] = did_addr;
		}

                /* HACK, bug in VC1.  4 dummy DID's needed. */
                frtableDID[i] = did_addr + 4;

                Gr2VC1LoadSRAM((unsigned short *)frtableDID, 0x4000, 1024);
	}
}
	
gr2_CursorMode(int argc, char *argv[])
{
	long patnum, curs_mode,curs_numrow,numbytes;
	 msg_printf(DBG, "usage: %s <curs_mode (default=0, special=1, crosshair = 3)> <numrows in special bitmap> <numbytes in special bitmap> <special patnum>\n", argv[0]); 

	if (argc >= 2 ) {
                atob(*(++argv), &curs_mode);
        } else curs_mode = 0;  
	if (argc >= 3 ) 
                atob(*(++argv), &numbytes);
	if (argc >= 4 ) 
                atob(*(++argv), &curs_numrow);
	if (argc >= 5 ) 
                atob(*(++argv), &patnum);

	base->vc1.sysctl = DISABLE_INTR;
     if (curs_mode == 1) /*Special*/ {
        if (patnum == 2) {
                Gr2VC1LoadSRAM((unsigned short *)pat2, CURS_BASE, sizeof(pat2)/sizeof(short) );
		msg_printf(DBG, "Size of special cursor pattern: %d bytes by %d rows, total %d BYTES  \n",
                	numbytes, curs_numrow, sizeof(pat2));

        }
        else if (patnum == 1) {

                Gr2VC1LoadSRAM((unsigned short *)pat1, CURS_BASE, sizeof(pat1) /sizeof(short));
        	msg_printf(DBG, "Size of special cursor pattern: %d bytes by %d rows, total %d BYTES  \n",
			numbytes, curs_numrow, sizeof(pat1));
        }
        else {
                Gr2VC1LoadSRAM((unsigned short *)pat0, CURS_BASE, sizeof(pat0)/sizeof(short) );
        msg_printf(DBG, "Size of special cursor pattern: %d bytes by %d rows, total %d BYTES  \n",
                        numbytes, curs_numrow, sizeof(pat0));
        }
      } else if (curs_mode == 0) { 

    unsigned char buf[256];
    int i;

    for ( i = 0; i < 128; i++ )
    {
        unsigned char tmp;

        tmp = xlogo32_bits[i];
        /* Reverse bits */
        buf[i] =
                ( ( tmp >> 7 )  & 1 ) |
                ( ( tmp >> 5 )  & 2 ) |
                ( ( tmp >> 3 )  & 4 ) |
                ( ( tmp >> 1 )  & 8 ) |
                ( ( tmp << 1 )  & 0x10 ) |
                ( ( tmp << 3 )  & 0x20 ) |
                ( ( tmp << 5 )  & 0x40 ) |
                ( ( tmp << 7 )  & 0x80 ) ;
        buf[i+128] = 0;
    }

		Gr2VC1LoadSRAM((unsigned short *)buf, CURS_BASE, sizeof(buf) /sizeof(short));	
	}

	base->vc1.addrhi = 0x00;
    	base->vc1.addrlo = GR2_CUR_MODE;

/* default: normal cursor mode */
/*GR2_CUR_BX byte count for special pattern */
	base->vc1.cmd0 = (curs_mode << 8) |numbytes; 

	if (curs_mode == 1) /*Special*/ {
		curs_numrow &= 0x7ff;           /* GR2_CUR_LY, 11 bits */
        	base->vc1.cmd0 = curs_numrow;
	}
	base->vc1.sysctl = ENABLE_INTR;
}
gr2_movecurs(int argc, char *argv[])
{
	long x,y;
	if (argc < 2) {
		msg_printf(VRB,"usage: %s decimal_xcoord <ycoord, default 300>\n",argv[0]);
		return -1;
	}

	x =  atoi(*(++argv));

	if (argc == 3)
		 y =  atoi(*(++argv));
	else y = 300;
		
                y +=  GR2_CURS_YOFF_1280;
	if (x <  1024){
	while (1) {
                if (*(volatile unchar *)PHYS_TO_K1(VRSTAT_ADDR) & VRSTAT_MASK)                        break;
        }

                base->vc1.addrhi =    (GR2_VID_EP >> 8) & 0xff ;
                base->vc1.addrlo =    GR2_VID_EP & 0xff ;
                base->vc1.cmd0 = FRTABLE_BASE;

         x +=  GR2_CURS_XOFF_1280 ;
	base->vc1.addrhi =    (GR2_CUR_XL >> 8) & 0xff ;
        base->vc1.addrlo =     GR2_CUR_XL & 0xff ;
        base->vc1.cmd0 =        x ;
        base->vc1.cmd0 =        y ;


        } else {        /* x >= 1024 */

	while (1) {
                if (*(volatile unchar *)PHYS_TO_K1(VRSTAT_ADDR) & VRSTAT_MASK)                        break;
        }

                base->vc1.addrhi =    (GR2_VID_EP >> 8) & 0xff ;
                base->vc1.addrlo =    GR2_VID_EP & 0xff ;
                base->vc1.cmd0 = CURSFRMT_BASE ;

	base->vc1.addrhi =    (GR2_CUR_XL >> 8) & 0xff ;
        base->vc1.addrlo =     GR2_CUR_XL & 0xff ;
        base->vc1.cmd0 =        x ;
        base->vc1.cmd0 =        y ;

        }
	return 0;

}
#endif /* NOTUSED */
