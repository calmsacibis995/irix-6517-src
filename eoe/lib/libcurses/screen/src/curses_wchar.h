/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/curses_wchar.h	1.1"
#define SS2     0x8e
#define SS3     0x8f
#define H_EUCMASK 0x8080
#define H_P00     0          	  /* Code Set 0 */
#define H_P11     0x8080          /* Code Set 1 */
#define H_P01     0x0080          /* Code Set 2 */
#define H_P10     0x8000          /* Code Set 3 */
#define EUCMASK 0x30000000
#define P00     0          	    /* Code Set 0 */
#define P11     0x30000000          /* Code Set 1 */
#define P01     0x10000000          /* Code Set 2 */
#define P10     0x20000000          /* Code Set 3 */

#ifdef __STDC__
#define _ctype __ctype
#endif
extern unsigned char _ctype[];

#define multibyte       (_ctype[520]>1)
#define eucw1   _ctype[514]	/* length of code set 1 */
#define eucw2   _ctype[515]	/* length of code set 2 */
#define eucw3   _ctype[516]	/* length of code set 3 */
#define scrw1   _ctype[517]	/* width of code set 1 */
#define scrw2   _ctype[518]	/* width of code set 2 */
#define scrw3   _ctype[519]	/* width of code set 3 */
#define _mbyte  _ctype[520]
