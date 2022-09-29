/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/acct/RCS/acctprc2.c,v 1.6 1996/06/14 19:52:18 rdb Exp $"

/*
 *	acctprc2 <ptmp1 >ptacct
 *	reads std. input (in ptmp.h/ascii format)
 *	hashes items with identical uid/name together, sums times
 *	sorts in uid/name order, writes tacct.h records to output
 */

#include <sys/types.h>
#include <sys/param.h>
#include "acctdef.h"
#include <stdio.h>

struct	ptmp	pb;
struct	tacct	tb;

struct	utab	{
	uid_t	ut_uid;
	char	ut_name[NSZ];
	float	ut_cpu[2];	/* cpu time (mins) */
	float	ut_kcore[2];	/* kcore-mins */
	long	ut_pc;		/* # processes */
};
struct utab *ub;
static	usize;
int a_usize;

void enter(struct ptmp *);
void output(void);
void squeeze(void);
int ucmp();

main(argc, argv)
char **argv;
{
	char *str;

	/* allocate memory for uid record */
	str = getenv(ACCT_A_USIZE);
	if (str == NULL) 
		a_usize = A_USIZE;
	else {
		a_usize = strtol(str, (char **)0, 0);
		if (errno == ERANGE || a_usize < A_USIZE)
			a_usize = A_USIZE;
	}
	ub = (struct utab *)calloc(a_usize, sizeof(struct utab));
	if (ub == (struct utab *)NULL) {
		fprintf(stderr, "%s: Cannot allocate memory\n", argv[0]);
		exit(5);
	}

	while (scanf("%ld\t%s\t%lu\t%lu\t%u",
		&pb.pt_uid,
		pb.pt_name,
		&pb.pt_cpu[0], &pb.pt_cpu[1],
		&pb.pt_mem) != EOF)
			enter(&pb);
	squeeze();
	qsort(ub, usize, sizeof(ub[0]), ucmp);
	output();
}

void
enter(struct ptmp *p)
{
	register unsigned i;
	int j;
	double memk;

	/* clear end of short users' names */
	for(i = strlen(p->pt_name) + 1; i < NSZ; p->pt_name[i++] = '\0') ;
	/* now hash the uid and login name */
	for(i = j = 0; p->pt_name[j] != '\0'; ++j)
		i = i*7 + p->pt_name[j];
	i = i*7 + (unsigned)p->pt_uid;
	j = 0;
	for (i %= a_usize; ub[i].ut_name[0] && j++ < a_usize; i = (i+1)% a_usize)
		if (p->pt_uid == ub[i].ut_uid && EQN(p->pt_name, ub[i].ut_name))
			break;
	if (j >= a_usize) {
		fprintf(stderr, "acctprc2: INCREASE THE VALUE OF THE ENVIRONMENT VARIABLE A_USIZE\n");
		exit(1);
	}
	if (ub[i].ut_name[0] == 0) {	/*this uid not yet in table so enter it*/
		ub[i].ut_uid = p->pt_uid;
		CPYN(ub[i].ut_name, p->pt_name);
	}
	ub[i].ut_cpu[0] += MINT(p->pt_cpu[0]);
	ub[i].ut_cpu[1] += MINT(p->pt_cpu[1]);
	memk = KCORE(pb.pt_mem);
	ub[i].ut_kcore[0] += memk * MINT(p->pt_cpu[0]);
	ub[i].ut_kcore[1] += memk * MINT(p->pt_cpu[1]);
	ub[i].ut_pc++;
}

void
squeeze(void)		/*eliminate holes in hash table*/
{
	register i, k;

	for (i = k = 0; i < a_usize; i++)
		if (ub[i].ut_name[0]) {
			ub[k].ut_uid = ub[i].ut_uid;
			CPYN(ub[k].ut_name, ub[i].ut_name);
			ub[k].ut_cpu[0] = ub[i].ut_cpu[0];
			ub[k].ut_cpu[1] = ub[i].ut_cpu[1];
			ub[k].ut_kcore[0] = ub[i].ut_kcore[0];
			ub[k].ut_kcore[1] = ub[i].ut_kcore[1];
			ub[k].ut_pc = ub[i].ut_pc;
			k++;
		}
	usize = k;
}

int
ucmp(struct utab *p1, struct utab *p2)
{
	if (p1->ut_uid != p2->ut_uid)
		/* the following (short) typecasts are a kludge fix
		 * for a bug in the 5.0 C compiler.  The bug returns a
		 * result that is always positive from the subtraction
		 * because of the unsigned short type of ut_uid.
		 */
		return((short)p1->ut_uid - (short)p2->ut_uid);
	return(strncmp(p1->ut_name, p2->ut_name, NSZ));
}

void
output(void)
{
	register i;

	for (i = 0; i < usize; i++) {
		tb.ta_uid = ub[i].ut_uid;
		CPYN(tb.ta_name, ub[i].ut_name);
		tb.ta_cpu[0] = ub[i].ut_cpu[0];
		tb.ta_cpu[1] = ub[i].ut_cpu[1];
		tb.ta_kcore[0] = ub[i].ut_kcore[0];
		tb.ta_kcore[1] = ub[i].ut_kcore[1];
		tb.ta_pc = ub[i].ut_pc;
		fwrite(&tb, sizeof(tb), 1, stdout);
	}
}
