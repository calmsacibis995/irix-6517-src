/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * This file defines a structure used for sar accounting on various
 * SGI disk drivers.
 */
struct disksar
{
	struct iotime 	iot;		/* see elog.h */
	time_t		stamp;  	/* time drive last went active */
	unsigned short  commands;	/* # commands outstanding for drive*/
	unsigned short	attached;	/* inc on open, dec on close */
}; 
