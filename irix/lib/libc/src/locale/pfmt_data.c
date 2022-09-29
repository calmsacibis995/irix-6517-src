/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/pfmt_data.c	1.2"

#include "synonyms.h"
#include <pfmt.h>

char __pfmt_label[MAXLABEL+1] _INITBSSS;
struct sev_tab *__pfmt_sev_tab _INITBSS;
int __pfmt_nsev _INITBSS;
