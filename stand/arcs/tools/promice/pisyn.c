/*	PiSyn.c - Edit 6

	LoadICE Version 3
	Copyright (C) 1990-95 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*	 - Promice Syntax Parser
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "piconfig.h"
#include "pistruct.h"
#include "pierror.h"
#include "pisyn.h"

#ifdef ANSI
static void psgets(void);
static void pschar(void);
static void psline(void);
static void psnum(void);
static void psnuml(void);
static void psnums(void);
static void psid(void);
static short psbreak(void);
static long psrdx(void);

#else
static void psgets();
static void pschar();
static void psline();
static void psnum();
static void psnuml();
static void psnums();
static void psid();
static short psbreak();
static long psrdx();
#endif

/*	local 'global' variables -
	- used by the routines in this module
*/
static char *ps_brk = " ,:=[](){}\n\r\t\""; /* break characters */
static char *psi;		/* current input pointer */
static short ps_nun;	/* no input read or found */
static short ps_rad;	/* radix for numeric input */
static short ps_end;	/* end of input seen */
static char ps_syn;	/* current script character */
static char ps_sw;		/* command sperator */
static char ps_string[PIC_FN];	/* place to hold string pointed to by ps_str */

/*
	The parser will search the script table (**pscript) for entry matching
	the command portion of the input string. A match is made if all characters
	match till the '&' is incountered in the script. Then on the input is
	parsed as indicated by the charcters below:

	$	-string
	*	-hex number
	#	-decimal number
	@	-list of hex numbers
	&	-list of decimal numbers
	%	-number embedded in a string (ROM part# etc.)
	:	-unit id (unless reading a string - i.e. filename)
	+	-a single character
	!	-rest of the input line
	(	-call function in (*psynf[x]()) where x=letter follwing '('

	The parser initializes globals to be used by functions in PISYNF.c
*/

#ifdef ANSI
void pisyn(char *is, char **pscript, void (*psynf[])(void))
#else
void pisyn(is, pscript, psynf)
char *is;
char **pscript;
void (*psynf[])();
#endif
	{
	char *sp;
	char **ts;
	short i;
	int ndx;
	char c;
	
	if (pxdisp&PXMH) /* diagnostic output */
		{
		i = strlen(is);
		c = is[i-1];
		if (c == '\n' || c == '\r')
			is[i-1] = '\0';
		printf("\nparsing `%s`",is);
		}

	pi_estr1 = is;			/* error string - when error */
	pi_eloc = (char *)0;	/* possible pointer to error location */
	ps_bchr = '\0';
	ps_bchrc = '\0';
	ps_sw = '\0';
	
	/* while not the end of input or no error */

	while (*is != '\0' && *is != '\n' && *is != '\r' && !pxerror)
		{
		if ((*is == ' ') || (*is == '\t'))
			{
			is++;		/* skip over white spaces in front on input */
			continue;
			}

		/* initialize globals */

		if (piflags&PiCM)
			{
			if (*is == '-' || *is == '/')
				{
				ps_sw = *is;
				is++;
				}
			else
				ps_sw = '\0';
			}
		pi_eloc = is;
		ps_str = (char *)0;
		ps_emb = -1;
		ps_end = 0;
		ps_nun = 0;
		ps_id = -1;
		ps_sli = 0;
		ps_li = 0;
		ps_i = 0;
		ts = pscript;

		/* search till end of table or match */

		while ((sp = *(ts++)) != (char *)0)
			{
			psi = is;
			if (!(piflags&PiCM) || (ps_sw != '\0'))
				while (*psi == *sp)
					{
					psi++;
					sp++;
					}
			if (*sp != '&')		/* if not '&' then no match */
				if (*sp != '~')	/* unless default action wanted */
					continue;
			pi_estr2 = sp;
			ps_bchrc = *psi;

			/* parse further input till '(' encountered or error */

			while ((*(++sp) != '(') && (!pxerror))
				{
				ps_rad = 10;
				ps_syn = *sp;
				switch(*sp)
					{
					case '$':		/* read a string from input */
						psgets();
						break;
					case '+':		/* a single character */
						pschar();
						break;
					case '*':		/* a hex number */
						ps_rad = 16;
						psnum();
						break;
					case '#':		/* a decimal number */
						psnum();
						break;
					case '@':		/* list of hex numbers */
						ps_rad = 16;
						psnuml();
						break;
					case '&':		/* list of decimal numbers */
						psnuml();
						break;
					case '%':		/* embedded number */
						psnums();
						break;
					case ':':		/* possible unit ID */
						psid();
						break;
					case '!':		/* rest of the line */
						psline();
						break;
					case '\0':		/* end of input */
						break;
					default:		/* bad news */
						pxerror = PGE_SYN;
						break;
					}
				}
			if (!pxerror)		/* if all is kool */
				{
				sscanf(++sp,"%d",&ndx);
				ndx -= 1;
				if (ndx < PIC_SCR)
					{
					if (pxdisp & PXMH)	/* more diagnostic output */
						{
						printf("\n");
						if (ps_str != (char *)0)
							printf(" ps_str=`%s`",ps_str);
						if (ps_emb >= 0)
							printf(" ps_emb=%ld",ps_emb);
						if (ps_id >= 0)
							printf(" ps_id=%d",ps_id);
						if (ps_i)
							{
							printf(" Nums(%d):",ps_i);
							for (i=0; i<ps_i; i++)
								printf(" %ld",ps_num[i]);
							}
						if (ps_li)
							{
							printf(" List(%d/%d):",ps_li,ps_sli);
							for (i=0; i<ps_li; i++)
								printf(" %ld",ps_numl[i]);
							}
						}
					(*psynf[ndx])();	/* call funtion (in PISYNF) */
					}
				}
			break;
			}
		if (sp == (char *)0)	/* if end of script table */
			if (!pxerror)		/* and no error then */
				pxerror = PGE_CMD;	/* command not found */
		is = psi;		/* else keep processing input */
		if (piflags&PiiX)
			break;
		}
	}


