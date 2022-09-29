 /**************************************************************************
 *									  *
 *		Copyright ( C ) 1984, 1990, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*
 * Internals for the dials driver -- stolen from the
 * mouse driver
 *
 * Most of the #define's are "dial" dependant,
 * most of the "warp" factors are not.
 */
#ident "$Revision: 1.1 $"

/* Need to deal with buttons from the button box */

#ifndef DL_H
#define DL_H 1
#define	DL_MAX_LEDS	32
#define	DL_MAX_BUTTONS	32
#define	DL_MAX_DIALS	8
#define	DL_MAX_TEXT_LEN	9

#define	DL_ALL_DIALS	(0xffff)
#define	DL_ALL_BUTTONS	(0xffffffff)
#define	DL_ALL_LEDS	(0xffffffff)

#define	DL_WHICH_DEVICE	0
#define	DL_VALUE_HIGH	1
#define	DL_VALUE_LOW	2
#define	DL_N_STATES	3

	/* dial/button box commands */
#define DL_INITIALIZE		0x20
#define	DL_SET_LEDS		0x75
#define	DL_SET_TEXT		0x61
#define	DL_SET_AUTO_DIALS	0x50
#define	DL_SET_BUTTONS_MOM_TYPE	0x71
#define	DL_SET_AUTO_MOM_BUTTONS	0x73

	/* dial/button box replies */
#define DL_INITIALIZED		0x20
#define DL_BASE_DIAL_VALUE	0x30
#define DL_BASE_DIAL_DELTA	0x40
#define DL_BASE_BTN_PRESS	0xc0
#define DL_BASE_BTN_RELEASE	0xe0


#define	DL_MIN_VAL		((short)0x8000)
#define	DL_MAX_VAL		((short)0x7fff)

#define	QE_DIAL_QINDEX		4	/* A LIE!! DOESN'T BELONG HERE */

struct _dial_button_state {
	/* Actual state */
	unsigned short	hwPos[DL_MAX_DIALS];
	unsigned char	btnPressed[DL_MAX_BUTTONS];
	unsigned char	text[DL_MAX_TEXT_LEN];
	unsigned long	activeDials;
	unsigned long	activeBtns;
	unsigned long	ledState;

	/* Scratch Intra-report Data */
	signed char state;
	unsigned char report[DL_N_STATES] ;

	/* Stuff to play by the shmiq rules */
	struct shmiqlinkid shmiqid ;

	/* Misc. Std. Driver stuff */
	unsigned int inited :1;	/* true if data structures are set */
				/* and init sequence has been sent to */
				/* device */
	unsigned int ready :1;	/* true if we've received confirmation */
				/* of the init sequence from the device */
	struct queue *rq;
	struct queue *wq;
} ;
#endif
