/*	RvEdit.c - Edit 0.2

	RomView Version 1.1a
	Copyright (C) 1990-2 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*	 - Target space - Edit/Display module
*/
#ifdef PIRV
#include <stdio.h>

#include "rvconfig.h"
#include "rvstruct.h"
#include "pidriver.h"
#include "pierror.h"

#ifdef ANSI
extern void rvread(short rvcm);
extern void rvwrite(short rvcm);
extern void rviwrite(long loc, char byte);
extern void pi_beep(void);
#else
extern void rvread();
extern void rvwrite();
extern void rviwrite();
extern void piemu();
extern void pi_beep();
#endif

/* `rvdoedit` - perform edit functions per user input */

void rvdoedit()
	{
	char uline[RVC_CL];
	long i,j,j2,k,sj;
	long words;
#ifdef MAC
	long jj,jjj;
#endif

	words = rxwords;
	rxyloc = rxestart;
	k = 0;
	for (;;)
		{
		rxybc = words;
		if (rxflags&RxIR)
			rvread(RV_RD|RVm_IR);
		else
			if (rxflags&RxIS)
				rvread(RV_RD|RVm_IS);
			else
				rvread(RV_RD);
		if (pxerror)
			return;
		if (!rxelcnt && !k)
			{
			printf("%lX: ",rxyloc);
			for (i=0; i<words; i++)
				{
				printf("%02x",rxybf[i]&0xff);
				}
			printf(" ");
			}
		if (rxelcnt)
			{
			rxelcnt--;
			j = rxelist[k++];
			if (words > 4)
				{
				rxelcnt--;
				j2 = rxelist[k++];
				}
			}
		else
			{
			if (k)
				break;
			if (fgets(uline,RVC_CL,stdin) != NULL)
				{
				if ((uline[0] == '.') || (uline[0] == 'x'))
					return;
				if (uline[0] == '^')
					{
					rxyloc -= words;
					continue;
					}
				if ((uline[0] == '\r') || (uline[0] == '\n'))
					{
					rxyloc += words;
					continue;
					}
#ifdef	MAC
				sj = sscanf(uline,"%X: %X %8lX%8lX",&jj,&jjj,&j,&j2);
				if (sj < 3)
#else
				sj = sscanf(uline,"%8lX%8lX",&j,&j2);
				if (sj < 1)
#endif
					{
					pi_beep();
					continue;
					}
				}
			}
		if (words < 4)
			for (i=0; i<words; i++)
					rxxbf[i] = (char)(j >> (words-1-i)*8);
		else
			for (i=0; i<words; i++)
					{
					if (i<4)
						rxxbf[i] = (char)(j >> (3-i)*8);
					else
						rxxbf[i] = (char)(j2 >> (words-1-i)*8);
					}	
		rxxbc = rxybc;
		rxxloc = rxyloc;
		if (rxflags&RxIR)
			rvwrite(RV_WR|RVm_IR);
		else
			if (rxflags&RxIS)
				{
				for (i=0; i<rxybc; i++)
					if (rxxbf[i] != rxybf[i])
						rviwrite(rxyloc+i,rxxbf[i]);
				}
			else
				rvwrite(RV_WR);
		rxyloc += words;
		}
	}

/* `rvdodump` - dump target space per user request */

void rvdodump()
	{
	char c,asci[18];
	long dct;
	long i,j,k,l,m,n,nn;
	long words;

	dct = rxdend - rxdstart + 1;
	words = rxwords;

	printf("\nDump: word=%d",words*8);
	rxyloc = rxdstart;
	while (dct)
		{
		if (dct > RVC_BS)
			rxybc = RVC_BS;
		else
			rxybc = dct;
		dct -= rxybc;
		if (rxflags&RxIR)
			rvread(RV_RD|RVm_IR);
		else
			if (rxflags&RxIS)
				rvread(RV_RD|RVm_IS);
			else
				rvread(RV_RD);
		n = nn = rxyloc & 0xf;
		if (!pxerror)
			{
			for (i=n,j=0,k=n; i<(rxybc+nn);)
				{
				if (!k)
					{
					printf("\n%08lX:",rxyloc + i - nn);
					k = 1;
					}
				if (n)
					{
					printf("\n%08lX:",rxyloc);
					if (n > 7)
						{
						printf(" ");
						asci[j++] = ' ';
						}
					while (n)
						{
						printf("   ");
						asci[j++] = ' ';
						n--;
						}
					}
				c = rxybf[i-nn];
				i++;
				printf(" %02X",c&0xff);
				if (c < 0x20 || c > 0x7e)
					c = '.';
				asci[j++] = c;
				if (j == 8)
					asci[j++] = ' ';
				asci[j] = '\0';
				if (!(i%8))
					printf(" ");
				if (i == (rxybc+nn))
					{
					l = 16 - (i%16);
					if (l<16)
						{
						for (m=0; m<l; m++)
							{
							printf("   ");
							asci[j++] = ' ';
							}
						if (l > 8)
							{
							printf("  ");
							asci[j++] = ' ';
							}
						else
							printf(" ");
						i += l;
						}
					asci[j] = '\0';
					}
				if (!(i%16))
					{
					printf("|%s|",asci);
					j = 0;
					k = 0;
					}
				}
			}
		else
			break;
		rxyloc += rxybc;
		}
	}

/* `rvdostak` - dump stack */

void rvdostak()
	{
	}
#endif
