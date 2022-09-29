/*	PiFile.c - Edit 6

	LoadICE Version 3
	Copyright (C) 1990-95 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*	 - Process files - read BigBuf and do translation
*/

#include <stdio.h>
#include <fcntl.h>
#include <string.h>

#ifdef MSDOS
#include <io.h>
#endif

#include "piconfig.h"
#include "pistruct.h"
#include "pierror.h"
#include "pifile.h"
#include "pidata.h"

#ifdef MACAPPL
#include "flTuple.h"
#include "includes.h"
#include "LP.h"
#endif

#ifdef ANSI
extern void piconfig(short code);
extern void piread(void);
extern void piwrite(void);
extern void pifixcfg(void);
extern void pimap(unsigned long here, unsigned long there);
void pixlate(void);
void pixbfmt(void);
static void pi_hxint(void);
static void pi_hxmot(void);
static void pi_hxtek(void);
static void pi_hxetk(void);
static void pi_hxdsp(void);
static void pi_hxmos(void);
static void pi_hxrca(void);
static void pi_pixfr(void);
static short pi_csmot(char *cp, short ln);
static short pi_csint(char *cp, short ln);
static short pi_csmos(char *cp, short ln);
static short pi_cstek(char *cp, short ln);
static short pi_csetk(char *cp, short ln);
static long pi_hxpget(char *in, short n);
static long pi_hxget(short n);
static void pi_hxput(long loc, short ct);

#else
extern void piconfig();
extern void piread();
extern void piwrite();
extern void pifixcfg();
extern void pimap();
void pixlate();
void pixbfmt();
static void pi_hxint();
static void pi_hxmot();
static void pi_hxtek();
static void pi_hxetk();
static void pi_hxdsp();
static void pi_hxmos();
static void pi_hxrca();
static void pi_pixfr();
static short pi_csmot();
static short pi_csint();
static short pi_csmos();
static short pi_cstek();
static short pi_csetk();
static long pi_hxpget();
static long pi_hxget();
static void pi_hxput();
#endif

static char *putptr,*getptr,*tmptr;
static long rqct,rca,hxbase,romloc;
static PIFILE *fp;
static char *piestrs = (char *)0;
static short pieof = 0;
static unsigned char firstt = 0;
static unsigned long here, there;
void pifile()						/* just process current file 'pxcfile' */
	{
	int ref;
	short i;

#ifdef MACAPPL
	WDPBPtr		pbp;
	fileLTPtr   flPtr;
	long		hold;
#endif
	
	fp = &pxfile[pxcfile];
	if (!(pxcfg = fp->pfcfg))
		pxcfg = pxpcfg;
	pifixcfg();
	pxxloc = fp->offset;
	rqct = PIC_BS;
	
	if (pxdisp&(PXHI|PXMI))
		{
		printf("\nOpening file `%s` for processing",fp->name);
		if (pxdisp&PXMI)
			piconfig(PcFLE);
		}
#ifdef MACAPPL
	
	pbp = (WDPBPtr)NewPtrClear(sizeof(WDPBRec));
	if(MemError())
		pxerror = PGE_OPN;
	flPtr = (fileLTPtr)up.file[pxcfile].fName;
	GetVInfo(0,flPtr->vName,&(pbp->ioVRefNum),&hold);
	
	/* set the default directory */
	pbp->ioWDDirID = flPtr->dirID;
	pbp->ioWDProcID = kPROGCREAT;
	ref = PBOpenWD(pbp,false);
	if (!ref)
		ref = SetVol("",pbp->ioVRefNum);
	if (!ref)
#endif
	if (fp->type & (PFBIN|PFBN2))
		{
#ifdef	MSDOS
		if ((ref = open(fp->name,O_RDONLY|O_BINARY)) < 0)
#else
		if ((ref = open(fp->name,O_RDONLY)) < 0)
#endif
			{
			pxerror = PGE_OPN;
			return;
			}
		}
	else
		{
		if ((ref = open(fp->name,O_RDONLY)) < 0)
			{
			pxerror = PGE_OPN;
			return;
			}
		}
#ifdef MACAPPL	
	DisposPtr((Ptr)pbp);
#endif

	switch (fp->type)
		{
		case PFBIN:
			here = fp->saddr;
			if (fp->skip)	/* if to skip binary file data */
				{
				if (lseek(ref,fp->skip,0) < 0)
					{
					pxerror = PGE_SKP;
					return;
					}
				}	
			while (((pxxbc = read(ref,pxxbf,(unsigned int)rqct)) > 0) && !pieof)
				{
				if (fp->flags&PFPL) /* partial load */
					{
					if (((pxxloc < fp->saddr) && (pxxloc+pxxbc < fp->saddr))
							|| (pxxloc > fp->eaddr))
						{
						pxxloc += pxxbc;
						continue;
						}
					if (pxxloc+pxxbc > fp->eaddr)
						pxxbc =(short)(fp->eaddr - pxxloc + 1);
					if (pxxloc < fp->saddr)
						{
						pxxbc -= fp->saddr - pxxloc;
						for (i=0; i<pxxbc; i++)
							pxxbf[i] = pxxbf[i + fp->saddr - pxxloc];
						pxxloc = fp->saddr;
						}
					there = fp->eaddr;
					}
				else
					there = pxxloc+pxxbc-1;
				pi_pixfr();
				if (pxerror)
					break;
				pxxloc += pxxbc;
				}
			pimap(here,there);
			if (pxxbc<0)
				pxerror = PGE_IOE;
			if (pxdisp&PXHI)
				printf("/Done");
			break;
		default:				/* hex file */
			getptr = pxbbf;
			rca = hxbase = pieof = 0;
			firstt = 1;
			while ((pxbbc = read(ref,getptr,(unsigned int)rqct)) > 0)
				{
				if (pxdisp&PXMI)	/* diagnostic output */
					{
					printf("\n\nBigBuf-readCount=%ld bufSize=%ld data='",
						pxbbc,pxbbc+PIC_BS-rqct);
					for (i=0; i<15; i++)
						if (fp->type == PFBN2)
							printf("%02X",pxbbf[i]&0xff);
						else
							printf("%c",pxbbf[i]);
					printf("...");
					}
				pxbbc += PIC_BS - rqct;
				if (fp->type == PFBN2)
					pixbfmt();
				else
					pixlate();	/* translate big hex buffer to binary */
				if (pxerror)
					break;
				}
			if (pxxbc)
				pi_pixfr();		/* transfer any left over data */
			there = (unsigned long)(pxxloc+pxxbc-1);
			pimap(here,there);	/* display last data range */
			if (pxerror)
				pierror();
			if (pxdisp&PXHI)
				printf("/Done");
			break;
		}
	close(ref);
	}

