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

#ifndef _HWGENERA_H
#define _HWGENERA_H

/***************************************************************************
*
*  Module Name:   HWGENERA.H
*
*  Description:   Definitions which are generic and general
*    
*  Owners:	  ECX IC Firmware Team
*    
*  Notes:	  
*
***************************************************************************/

/***************************************************************************
* Generic (extended) data type
***************************************************************************/
typedef char CHAR;			  /* character			*/
typedef unsigned char UCHAR;		  /* unsigned character		*/
typedef int INT;			  /* integer			*/
typedef unsigned int UINT;		  /* unsigned integer		*/
typedef long LONG;			  /* long			*/
typedef unsigned long ULONG;		  /* unsigned long		*/
typedef CHAR BYTE;			  /* signed byte		*/
typedef UCHAR UBYTE;			  /* unsigned byte		*/
typedef short int SHORT;		  /* short integer		*/
typedef unsigned short USHORT;		  /* unsigned short		*/

#define SHIM_DEBUG		0
#define LIP_DEBUG		0
#define QSIZE_WAR		1

#endif /* _HWGENERA_H */
