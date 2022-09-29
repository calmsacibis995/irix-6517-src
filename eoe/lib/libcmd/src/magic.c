/*
 * magic.c
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.9 $"


#include <stdio.h>
#include <pfmt.h>
#include <string.h>
#include <errno.h>
#include <malloc.h>

/*
**	Types
*/

#define	BYTE	0
#define	SHORT	2
#define	LONG	4
#define	STR	8
#ifdef sgi
#define	ADDR	10
#endif /* sgi */

/*
**	Opcodes
*/

#define	EQ	0
#define	GT	1
#define	LT	2
#define	STRC	3	/* string compare */
#define	ANY	4
#define AND	5
#define NSET	6	/* True if bit is not set */
#ifdef sgi
#define	NE	7
#endif /* sgi */
#define	SUB	64	/* or'ed in, SUBstitution string, for example %ld, %s, %lo */
			/* mask: with bit 6 on, used to locate print formats */
#ifdef sgi
#define	SUB_STR	128	/* or'ed in, only for %s */
#endif /* sgi */
/*
**	Misc
*/

#define	BSZ	128
#ifdef sgi
#define	NENT	512
#else
#define	NENT	200 
#endif /* sgi */
#define	reg	register

/*
**	Structure of magic file entry
*/

struct	entry	{
	char	e_level;	/* 0 or 1 */
	long	e_off;		/* in bytes */
#ifdef sgi
	char	e_type;		/* BYTE, SHORT, LONG, ADDR, STR */
	char	e_opcode;	/* EQ, GT, LT, ANY, AND, NSET, NE */
#else
	char	e_type;		/* BYTE, SHORT, LONG, STR */
	char	e_opcode;	/* EQ, GT, LT, ANY, AND, NSET */
#endif /* sgi */
	union	{
		long	num;
		char	*str;
	}	e_value;
	char	*e_msgid;
	char	*e_str;
};

typedef	struct entry	Entry;
static Entry	*mtab;

static long	atolo(char *);

static const char
	fmterror[] = "uxlibc:82:Fmt error, no tab after %son line %d\n",
	nomem[] = "uxlibc:83:Not enough memory for magic table: %s\n";

static char *
get_str(Entry *ep)
{
#ifdef sgi
	char	*p = ep->e_str;

	p = (*p == '\b' ? p+1 : p);
	if (ep->e_msgid)
		return gettxt(ep->e_msgid, p);
	else
		return p;
#else
	if (ep->e_msgid)
		return gettxt(ep->e_msgid, ep->e_str);
	else
		return ep->e_str;
#endif /* sgi */
}
		