/* `pixlate` - translate a hex buffer */

void pixlate()
	{
	long i,tbc;
	short eol;
	char c;
	
	tmptr = pxbbf;
	while (pxbbc && !pxerror && !pieof)	/* while data, no error and no EOF */
		{
		getptr = tmptr;
		tbc = pxbbc;
		eol = 0;
		for (i=0; i<tbc; i++,tmptr++,pxbbc--) /* find one record */
			{
			c = *tmptr;
			if (c == '\n' || c == '\r')
				{
				eol++;
				*tmptr++ = '\0';
				pxbbc--,i++;
				c = *tmptr;
				if (!pxbbc)
					break;
				if (c == '\n' || c == '\r')
					tmptr++,pxbbc--,i++;
				break;
				}
			}
		if (!eol)	/* no end-o-line in buffer , read more */
			{
			if (pxdisp&PXML)
				printf("\nCopyIt(ct=%ld)-'%s'",tbc,getptr);
			tmptr = pxbbf;
			for (i=0; i<tbc; i++)
				*tmptr++ = *getptr++;
			rqct = PIC_BS - tbc;
			getptr = tmptr;
			return;
			}
		else		/* there is at least a record in the buffer */
			{
			if (i<4)
				{
				continue;
				}
			if (pxdisp & PXML)
				printf("\nHexRec'%s'",getptr);
			pi_estr1 = getptr;
			switch (*getptr++)
				{
				case ':':		/* intel hex */
					pi_hxint();
					break;
				case 's':		/* motorola hex */
				case 'S':
					pi_hxmot();
					break;
				case '$':		/* motorola hex comment record */
					break;
				case '/':		/* standard tektronix hex */
					pi_hxtek();
					break;
				case '%':		/* extended tektronics hex */
					pi_hxetk();
					break;
				case '_':		/* funky DSP format */
					pi_hxdsp();
					break;
				case ';':		/* defunct mostek format */
					pi_hxmos();
					break;
				case '?':		/* possible archaic rca format */
				case '!':
					rca = 1;
					break;
				default:		/* can only be rca or bogus */
					if (rca)
						pi_hxrca();
					else
						{
						pxerror = PGE_BAD;
						}
					break;
				}
			}
		piestrs = pi_estr1;
		}
	if (!pxbbc)		/* more data in buffer? look for more records */
		{
		getptr = pxbbf;
		rqct = PIC_BS;
		}
	}

