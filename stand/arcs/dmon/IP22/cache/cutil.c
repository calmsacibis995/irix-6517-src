#ident	"$Id: cutil.c,v 1.1 1994/07/20 23:46:11 davidl Exp $"
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#include "sys/types.h"

GetOptions_Loop()
{
	u_int response;

	puts("\n\rDo you want to Loop on first error? (y = yes, n = no):  ");
	response = getchar();
	if( response != 'y')
		return(0);
	else
		return(1);
}
GetOptions_Quiet()
{
	u_int response;

	puts("\n\rDo you want to run in quiet mode ? (y = yes, n = no):  ");
	response = getchar();
	if( response != 'y') {
		return(0);
	}	
	else {
		return(1);
	}
}
GetFirstAddr()
{
	u_int junk[40];
	u_int value;

	puts("\n\rEnter starting Cache address: ");
	_gets( &junk);
	value = atoi( &junk);
	return( value);
}
GetLastAddr()
{
	u_int junk[40];
	u_int value;

	puts("\n\rEnter last Cache address: ");
	_gets( &junk);
	value = atoi( &junk);
	return( value);
}


/* Mask for right 16 bits */
#define R16BITS	0x0000ffff

/* Mask for left 16 bits */
#define L16BITS 0xffff0000


/*	G U I N N E S S  S T U F F */

#if	defined(IP24)
/* Chip coordinates for secondary cache chips on a Fab 3 board */
static u_char *fab3_chip[4][2] = {
	{ "u2", "u1"},
	{ "u5", "u3"},
	{ "u8", "u6"},
	{ "u11", "u9"}
	};

/* Chip coordinates for secondary cache chips on a Fab 4 board */
static u_char *fab4_chip[4][2] = {
	{ "u504", "u500"},
	{ "u505", "u502"},
	{ "u503", "u501"},
	{ "u11", "u7"}
	};
#endif

/*	F U L L H O U S E   S T U F F  */

#if	!defined(IP24) & defined(IP22)

/* Chip coordinates for secondary cache chips on a PM1 board */
static u_char *pm1_chip[4][2] = {
	{ "u2", "u1"},
	{ "u5", "u3"},
	{ "u8", "u6"},
	{ "u11", "u9"}
	};

/* Chip coordinates for secondary cache chips on a PM2 board */
static u_char *pm2_chip[4][2] = {
	{ "u12", "u3"},
	{ "u13", "u2"},
	{ "u14", "u5"},
	{ "u11", "u4"}
	};
#endif

/* Starting bit number for each word of a quad word */
/* Secondary read a 128bits at a time */
static u_int bitnumber[] = { 96, 64, 32, 0};


/*	b a d _ s c a c h e _ c h i p ()
 *
 * bad_scache_chip()- determines the failing secondary cache bit/chip for
 * Indy machines.
 */
bad_scache_chip( u_int failaddr, u_int xordata)
{
	u_int tblindx, bad_bit, i, errdata;

	/* Get the failing word */
	tblindx = (failaddr & 0xc) >> 2;

	/* Get the starting bit number of the failing word */
	bad_bit = bitnumber[ tblindx];

	puts("\r\nFailure on bit(s):\n\r");
	errdata = xordata;

	/* display the failing bits */
	for(i=0; i < 32; i++ ) {

		if( errdata & 1) {
			/* Don't want 4 spaces between digits so add
			 * if above 99 for three digit numbers 
			 */
			if( (i + bad_bit) > 99)
				puts(" ");
			/* putdec prints the number of characters
			 * specified, blanking filling if necessary
			 */
			putdec((i + bad_bit), 3 );
		}
		errdata >>= 1;
	}
		
	puts("\n\rProbable failing chip\n\r");

#if	defined(IP24) 
	puts("  FAB 3 -> ");
	if( xordata & R16BITS) {
		puts( fab3_chip[ tblindx][0]);
		puts("  ");
	}
	
	if( xordata & L16BITS) {
		puts( fab3_chip[ tblindx][1]);
	}
	
	puts("\n\r  FAB 4 -> ");
	if( xordata & R16BITS) {
		puts(fab4_chip[ tblindx][0]);
		puts("  ");
	}
	
	if( xordata & L16BITS) {
		puts( fab4_chip[tblindx][ 1]);
	}
#endif

#if	!defined(IP24) & defined(IP22) 
	puts("  PM1  -> ");
	if( xordata & R16BITS) {
		puts( pm1_chip[ tblindx][0]);
		puts("  ");
	}
	
	if( xordata & L16BITS) {
		puts( pm1_chip[ tblindx][1]);
	}
	
	puts("\n\r  PM2 -> ");
	if( xordata & R16BITS) {
		puts(pm2_chip[ tblindx][0]);
		puts("  ");
	}
	
	if( xordata & L16BITS) {
		puts( pm2_chip[tblindx][ 1]);
	}
#endif
	puts("\n\r");
	return;
}