int
mkmtab(char *magfile, reg int cflg)
{
	reg	Entry	*ep;
	reg	FILE	*fp;
	reg	int	lcnt = 0;
	auto	char	buf[BSZ];
	auto	Entry	*mend;

	ep = (Entry *) calloc(sizeof(Entry), NENT);
	if(ep == NULL) {
		pfmt(stderr, MM_ERROR, nomem, strerror(errno));
		return(-1);
	}
	mtab = ep;
	mend = &mtab[NENT];
	fp = fopen(magfile, "r");
	if(fp == NULL) {
		pfmt(stderr, MM_ERROR, "uxlibc:85:Cannot open magic file <%s>: %s\n",
			magfile, strerror(errno));
		return(-1);
	}
	while(fgets(buf, BSZ, fp) != NULL) {
		reg	char	*p = buf;
		reg	char	*p2, *p3;
		reg	char	opc;

#ifdef sgi
		lcnt++;
#endif /* sgi */
		if(*p == '\n' || *p == '#')
			continue;
#ifndef sgi
		lcnt++;
#endif /* !sgi */
			

			/* LEVEL */
		if(*p == '>') {
			ep->e_level = 1;
			p++;
		}
			/* OFFSET */
		p2 = strchr(p, '\t');
		if(p2 == NULL) {
			if(cflg)
				pfmt(stderr, MM_ERROR, fmterror, p, lcnt);
			continue;
		}
		*p2++ = NULL;
		ep->e_off = atolo(p);
		while(*p2 == '\t')
			p2++;
			/* TYPE */
		p = p2;
		p2 = strchr(p, '\t');
		if(p2 == NULL) {
			if(cflg)
				pfmt(stderr, MM_ERROR, fmterror, p, lcnt);
			continue;
		}
		*p2++ = NULL;
		if(*p == 's') {
			if(*(p+1) == 'h')
				ep->e_type = SHORT;
			else
				ep->e_type = STR;
		} else if (*p == 'l')
			ep->e_type = LONG;
#ifdef sgi
		else if (*p == 'a')
		   	ep->e_type = ADDR;
#endif /* sgi */
		while(*p2 == '\t')
			p2++;
			/* OP-VALUE */
		p = p2;
		p2 = strchr(p, '\t');
		if(p2 == NULL) {
			if(cflg)
				pfmt(stderr, MM_ERROR, fmterror, p, lcnt);
			continue;
		}
		*p2++ = NULL;
		if(ep->e_type != STR) {
			opc = *p++;
			switch(opc) {
			case '=':
				ep->e_opcode = EQ;
				break;

			case '>':
				ep->e_opcode = GT;
				break;

			case '<':
				ep->e_opcode = LT;
				break;

			case 'x':
				ep->e_opcode = ANY;
				break;

			case '&':
				ep->e_opcode = AND;
				break;

			case '^':
				ep->e_opcode = NSET;
				break;
#ifdef sgi
			case '!':
				ep->e_opcode = NE;
				break;
#endif /* sgi */
			default:		/* EQ (i.e. 0) is default	*/
				p--;		/* since global ep->e_opcode=0	*/
			}
		}
		if(ep->e_opcode != ANY) {
			if(ep->e_type != STR)
				ep->e_value.num = atolo(p);
			else if ((ep->e_value.str = strdup(p)) == NULL) {
				pfmt(stderr, MM_ERROR, nomem, strerror(errno));
			 	return(-1);
			}
		}
		p2 += strspn(p2, "\t");
			/* STRING or MSGID */
		if ((p3 = strchr(p2, '\t')) != NULL){
			/* MSGID */
			*p3++ = '\0';
			p3 += strspn(p3, "\t");
			if ((ep->e_msgid = strdup(p2)) == NULL){
				pfmt(stderr, MM_ERROR, nomem, strerror(errno));
				return -1;
			}
			p2 = p3;
		}
			/* STRING */
		if ((ep->e_str = strdup(p2)) == NULL) {
			pfmt(stderr, MM_ERROR, nomem, strerror(errno));
			return(-1);
		}
		else {
			if ((p = strchr(ep->e_str, '\n')) != NULL)
				*p = '\0';
#ifdef sgi
			if ((p = strchr(ep->e_str, '%')) != NULL) {
				ep->e_opcode |= SUB;
				if (p[1] == 's')
					ep->e_opcode |= SUB_STR;
			}
#else
			if (strchr(ep->e_str, '%') != NULL)
				ep->e_opcode |= SUB;
#endif /* sgi */
		}
		ep++;
		if(ep >= mend) {
			unsigned int tbsize, oldsize;
			
			oldsize = (int)(mend - mtab);  /* off by one? */
			tbsize = (NENT + oldsize) * (int)sizeof(Entry);
			if ((mtab = (Entry *) realloc((char *) mtab, tbsize))
				== NULL) {
				pfmt(stderr, MM_ERROR, nomem, strerror(errno));
				return(-1);
			}
			else {
				memset(mtab + oldsize, 0, sizeof(Entry) * NENT);
				mend = &mtab[tbsize / sizeof(Entry)];
				ep = &mtab[oldsize];  
			}
		}
	}
	ep->e_off = -1L;
	if (fclose(fp) == EOF) {
		pfmt(stderr, MM_ERROR, "uxlibc:86:Cannot close %s: %s\n",
			magfile, strerror(errno));
		return(-1);
	}
	mtab = (Entry *)realloc((char *)mtab, (1 + ep - mtab) * sizeof(Entry));
	if (mtab == NULL) {
		pfmt(stderr, MM_ERROR, nomem, strerror(errno));
		return(-1);
	}
	return(0);
}

static long
atolo(reg char *s)		/* determine read format and get e_value.num */
{
	reg	char	*fmt = "%ld";
	auto	long	j = 0L;

	if(*s == '0') {
		s++;
		if(*s == 'x') {
			s++;
			fmt = "%lx";
		} else
			fmt = "%lo";
	}
	sscanf(s, fmt, &j);
	return(j);
}