/* `psgets` - read a string from the input */

static void psgets()
	{
	while (psbreak() & !ps_end)		/* skip break charcters */
		psi++;

	if (ps_end)						/* nothing to read */
		{
		ps_str = (char *)0;
		}
	else
		{
		if (piflags&PiCM && *psi == ps_sw)	/* if out of args (in cmd line) */
			{
			pxerror = PGE_INP;
			}
		else
			{
			ps_str = psi;
			if (ps_bchr == '\"')	/* read everyting in "" */
				while (*psi != '\"')
						psi++;
			else
				while (!psbreak())
					psi++;
			if (*psi != '\0')
				{
				*psi = '\0';
				strcpy(ps_string,ps_str);
				*psi = ' ';
				ps_str = ps_string;
				}
			}
		}
	}

/* `pschar` - read a single character */

static void pschar()
	{
	while (psbreak() & !ps_end)
		psi++;

	if (ps_end)
		ps_char = '\0';
	else
		ps_char = *psi++;
	}

/* `psline` - read a rest of the input line */

static void psline()
	{
	while (psbreak() & !ps_end)
		psi++;

	if (ps_end)
		{
		ps_str = (char *)0;
		}
	else
		ps_str = psi;
	psi += strlen(ps_str);
	}

/* `psnum` - read a number from the input */

static void psnum()
	{
	ps_num[ps_i] = psrdx();
	if (!ps_nun)
		if (++ps_i >= PIC_NUM)
			pxerror = PGE_NUM;
	}

/* `psnuml` - read a list of zero or more numbers from the input */

static void psnuml()
	{
	while (ps_li < PIC_LST)
		{
		if ((ps_bchr == '(') || (ps_bchr == ')'))
			{
			ps_rad = 16;
			ps_sli++;
			}
		ps_numl[ps_li++] = psrdx();
		if (ps_nun)
			{
			ps_li--;
			ps_li -= ps_sli;
			if (ps_li < 0)
				ps_li = 0;
			return;
			}
		}
	pxerror = PGE_LST;
	}

/* `psnums` - read an embedded number from the input */

static void psnums()
	{
	char c;

	while (psbreak() & !ps_end)
		psi++;
	if ((piflags&PiCM && (*psi == ps_sw)) || ps_end) /* we hit the next arg */
		return;
	while (!psbreak())
		{
		c = *(psi++);
		if (c < '0' || c > '9')
			continue;
		if (ps_emb == -1)
			ps_emb = 0;
		ps_emb = ps_emb * 10 + (c - '0');
		}
	}

/* `psid` - read a unit ID from the input */

static void psid()
	{
	char *tp;
	
	tp = psi;
	
	ps_id = (short)psrdx();
	
	if (ps_nun || ps_end || ps_bchr != ':')
		{
		ps_nun = 0;
		ps_end = 0;
		ps_id = -1;
		psi = tp;
		}
	else
		{
		if (ps_id >= PIC_NR)
			{
			pxerror = PGE_BIG;
			ps_id = -1;
			}
		}
	}

/* `psrdx` - read string as hex */

static long psrdx()
	{
	char c;
	char *tis;
	long value = 0;
	short neg = 0;
	
	while (psbreak() && !ps_end)
		psi++;
	if (ps_bchr == '(')
		{
		ps_rad = 16;
		ps_sli++;
		}
	if ((ps_end)  || (piflags&PiCM && (*psi == ps_sw)))
		{
		ps_nun = 1;
		return(0);
		}

	tis = psi;
	while (!psbreak())
		{
		c = *(psi++);
		if (c == 'x' || c == 'X')
			{
			ps_rad = 16;
			continue;
			}
		if (c == '-')
			{
			neg = 1;
			continue;
			}
		if (ps_rad == 16)
			{
			if (c >= '0' && c <= '9')
				{	
				value = value * 16 + (long)(c - '0');
				continue;
				}
			if (c >= 'A' && c <= 'F')
				{
				value = value * 16 + (long)(c - 'A' + 10);
				continue;
				}
			if (c >= 'a' && c <= 'f')
				{
				value = value * 16 + (long)(c - 'a' + 10);
				continue;
				}
			else
				{
				ps_nun = 1;
				break;
				}
			}
		else
			{
			if (c >= '0' && c <= '9')
				{
				value = value * 10 + (long)(c - '0');
				continue;
				}
			else
				{
				ps_nun = 1;
				break;
				}
			}
		}
	if (ps_nun)
		psi = tis;

	if (neg)
		return(-value);

	return(value);
	}

/* `psbreak` - returns non-zero if break char */

static short psbreak()
	{
	char *bs = ps_brk;

	if (*psi == '\0' || *psi == '\n' || *psi == '\r')
		{
		ps_bchr = *psi = '\0';
		ps_end = 1;
		return(1);
		}

	while (*bs != '\0')
		{
		if (*bs == *psi)
			{
			if ((ps_syn != '$') || (*bs != ':'))
				{
				ps_bchr = *bs;
				return(1);
				}
			}
		bs++;
		}
	return(0);
	}
