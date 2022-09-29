/*	PiEdit.c - Edit 6

	LoadICE Version 3
	Copyright (C) 1990-95 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*	 - PROMICE - Edit/Display module
*/

#include <stdio.h>

#include "piconfig.h"
#include "pistruct.h"
#include "pierror.h"

#ifdef ANSI
extern short piconfig(short code);
extern void piread(void);
extern void piwrite(void);
extern void pi_beep(void);
extern void pi_kbd(void);

#else
extern short piconfig();
extern void piread();
extern void piwrite();
extern void pi_beep();
extern void pi_kbd();
#endif

/* `pidoedit` - perform edit functions per user input */

void pidoedit()
	{
	char uline[PIC_CL];
	long i,j,j2,k,sj;
#ifdef MAC
	long jj,jjj;
#endif

	
	if (pxestart<pxcfg->start)
		{
		pxerror = PGE_BAA;
		return;
		}
 	if (pxestart>pxmax)
		{
		pxerror = PGE_AOR;
		return;
		}
	if (pxdisp&PXMI)
		piconfig(PcCFG);
	pxyloc = pxestart;
	k = 0;
	printf("\n");
	for (;;)
		{
		if (pxyloc <pxcfg->start)
			{
			pi_beep();
			pxyloc = pxcfg->start;
			}
		if (pxyloc > pxmax)
			{
			pi_beep();
			pxyloc = pxmax-pxcfg->words;
			}
		pxybc = pxcfg->words;
		piread();
		if (pxerror)
			return;
		if (!pxelcnt && !k)		/* if no list of data given */
			{
			printf("%lX: ",pxyloc);
			for (i=0; i<pxcfg->words; i++)
				{
				printf("%02x",pxybf[i]&0xff);
				}
			printf(" ");
			}
		if (pxelcnt)			/* else do it quitely */
			{
			pxelcnt--;
			j = pxelist[k++];
			if (pxcfg->words > 4)
				{
				pxelcnt--;
				j2 = pxelist[k++];
				}
			}
		else					/* interactive editing */
			{
			if (k)
				break;
			if (fgets(uline,PIC_CL,stdin) != NULL)
				{
				if ((uline[0] == '.') || (uline[0] == 'x'))
					return;
				if (uline[0] == '^')	/* previous location */
					{
					pxyloc -= pxcfg->words;
					continue;
					}
				if ((uline[0] == '\r') || (uline[0] == '\n'))	/* next loc. */
					{
					pxyloc += pxcfg->words;
					continue;
					}
#ifdef	MACAPPL

				sj = sscanf(uline,"%8lX%8lX",&j,&j2);
				if (sj < 1)
#else
#ifdef	MAC
				sj = sscanf(uline,"%X: %X %8lX%8lX",&jj,&jjj,&j,&j2);
				if (sj < 3)
#else
				sj = sscanf(uline,"%8lX%8lX",&j,&j2);
				if (sj < 1)
#endif
#endif
					{
					pi_beep();
					continue;
					}
				}
			}
		if (pxcfg->words < 4)	/* less than a long word */
			for (i=0; i<pxcfg->words; i++)
					pxybf[i] = (char)(j >> (pxcfg->words-1-i)*8);
		else
			for (i=0; i<pxcfg->words; i++)	/* we can edit 64-bit words */
					{
					if (i<4)
						pxybf[i] = (char)(j >> (3-i)*8);
					else
						pxybf[i] = (char)(j2 >> (pxcfg->words-1-i)*8);
					}	
		for (i=0; i<pxybc; i++)
			pxxbf[i] = pxybf[i];
		pxxbc = pxybc;
		pxxloc = pxyloc;
		piwrite();
		pxyloc += pxcfg->words;
		}
	piflags &= ~PiEL;
	}

/* `pidodump` - dump ROM data per user request */

void pidodump()
	{
	char c,asci[18];
	long dct;
	long i,j,k,l,m,n,nn;

	piflags |= PiDF;

	if ((pxdstart<0) || (pxdend<0) || (pxdstart>pxdend))
		{
		pxerror = PGE_BAA;
		return;
		}
	if ((pxdstart>pxmax) || (pxdend>pxmax))
		{
		pxerror = PGE_AOR;
		return;
		}
	dct = pxdend - pxdstart + 1;
	
	if (pxdisp&PXMI)
		piconfig(PcCFG);

	printf("\nDump: word=%d ID ",pxcfg->words*8);
	for (i=0; i<pxcfg->words; i++)
		printf("%d ",pxcfg->uid[i]);
	if (pxbanks)
		printf(" Bank %ld",pxbank);
	pxyloc = pxdstart;
	while (dct && !pxerror)
		{
		if (dct > PIC_BS)
			pxybc = PIC_BS;
		else
			pxybc = dct;
		dct -= pxybc;
		piread();
		n = nn = pxyloc & 0xf;
		if (!pxerror)
			{
			for (i=n,j=0,k=n; i<(pxybc+nn);)
				{
				pi_kbd();	/* user keyboard activity monitor */
				if (pxerror)	/* user did abort! */
					break;
				if (!k)
					{
					printf("\n%08lX:",pxyloc + i - nn);
					k = 1;
					}
				if (n)
					{
					printf("\n%08lX:",pxyloc);
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
				c = pxybf[i-nn];
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
				if (i == (pxybc+nn))
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
		pxyloc += pxybc;
		}
	piflags &= ~PiDF;
	}
