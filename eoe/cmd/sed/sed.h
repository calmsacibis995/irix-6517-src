/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)sed:sed.h	1.8"	*/
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sed/RCS/sed.h,v 1.11 1996/03/18 23:06:05 tung Exp $"
/*
 * sed -- stream  editor
 */

#include <regex.h>

#define NLINES  256
#define DEPTH   20
#define PTRSIZE 2000
#define RESIZE  100000
#define ABUFSIZE        20
#define LBSIZE  (8192+10)
#define ESIZE   256
#define LABSIZE 50

extern union reptr     *abuf[];
extern union reptr **aptr;
extern char    genbuf[];
extern char    *lbend;
extern char	*lcomend;
extern long    lnum;
extern char    linebuf[];
extern char    holdsp[];
extern char    *spend;
extern char    *hspend;
extern int     nflag;
extern long    tlno[];

#define ACOM    01
#define BCOM    020
#define CCOM    02
#define CDCOM   025
#define CNCOM   022
#define COCOM   017
#define CPCOM   023
#define DCOM    03
#define ECOM    015
#define EQCOM   013
#define FCOM    016
#define GCOM    027
#define CGCOM   030
#define HCOM    031
#define CHCOM   032
#define ICOM    04
#define LCOM    05
#define NCOM    012
#define PCOM    010
#define QCOM    011
#define RCOM    06
#define SCOM    07
#define TCOM    021
#define WCOM    014
#define CWCOM   024
#define YCOM    026
#define XCOM    033


union   reptr {
        struct reptr1 {
                char    *rhs;
                FILE    *fcode;
                char    command;
                int    gfl;
                char    pfl;
                char    inar;
                char    negfl;
		int	ad1lno;
		regex_t	ad1;
		int	ad2lno;
		regex_t ad2;
		int	re1lno;
		regex_t re1;
        } r1;
        struct reptr2 {
                char    *rhs;
                FILE    *fcode;
                char    command;
                int    gfl;
                char    pfl;
                char    inar;
                char    negfl;
		int	ad1lno;
		regex_t	ad1;
		int	ad2lno;
		regex_t ad2;
                union reptr     *lb1;
        } r2;
        struct reptr3 {
                char    *rhs;
                FILE    *fcode;
                char    command;
                int    gfl;
                char    pfl;
                char    inar;
                char    negfl;
		int	ad1lno;
		regex_t	ad1;
		int	ad2lno;
		regex_t ad2;
		char	*p;
        } r3;
};
extern union reptr ptrspace[];

#define LNO_RE (-1)
#define LNO_LAST (-2)
#define LNO_NONE (-3)
#define LNO_EMPTY (-4)

#define	MAXLABLEN	8

struct label {
        char    	asc[MAXLABLEN+1];
        union reptr     *chain;
        union reptr     *address;
};


struct savestr_s {
	int	maxlength;
	int	length;
	char	*s;
};

extern int     eargc;

void	initsavestr(struct savestr_s *);
void	freesavestr(struct savestr_s *);
#define savestr(sp,s) savestrn((sp),(s),(-1))
int	savestrn(struct savestr_s *,char *,int);
int	savestrlen(struct savestr_s *);
int	scansavestr(struct savestr_s *, char **,int);
extern union reptr     *pending;
extern char    *badp;
int	ycomp(char *,char **);
int	address(regex_t *,int *,char *);
char    *text();
int	compsub(regex_t *,char *,char **);
struct label    *search();
char    *gline();
char    *place();