int
ckmtab(char *buf, int bufsize, int silent)
			/* Check for Magic Table entries in the file */
{

	unsigned short svar;
	reg	Entry	*ep;
	reg	char	*p;
	reg	int	lev1 = 0;
#ifdef u3b2
	auto	union	{
		long	l;
		char	ch[4];
		}	val, revval;
	static	char	tmpbyte;
#else
	auto	union	{
		long	l;
		char	ch[4];
		}	val;
#endif /* u3b2 */

	for(ep = mtab; ep->e_off != -1L; ep++) {  /* -1 offset marks end of */
		if(lev1) {			/* valid magic file entries */
			if(ep->e_level != 1)
				break;
		} else if(ep->e_level == 1)
			continue;
		if (ep->e_off > (long) bufsize)
			continue;
		p = &buf[ep->e_off];
		switch(ep->e_type) {
		case STR:
		{
			if(strncmp(p,ep->e_value.str,strlen(ep->e_value.str)))
				continue;
			if (!silent) {
				if(ep->e_opcode & SUB)
					printf(get_str(ep), ep->e_value.str);
				else
					printf(get_str(ep));
			}
			lev1 = 1;
		}

		case BYTE:
			val.l = (long)(*(unsigned char *) p);
			break;

		case SHORT:
			/* 
			 * This code is modified to avoid word alignment problem 
			 * which caused command "more" to core dump on a 3b2 machine 
			 * when the  word pointed to by p is not aleast halfword 
			 * aligned.
			 */
			memcpy(&svar, p, sizeof(unsigned short));
			val.l = svar;
			break;

		case LONG:
			/* This code is modified to avoid word alignment problem */
			memcpy(&val.l, p, sizeof(long));
			break;
#ifdef sgi
		case ADDR:
			val.l = (long) p;
			break;
#endif /* sgi */
		}
#ifndef sgi
		switch(ep->e_opcode & ~SUB) { /*}*/
#else
		switch(ep->e_opcode & ~(SUB|SUB_STR)) {
#endif /* sgi */
		case EQ:
#ifdef u3b2
			if(val.l != ep->e_value.num)
				if(ep->e_type == SHORT) {
					/* reverse bytes */
					revval.l = 0L;
					tmpbyte = val.ch[3];
					revval.ch[3] = val.ch[2];
					revval.ch[2] = tmpbyte;
					if(revval.l != ep->e_value.num)
						continue;
					else
						break;
				}
				else	continue;
			else
				break;
#else
			if(val.l != ep->e_value.num)
				continue;
			break;
#endif
		case GT:
			if(val.l <= ep->e_value.num)
				continue;
			break;

		case LT:
			if(val.l >= ep->e_value.num)
				continue;
			break;

		case AND:
			if((val.l & ep->e_value.num) == ep->e_value.num)
				break;
			continue;
		case NSET:
			if((val.l & ep->e_value.num) != ep->e_value.num)
				break;
			continue;
#ifdef sgi
		case NE:
			if(val.l == ep->e_value.num)
				continue;
			break;
#endif /* sgi */
		}
#ifdef sgi
		if(lev1 && !silent)
			if (*(ep->e_str) != '\b')
				putchar(' ');
#else
		if(lev1 && !silent)
			putchar(' ');
#endif /* sgi */
		if (!silent) {
#ifdef sgi
			if(ep->e_opcode & SUB_STR) {

				/* modified to print up to (not including) \n or \0,
				 * whichever comes first, rather than whatever the
				 * buffer size happens to be; this is useful for
				 * using %s formats on text files.  done only if %s;
				 * this case is here so that you can uses %s with "addr"
				 * types and a match of 'x' to match/print arbitrary
				 * strings */

				char *nl = strchr((char *)val.l, '\n');
				if(nl)
					*nl = '\0';
				printf(get_str(ep), val.l);
			}else
#endif /* sgi */
			if(ep->e_opcode & SUB)
				printf(get_str(ep), val.l);
			else
				printf(get_str(ep));
		}
		lev1 = 1;
	}
	if(lev1) {
		if (!silent)
			putchar('\n');
		return(1 + (int)(ep - mtab));
	}
	return(0);
}

void
prtmtab(void)
{
	reg	Entry	*ep;

	pfmt(stdout, MM_NOSTD, "uxlibc:87:level	off	type	opcode	value	string\n");
	for(ep = mtab; ep->e_off != -1L; ep++) {
		printf("%d\t%d\t%d\t%d\t", ep->e_level, ep->e_off,
			ep->e_type, ep->e_opcode);
		if(ep->e_type == STR)
			printf("%s\t", ep->e_value.str);
		else
			printf("%lo\t", ep->e_value.num);
		printf("%s", get_str(ep));
		if(ep->e_opcode & SUB)
			printf("\tsubst");
		printf("\n");
	}
}
