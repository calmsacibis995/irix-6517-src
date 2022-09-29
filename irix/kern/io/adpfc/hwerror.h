/***************************************************************************
*									   *
* Copyright 1997 Adaptec, Inc.,  All Rights Reserved.			   *
*									   *
* This software contains the valuable trade secrets of Adaptec.  The	   *
* software is protected under copyright laws as an unpublished work of	   *
* Adaptec.  Notice is for informational purposes only and does not imply   *
* publication.	The user of this software may make copies of the software  *
* for use with parts manufactured by Adaptec or under license from Adaptec *
* and for no other use.							   *
*									   *
***************************************************************************/

#ifndef _HWERROR_H
#define _HWERROR_H

/*
*
*  Module Name:   hwerror.h
*
*  Description:   Error codes shared by ULM/CHIM clients
*    
*  Owners:	  ECX IC Firmware Team
*    
*  Notes:	  
*
*/

/* errors for sh_initialize	*/
#define CANNOT_LOAD_SEQUENCER	-1
#define BIST_FAILED		-2
#define RAM_TEST_FAILED		-3

/* errors for sh_get_configuration */
#define NO_SRAM			-1
#define CANT_FIND_LINK_SPEED	-2

/* definitions for sh_frontend_isr */
#define NO_INTERRUPT_PENDING	0
#define INTERRUPT_PENDING	1
#define LONG_INTERRUPT_PENDING	2

/* definitions for return status for sh_special				*/
#define SH_SUCCESS		0
#define SH_REQUEST_FAIL		-1
#define SH_ILLEGAL_REQUEST	-2

/* definitions for error codes from ulm_event				*/


#endif /* _HWERROR_H */