/* `pi_hxint` - process intel hex records - supports 8, 16 and 32 bit formats */

static void pi_hxint()
	{
	long loc;
	short ct;

	ct = (short)pi_hxget(2);
	loc = pi_hxget(4);
	
	if (!(pxflags&PONK))	/* check record checksum */
		{
		if (pi_csint(pi_estr1+1,ct*2+8) != (short)pi_hxpget(pi_estr1+9+ct*2,2))
			{
			pxerror = PGE_CHK;
			return;
			}
		}
	switch((short)pi_hxget(2))
		{
		case 0:				/* data record */
			loc += hxbase;
			break;
		case 1:				/* end-o-file - reset base */
			ct = 0;
			hxbase = 0;
			break;
		case 2:				/* segment record  (for 20-bit address) */
			hxbase = pi_hxget(4) << 4;
			ct = 0;
			break;
		case 4:				/* BIG segment record (for 32-bit address) */
			hxbase = pi_hxget(4) << 16;
			ct = 0;
			break;
		default:			/* bogus record */
			ct = 0;
			break;
		}
	if (ct)					/* if data record */
		pi_hxput(loc, ct);
	}

/* `pi_hxmot` - process motorola hex records */

static void pi_hxmot()
	{
	long loc;
	short len, ct;
	
	if (!(pxflags&PONK))		/* check checksum */
		{
		ct = (short)pi_hxpget(pi_estr1+2,2);
		if (pi_csmot(pi_estr1+2,(ct+1)*2) != 0xff)
			{
			pxerror = PGE_CHK;
			return;
			}
		}
	switch (*getptr++)
		{
		case '1':		/* 16-bit address + data */
			len = 4;
			break;
		case '2':		/* 24-bit address + data */
			len = 6;
			break;
		case '3':		/* 32-bit address + data */
			len = 8;
			break;
		default:		/* don't care */
			len = 0;
			break;
		}
	if (len)			/* if data */
		{
		ct = (short)pi_hxget(2) - len/2 - 1;
		loc = pi_hxget(len);
		pi_hxput(loc,ct);
		}
	}

/* `pi_hxtek` - process tektronics hex records */

static void pi_hxtek()

	{
	long loc;
	short ct;
	
	if (*getptr != '/')
		{
		loc = pi_hxget(4);
		ct = (short)pi_hxget(2);
		if (ct)
			{
			if (!(pxflags&PONK))
				{
				if ((pi_cstek(pi_estr1+1,6) != (short)pi_hxpget(pi_estr1+7,2))
					|| (pi_cstek(pi_estr1+9,ct*2) 
							!= (short)pi_hxpget(pi_estr1+9+ct*2,2)))
					{
					pxerror = PGE_CHK;
					return;
					}
				}
			getptr += 2; /*skip checksum*/
			pi_hxput(loc,ct);
			}
		}
	}
/* `pi_hxetk` - process extended tektronics hex records */

static void pi_hxetk()
	{
	long loc;
	short len, len1, len2, ct;

	loc=len1=0; /* keep compiler happy */
	ct = (short)pi_hxget(2);
	if (!(pxflags&PONK))
		{
		if (pi_csetk(pi_estr1,ct) != (short)pi_hxpget(pi_estr1+4,2))
			{
			pxerror = PGE_CHK;
			return;
			}
		}
	switch(*getptr++)
		{
		case '6':
			getptr += 2; /* skip checksum */
			len = (short)(*getptr++ - '0');
			if (!len)
				len = 16;
			loc = hxbase + pi_hxget(len);
			ct -= 6 + len;
			break;
		case '3':
			getptr += 2; /* skip checksum */
			len = (short)pi_hxget(1);
			if (!len)
				len = 16;
			len2 = len+7;
			while (len2 < ct)
				{
				len = (short)pi_hxget(1);
				len1 = (short)pi_hxget(1);
				if (!len)
					break;
				len2 += len1;
				}
			if (!len)
				hxbase = pi_hxget(len1);
			ct = 0;
			break;
		case '8':
			ct = 0;
			hxbase = 0;
			pieof = 1;
			break;
		default:
			ct = 0;
			break;
		}
	if (ct)
		{
		ct /= 2;
		pi_hxput(loc,ct);
		}
	}

