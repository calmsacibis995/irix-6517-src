/* --------------------------------------------------- */
/* | Copyright (c) 1988 MIPS Computer Systems, Inc.  | */
/* | All Rights Reserved.                            | */
/* --------------------------------------------------- */
/* $Header: /proj/irix6.5.7m/isms/stand/arcs/dmon/include/RCS/gen_ioctl.h,v 1.1 1994/07/20 22:54:56 davidl Exp $ */


/*
 * Generic header file for ioctls
 */

#ifndef _SYS_GEN_IOCTL_
#define _SYS_GEN_IOCTL_	1

/* string returned by GIOCPRSTR contains the following fields */
#define IDENT_VENDOR	8
#define IDENT_PRODUCT	16
#define IDENT_REVISION	4
#define IDENT_MICRO	8
#define IDENT_SERIAL	12
#define IDENT_TYPE	16

#define IDENT_LEN	(IDENT_VENDOR+IDENT_PRODUCT+IDENT_REVISION+IDENT_MICRO+IDENT_SERIAL+IDENT_TYPE)

#define _GIOC_(x)	(('G'<< 8)|(x))

#define GIOCPRSTR	_GIOC_(0)	/* Print identification string */
#define GIOCGETVAL	_GIOC_(1)	/* Get current scsi modes	*/
#define GIOCSETVAL	_GIOC_(2)	/* Set current scsi modes	*/


/* 
 * Right now, this structure is only used to print strings.
 * Eventually, this may contain the modes for the get/set values
 */
struct gioctl{
    union {
	char string[IDENT_LEN];
	struct {
	    int retval;
	    int page_code;
	    int datasz;
	    int memaddr;
	} modes;
    } gioc_info;
};

#define gi_ident	gioc_info.string
#define gi_retval	gioc_info.modes.retval
#define gi_page		gioc_info.modes.page_code
#define gi_size		gioc_info.modes.datasz
#define gi_addr		gioc_info.modes.memaddr

/*
 * return values returned in retval when EIO returned
 */
#define GIOC_BADSIZE    1
#define GIOC_OPERR      2
#define GIOC_EFAULT     3
#define GIOC_EINVAL     4


#endif _SYS_GEN_IOCTL_
