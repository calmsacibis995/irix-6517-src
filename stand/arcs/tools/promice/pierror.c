/*	PiError.c - Edit 6

	LoadICE Version 3
	Copyright (C) 1990-95 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*	 - Display error
*/

#include <stdio.h>

#include "piconfig.h"
#include "pistruct.h"
#include "pierror.h"

extern int errno;

void pierror()
	{
	if (pxerror == PGE_NOP)
		{
		pxerror = PGE_NOE;
		return;
		}

	if (pxerror)
		{
		if (pxerror >= PGE_MAX || pxerror < 0)
			{
			printf("\n-UNKNOWN ERROR CODE > %ld",pxerror);
			printf("\n most likely a communication error has occured");
			}
		else
			{
			if (pxerror > 6)
				printf("\n-ERROR: (%ld) - %s",pxerror-6, pxerrmsg[pxerror]);
			else
				printf("\n-ERROR: (%ld) - %s",pxerror-7, pxerrmsg[pxerror]);
			if (pi_estr1 != (char *)0)
				{
				printf("\n-ERROR-STRING: `%s`",pi_estr1);
				pi_estr1 = (char *)0;
				}
			if (pxdisp&PXMI)
				{
				if (pi_eloc != (char *)0)
					{
					printf("\n-ERROR-LOCATION: `%s`",pi_eloc);
					pi_eloc = (char *)0;
					}
				if (pi_estr2 != (char *)0)
					{
					printf("\n-ERROR-STRING2: `%s`",pi_estr2);
					pi_estr2 = (char *)0;
					}
				if (pi_eflags&PIE_NUM)
					{
					printf("\n-ERROR-DATA: %lX / %lX",pi_enum1,pi_enum2);
					pi_eflags &= ~PIE_NUM;
					}
				}
			if (errno)
				{
#ifdef MACAPPL
				printf("\n-SYSTEM ERROR: CODE = %d",errno);
#else
				perror("\n-SYSTEM ERROR: ");
#endif
				errno = 0;
				}
			}
		piflags |= PiER;
		}
	if (pxerror == PGE_TMO)
		piflags &= ~PiUP;		/* link is not up */
	if (piflags&PiiX)
		pxerror = PGE_NOE;
	else
		pxexitv = PI_FAILURE;
	}