/* `pi_hxdsp` - process dsp hex records */

static void pi_hxdsp()
	{
	pxerror = PGE_NYI;
	}

/* `pi_hxmos` - process mostek hex records */

static void pi_hxmos()
	{
	long loc;
	short ct;
	
	ct = (short)pi_hxget(2);
	if (!(pxflags&PONK))
		{
		if (pi_csmos(pi_estr1+1,ct*2+6) != (short)pi_hxpget(pi_estr1+ct*2+7,4))
			{
			pxerror = PGE_CHK;
			return;
			}
		}
	loc = pi_hxget(4);
	if (ct)
		pi_hxput(loc,ct);
	}

/* `pi_hxrca` - process rca hex records */

static void pi_hxrca()
	{
	long loc;
	short ct;
	
	if (rca == 1)
		loc = pi_hxget(4);
	else
		loc = hxbase;
	ct = (short)strlen(getptr);
	if (*getptr == ' ')
		{
		getptr++;
		ct--;
		}
	switch(*(getptr+ct))
		{
		case ',':
			rca = 2;
			ct--;
			break;
		case ';':
			rca = 1;
			ct--;
			break;
		}
	hxbase = loc+(ct/2);
	if (ct)
		pi_hxput(loc,ct/2);
	}

/* 'pi_csmot' - compute checksum on a motorola hex record */

#ifdef ANSI
static short pi_csmot(char *cp, short ln)
#else
static short pi_csmot(cp, ln)
char *cp;
short ln;
#endif
	{
	short i;
	unsigned long cs;

	cs = 0;
	for (i=0; i<ln; i+=2, cp++)
		cs += pi_hxpget(cp++,2);
	return((short)cs & 0xff);
	}

/* 'pi_csint' - compute checksum on an intel hex record */

#ifdef ANSI
static short pi_csint(char *cp, short ln)
#else
static short pi_csint(cp, ln)
char *cp;
short ln;
#endif
	{
	short i;
	unsigned long cs;

	cs = 0;
	for (i=0; i<ln; i+=2, cp++)
		cs += pi_hxpget(cp++,2);
	return((short)((~cs + 1) & 0xFF));
	}

/* 'pi_csmos' - compute checksum on a mostek hex record */

#ifdef ANSI
static short pi_csmos(char *cp, short ln)
#else
static short pi_csmos(cp, ln)
char *cp;
short ln;
#endif
	{
	short i;
	unsigned long cs;

	cs = 0;
	for (i=0; i<ln; i+=2, cp++)
		cs += pi_hxpget(cp++,2);
	return((short)(cs));
	}

/* 'pi_cstek' - compute checksum on standard Tekhex record */

#ifdef ANSI
static short pi_cstek(char *cp, short ln)
#else
static short pi_cstek(cp, ln)
char *cp;
short ln;
#endif
	{
	short i;
	unsigned long cs;

	cs = 0;
	for (i=0; i<ln; i++, cp++)
		cs += pi_hxpget(cp,1);
	return((short)cs & 0xff);
	}

/* `pi_csetk` - checksum Extended Tek hex record */

#ifdef ANSI
static short pi_csetk(char *bf, short ln)
#else
static short pi_csetk(bf, ln)
char *bf;
short ln;
#endif
	{
	short cs,i;

	cs = 0;
	for (i=1; i<=3; i++)
		cs += PIET[bf[i]];
	for (i=6; i<=ln; i++)
		cs += PIET[bf[i]];
	return (cs & 0xff);
	}

/* `pi_hxpget` - get hex data worth `n` chars from given pointer */

#ifdef ANSI
static long pi_hxpget(char *in, short n)
#else
static long pi_hxpget(in, n)
char *in;
short n;
#endif
	{
	long v;
	short i;

	for (i=0,v=0; i<n; i+=2)
		{
		v <<= 8;
		v += (PIXT2[PIXT1[in[i]]][in[i+1]])&0xff;
		}
	return(v);
	}

/* `pi_hxget` - get hex data worth `n` chars from hex buffer */

