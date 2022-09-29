/*	Copyright (c) 1993 UNIX System Laboratories, Inc. 	*/
/*	  All Rights Reserved                             	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.   	            	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*		copyright	"%c%" 	*/


#ident	"@(#)acct:common/cmd/acct/acctprc.c	1.3.7.4"
#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/acct/RCS/acctprc.c,v 1.3 1996/06/14 21:05:41 rdb Exp $"
/*
 *      acctprc
 *      reads std. input (acct.h format), 
 *      writes std. output (tacct format)
 *      sorted by uid
 *      adds login names
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include "acctdef.h"
#include <limits.h>
#include <string.h>

struct  acct    ab;
struct  ptmp    pb;
struct  tacct   tb;

struct  utab    {
        uid_t   ut_uid;
        char    ut_name[NSZ];
        float   ut_cpu[2];      /* cpu time (mins) */
        float   ut_kcore[2];    /* kcore-mins */
        long    ut_pc;          /* # processes */
};
struct utab *ub;

int a_usize;
static  usize;

void	enter(struct ptmp *);
void	output(void);
void	squeeze(void);
int     ucmp();

main(int argc, char **argv)
{
        long    elaps[2];
        long    etime, stime;
	__uint64_t mem, kcoremin;
	char 	*str;

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
	if (ub == NULL) {
		fprintf(stderr, "%s: Cannot allocate memory\n", argv[0]);
		exit(5);
	}

        while (fread(&ab, sizeof(ab), 1, stdin) == 1) {
                if (!MYKIND(ab.ac_flag))
                        continue;
                pb.pt_uid = ab.ac_uid;
		pb.pt_name[0] = '\0';
                /*
                 * approximate cpu P/NP split as same as elapsed time
                 */
                if ((etime = SECS(expand32(ab.ac_etime))) == 0)
                        etime = 1;
                stime = expand32(ab.ac_stime) + expand32(ab.ac_utime);
                mem = expand64(ab.ac_mem);
                pnpsplit(ab.ac_btime, etime, elaps);
                pb.pt_cpu[0] = (double)stime * (double)elaps[0] / etime;
                pb.pt_cpu[1] = (stime > pb.pt_cpu[0])? stime - pb.pt_cpu[0] : 0;
                pb.pt_cpu[1] = stime - pb.pt_cpu[0];
                if (stime) {
			kcoremin = (mem + stime - 1) / stime;
			if (kcoremin > UINT_MAX) {
				pb.pt_mem = UINT_MAX;
			}
			else {
				pb.pt_mem = (unsigned int) kcoremin;
			}
		}
                else
                        pb.pt_mem = 0;  /* unlikely */
                enter(&pb);
        }
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

        i=(unsigned)p->pt_uid;
        j=0;
        for (i %= a_usize; !UBEMPTY && j++ < a_usize; i = (i+1) % a_usize)
                if (p->pt_uid == ub[i].ut_uid) 
                        break;
        if (j >= a_usize) {
		fprintf(stderr, "acctprc: INCREASE THE VALUE OF THE ENVIRONMENT VARIABLE ACCT_A_USIZE\n");
                exit(1);
        }
        if (UBEMPTY) {
                ub[i].ut_uid = p->pt_uid;
		ub[i].ut_name[0] = '\0';
        }
        ub[i].ut_cpu[0] += MINT(p->pt_cpu[0]);
        ub[i].ut_cpu[1] += MINT(p->pt_cpu[1]);
        memk = KCORE(pb.pt_mem);
        ub[i].ut_kcore[0] += memk * MINT(p->pt_cpu[0]);
        ub[i].ut_kcore[1] += memk * MINT(p->pt_cpu[1]);
        ub[i].ut_pc++;
}

void
squeeze(void)               /*eliminate holes in hash table*/
{
        register i, k;

        for (i = k = 0; i < a_usize; i++)
                if (!UBEMPTY) {
                        ub[k].ut_uid = ub[i].ut_uid;
			ub[k].ut_name[0] = '\0';
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
	if (p1->ut_uid > p2->ut_uid)
                return 1;
        else if (p1->ut_uid < p2->ut_uid)
                return (-1);
        else 
                return (0);
}

void
output(void)
{
        register i;
        char *uidtonam();

        for (i = 0; i < usize; i++) {
                tb.ta_uid = ub[i].ut_uid;
                CPYN(tb.ta_name, uidtonam(ub[i].ut_uid));
                tb.ta_cpu[0] = ub[i].ut_cpu[0];
                tb.ta_cpu[1] = ub[i].ut_cpu[1];
                tb.ta_kcore[0] = ub[i].ut_kcore[0];
                tb.ta_kcore[1] = ub[i].ut_kcore[1];
                tb.ta_pc = ub[i].ut_pc;
                fwrite(&tb, sizeof(tb), 1, stdout);
        }
}
