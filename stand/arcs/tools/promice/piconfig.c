/*	PiConfig.c - Edit 6

	LoadICE Version 3
	Copyright (C) 1990-95 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*	 - Display config
*/

#include <stdio.h>

#include "piconfig.h"
#include "pistruct.h"

#ifdef ANSI
static void pidcfg(PICONFIG *cx,char *hdr);
void piconfig(short cd)
#else
static void pidcfg();

void piconfig(cd)
short cd;
#endif
	{
	short i;
	PIROM *pr;
	PIFILE *pf;
	char *ut;
	
	if (cd&PcLNK)
		{
		if (pxprom)
			{
			printf("\n\nHOST TO PromICE CONNECTION");
			if (piflags&PiNE)
				printf("\n Network Host: %s",pxhost);
			if (pxlink.flags&(PLPP|PLPQ))
				{
				if (piflags&PiNE)
					printf("\n  Parallel link:");
				else
#ifdef MAC
					printf("\n Parallel link: @0x%X",pxlink.ppdat);
#else
					printf("\n Parallel link: %s(@0x%X)",
						pxlink.pname,pxlink.paddr);
#endif
				if (pxflags&POSP)
					printf(" Standard-mode");
				else
					if (pxflags&POVF)
						printf(" Fast-mode");
					else
						printf(" Turbo-mode");
				if (pxlink.flags&PLPB)
						printf(" Bus-mode");
				if (pxlink.flags&PLPQ)
					printf(" Bidirectional");
				else
					printf(" Down-load only");
				if (piflags&PiZZ)
					printf("\n  Strobe-stretch ENABLED, delays are %d,%d,%d",
						ppxdl0,ppxdl1,ppxdl2);
				}
			if (!(pxlink.flags&PLPQ))
				{
				if (piflags&PiNE)
					printf("\n  Serial link: @%ld baud",pxlink.brate);
				else
#ifdef MSDOS
					printf("\n Serial link: %s(@0x%X) @%ld baud",
									pxlink.name,pxlink.saddr,pxlink.brate);
#else
					printf("\n Serial link: %s @%ld baud",
									pxlink.name,pxlink.brate);
#endif
				if (pxlink.flags&PLHI)
					printf(" (high speed response is on)");
				if (pxlink.flags&PLOW)
					printf(" (slow speed response is on)");
				}
			printf("\n\nOPTIONS IN EFFECT:");
			if (pxflags&POAR)
				printf("\n Autorecovery of link on timeout errors ENABLED");
			if (pxflags&PORQ)
				printf("\n Req/Ack (Hold/HoldAck) mode is ON");
			if (pxflags&POVF)
				printf("\n Verifying down-loaded data via block-checksum");
			else
				printf("\n Data verification via block-checksum is OFF");
			if (pxflags&PONO)
				printf("\n Ignoring data that falls outside of ROM address range");
			if (pxflags&PONK)
				printf("\n Not verifying checksum in HEX data files");
			if (pxflags&POMP)
				printf("\n Displaying MAP of data being down-loaded");
			}
		else
			printf("\n\n Link: not established");
		}
	if (piflags&PiAI)
		printf("\n AI virtual serial channel to be enabled on exit");
	if (cd&PcROM)
		{
		printf("\n\nEmulation units present:");
		for (i=0; i<pxnrom; i++)
			{
			pr = &pxrom[i];
			if (pr->flags&PRRB)
				ut = "ROMboy ";
			else
				ut = "PromICE";
			printf("\n %s ID%d V%4s Memory=%ld Emulating=%ld FillChar=0x%02X",
				ut,i,pr->ver,pr->size,pr->esize,pr->fdata);
			if (!(pr->flags&PRRB))
				{
				if (pr->mid == i)
					{
					printf(" Master");
					if (pr->res&GOTAI)
						printf("/AI");
					if (pr->res&AIS31)
						printf("-3.1");
					}
				else
					printf(" Slave of %d",pr->mid);
				}
			}
		if (pxbanks)
			printf("\n\n Banking is ENABLED with %ld banks",pxbanks);
		}
	
	if (cd&PcFLE)
		{
		if (!pxnfile)
			printf("\n\nNO FILES ARE SPECIFIED");
		if (piflags&PiFM)
			{
			printf("\n\nFilling of ROM space in effect:");
			if (pxflags&POFR)
				{
				printf("\n Filling ROMs");
				for (i=0; i<pxnrom; i++)
					{
					if (!(pxrom[i].flags&PRFL))
						continue;
					printf("\n  ID = %d Start = 0x%lX End = 0x%lX",
						i,pxrom[i].fstart,pxrom[i].fend);
					printf(" Size = %lX Bytes Filldata = 0x%lX",
						pxrom[i].fsize,pxrom[i].fdata);
					if (pxrom[i].fsize > 4)
						printf(" %lX",pxrom[i].fdata2);
					}
				}
			else
				{
				printf("\n Filling Config");
				printf("\n  Start = 0x%lX End = 0x%lX",pxfstart,pxfend);
				printf(" Size = %lX Bytes Filldata = 0x%lX",pxfsize,pxfdata);
				if (pxfsize > 4)
					printf(" %lX",pxfdata2);
				}
			}
		if (piflags&PiCK)
			{
			printf("\n\nChecksum computation in effect:");
			if (pxflags&POKR)
				{
				printf("\n Checksumming ROMs");
				for (i=0; i<pxnrom; i++)
					{
					if (!(pxrom[i].flags&PRCK))
						continue;
					printf("\n  ID = %d Start = 0x%lX End = 0x%lX",
						i,pxrom[i].kstart,pxrom[i].kend);
					printf(" Stored@ = 0x%lX Size = %d Bytes",
						pxrom[i].kstore,pxrom[i].ksize);
					if (pxrom[i].flags&POKA)
						{
						printf("\n   Checksum computed by addition:");
						if (pxrom[i].flags&POK1)
							printf(" and stored as 1's complement");
						else
							printf(" and stored as 2's complement");
						}
					else
						printf("\n   Checksum computed by exclusive OR");
					}
				}
			else
				{
				printf("\n Checksumming Config:");
				printf("\n  Start = 0x%lX End = 0x%lX",pxkstart,pxkend);
				printf(" Stored@ = 0x%lX Size = %d Bytes",pxkstore,pxksize);
				if (pxflags&POKA)
					{
					printf("\n   Checksum computed by addition:");
					if (pxflags&POK1)
						printf(" and stored as 1's complement");
					else
						printf(" and stored as 2's complement");
					}
				else
					printf("\n   Checksum computed by exclusive OR");
				}
			}
		for (i=0; i<pxnfile; i++)
			{
			pf = &pxfile[i];
			printf("\n\nFILE-%d\tname = \"%s\"",i+1, pf->name);
			printf("\ttype = ");
			switch (pf->type)
				{
				case PFHEX:
					switch (pf->htype)
						{
						case 's':
						case 'S':
							printf("Motorola hex");
							break;
						case ':':
							printf("Intel hex");
							break;
						case ';':
							printf("Tektronics hex");
							break;
						default:
							printf("Hex");
							break;
						}
						break;
					case PFBIN:
						printf("Binary");
						break;
					default:
						printf("Unknown");
						break;
					}							
				printf("\n\tOffset = 0x%lX",pf->offset);
				printf("\tSkip = 0x%lX",pf->skip);
				if (pf->flags&PFPL)
					{
					printf("\n\n\tPartial Loading of File Data:");
					printf("\n\t Start = 0x%lX",pf->saddr - pf->offset);
					printf("\tEnd = 0x%lX",pf->eaddr - pf->offset);
					}
			if (pf->pfcfg)
				pidcfg(pf->pfcfg,"\tData Configuration for down-load:");
			else
				printf("\n\tWill use DEFAULT ROM CONFIGURATION for down-load");
			}
		}
	if (cd&PcPCF)
		{
		if (!pxpcfg)
			printf("NO ROM CONFIGURATION SPECIFIED");
		else
			pidcfg(pxpcfg,"\nDEFAULT ROM CONFIGURATION");
		}
	if (pxdisp&PLOG)
		printf("\n\nLogging Host/PromICE traffic to file: %s",pxlog);
	}

#ifdef ANSI
static void pidcfg(PICONFIG *cx,char *hdr)
#else
static void pidcfg(cx, hdr)
PICONFIG *cx;
char *hdr;
#endif
	{
	short i=0;
	
	printf("\n%s\n\t Word Size = %d",hdr,cx->words*8);
	if (pxbanks)
		printf(" Current Bank = %ld (0x%lX)",pxbank,pxbank);
	printf("\n\t Emulation Space:");
	printf(" TOTAL Address Range = [0x%lX->0x%lX]",(long)0,cx->max);
	while (cx)
		{
		printf("\n\t  IDs =");
		for (i=0; i<cx->words; i++)
			printf(" %d",cx->uid[i]);
		printf(" Address Range = [0x%lX->0x%lX]",cx->start,cx->end);
		cx = cx->next;
		}
	}