#ifdef ANSI
static long pi_hxget(short n)
#else
static long pi_hxget(n)
short n;
#endif
	{
	long v;
	short i;
	char *in = getptr;

	for (i=0,v=0; i<n; i+=2)
		{
		v <<= 8;
		v += (PIXT2[PIXT1[in[i]]][in[i+1]])&0xff;
		}
	getptr += n;
	return(v);
	}

/* `pi_hxput` - do something with the hex data */

#ifdef ANSI
static void pi_hxput(long loc, short ct)
#else
static void pi_hxput(loc, ct)
long loc;
short ct;
#endif
	{
	short i;
	
	if (pxdisp&PXML)
		printf("\nRecLoc=%06lX ROMLoc=%06lX DataCount=%d",loc,loc+fp->offset,ct);

	if (fp->flags&PFPL) /* partial load */
		{
		if (((loc < fp->saddr) && (loc+ct < fp->saddr)) || (loc > fp->eaddr))
			{
			if (pxdisp&PXML)
				printf("\nPartial-load - skipping above record");
			return;
			}
		if (loc+ct > fp->eaddr)
			ct =(short)(fp->eaddr - loc + 1);
		if (loc < fp->saddr)
			{
			getptr += (fp->saddr - loc) * 2;
			ct -= fp->saddr - loc;
			loc = fp->saddr;
			}
		}
	loc += fp->offset;	/* compute where this data really goes */
	if (((loc<0) && ((loc+ct)<0)) || (loc>pxmax))	/* can go here? */
		{
		pxerror = PGE_AOR;
		}
	else	/* yes can go */
		{
		if (loc<0)	/* pointer is before this config, ignore some data */
			{
			getptr += (-loc*2);
			ct += loc;
			loc = 0;
			}
		if (firstt || ((loc+ct-pxxloc)>PIC_BS) || (loc!=(pxxloc+pxxbc)))
			{
			if (firstt)
				{
				firstt = 0;
				putptr = pxxbf;
				here = (unsigned long)loc;
				}
			else
				{
				if (loc!=(pxxloc+pxxbc))	/* keep load-map data current */
					{
					there = (unsigned long)pxxloc+pxxbc-1;
					pimap(here,there);
					here = (unsigned long)loc;
					}
				pi_pixfr();	/* transfer data to PROMICEs */
				}
			pxxbc = 0;
			pxxloc = loc;
			}
		for (i=0; i<ct; i++,getptr+=2)	/* translate hex to binary */
			{
			*putptr++ = PIXT2[PIXT1[*getptr]][*(getptr+1)];
			}
		pxxbc += ct;
		}
	if (pxerror)	/* if error, see what to do */
		{
		if (pxerror == PGE_AOR)	/* if address out of range */
			{
			if ((pxdisp&PXML) || (pxflags&PODO))
				{
				printf("\nAddressOutOfRange - '%s'",pi_estr1);
				printf("\n RecLoc=%06lX ROMLoc=%06lX DataCount=%d",
					loc-fp->offset,loc,ct);
				}
			if (pxflags&PONO)
				pxerror = PGE_NOE;
			else
				{
				pi_enum1 = loc;
				pi_enum2 = loc-fp->offset;
				pi_estr2 = fp->name;
				pi_eflags |= PIE_NUM;
				}
			}
		}
	}

/* `pi_pixfr` - either write or read and compare file data */

static void pi_pixfr()
	{
	long i,j;
	char c;

	if (pxflags&POCP) /* if compare */
		{
		pxybc = pxxbc;
		pxyloc = pxxloc;
		piread();
		if (pxerror)
			if (pxerror != PGE_AOR)
				return;
		for (i=0,j=0; i<pxybc; i++)
			{
			if (pxxbf[i] != pxybf[i])
				{
				printf("\ncompare failed - %06lX: f/%02X r/%02X",
					pxyloc+i,pxxbf[i]&0xff,pxybf[i]&0xff);
				if (!((++j)%24))
					{
					printf(" More...");
					c = fgetc(stdin);
					if ((c != 'y') && (c != '\r') && (c != '\n'))
						{
						pxerror = PGE_USR;
						return;
						}
					else
						j = 0;
					}
				}
			}
		putptr = pxxbf;
		}
	else	/* else load */
		{
		piwrite();
		putptr = pxxbf;
		}
	if (pxerror == PGE_AOR)
		pi_estr1 = piestrs;
	}

/* `pixbfmt` - process formatted binary file 'aaaaaaccdddddd......' */

void pixbfmt()
	{
	pxerror = PGE_NYI;
	}
