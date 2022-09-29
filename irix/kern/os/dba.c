/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * SGI Database Accelerator option - misc.
 */

#ident	"$Revision: 1.8 $"

#include <sys/types.h>
#include <sys/dbacc.h>
#include <sys/buf.h>
#include <sys/kaio.h>
#include <sys/pda.h>
#include <sys/systm.h>
#include <sys/errno.h>

extern void klistio_init(void), kaio_init(void), kaio_clearstats(void);
extern int nlpages_64k, nlpages_256k, nlpages_1m, nlpages_4m, nlpages_16m;
/* If any part of DBA is available, enable large pages */
int dba_use_lgpg;

void dba_init () {
	int i;
#if defined(IP19) || defined(IP25) || defined(IP27) || defined(IP33)   /* DBA supported on big servers only */
	/* Collect stats on a per-cpu basis */
	for (i = 0; i < maxcpus; i++) {
		if (pdaindr[i].CpuId == -1)
			continue;
		pdaindr[i].pda->dbastat = kern_calloc(1, sizeof(struct dba_stats));
	}
#endif
	/* Initialize DBA subsystems */
	klistio_init();
	kaio_init();
}


/*
 * It's ok to fill in the entire structure here. syssgi() will only copy
 * out as much of the structure as the user requested.
 */
int
dba_getconfig(struct dba_conf *dcf)
{
#if defined(IP19) || defined(IP25) || defined(IP27) || defined(IP33)   /* DBA supported on big servers only */
	dcf->dbcf_installed = postwait_installed() | klistio_installed()
		| kaio_installed();
	if (dcf->dbcf_installed) {
		/* Never return SGI_DBACF_LPG - it's the old 6.2 interface */
		dcf->dbcf_installed |= SGI_DBACF_LPG2;
		dba_use_lgpg = 1;
	} else {
		dcf->dbcf_installed = 0;
		dba_use_lgpg = 0;
	}
	if (dcf->dbcf_installed & (SGI_DBACF_PPHYSIO32|SGI_DBACF_PPHYSIO64)) {
		klistio_getparams(dcf);
	}
	if (dcf->dbcf_installed & SGI_DBACF_POSTWAIT) {
		postwait_getparams(dcf);
	}
	if (dcf->dbcf_installed & SGI_DBACF_AIO) {
		kaio_getparams(dcf);
	}
	if (dcf->dbcf_installed & (SGI_DBACF_LPG|SGI_DBACF_LPG2)) {
		dcf->dbcf_lpg_n64k = nlpages_64k;
		dcf->dbcf_lpg_n256k = nlpages_256k;
		dcf->dbcf_lpg_n1m = nlpages_1m;
		dcf->dbcf_lpg_n4m = nlpages_4m;
		dcf->dbcf_lpg_n16m = nlpages_16m;
		dcf->dbcf_lpg_maxwired = 0;
	}	
#else
	dcf->dbcf_installed = 0;
	dba_use_lgpg = 0;
#endif
	return 0;
}

/* ARGSUSED */
int
dba_putconfig(struct dba_conf *dcf)
{
	int error = 0;

	return error;
}

/* buffer, length, cpu# */
int
dba_getstats(dba_stat_t *dbuf, int dlen, int cpu)
{
#if defined(IP19) || defined(IP25) || defined(IP27) || defined(IP33)   /* DBA supported on big servers only */
     int i;
     dba_field_t proc_hi;

     if (cpu == -1) { 	  /* Sum for all CPUs */
	  dba_stat_t dsum;
	  bzero(&dsum, sizeof dsum);
	  for (i = 0; i < maxcpus; i++) {
	       if ((pdaindr[i].CpuId == -1) || !(pdaindr[i].pda->dbastat))
		    continue;
#define DBASUM(s,x,c)	((s).x += ((dba_stat_t *)(pdaindr[(c)].pda->dbastat))->x)
	       DBASUM(dsum,kaio_nobuf,i);
	       DBASUM(dsum,kaio_inprogress,i);
	       if ((proc_hi = ((dba_stat_t *)(pdaindr[i].pda->dbastat))->kaio_proc_maxinuse) > dsum.kaio_proc_maxinuse)
		       dsum.kaio_proc_maxinuse = proc_hi;
	       DBASUM(dsum,kaio_aio_inuse,i);
	       DBASUM(dsum,kaio_reads,i);
	       DBASUM(dsum,kaio_writes,i);
	       DBASUM(dsum,kaio_read_bytes,i);
	       DBASUM(dsum,kaio_write_bytes,i);
	       DBASUM(dsum,kaio_io_errs,i);
	       DBASUM(dsum,kaio_free,i);
#if 0
	       DBASUM(dsum,kaio_ck_free_cnt,i);/*DEBUG*/
#endif
	  }
	  kaio_stats(&dsum);
	  copyout(&dsum, dbuf, MIN(sizeof(dsum), dlen));
     } else {	/* Just get requested CPU */
	  dba_stat_t dtmp;
	  if ((cpu >= maxcpus) || (cpu < 0))
		  return EINVAL;
	  if ((pdaindr[cpu].CpuId == -1)
	      || !(pdaindr[cpu].pda->dbastat))
		  return EINVAL;
	  bcopy(pdaindr[cpu].pda->dbastat, &dtmp, sizeof(dtmp));
	  kaio_stats(&dtmp);
	  copyout(&dtmp, dbuf,
		  MIN(sizeof(dba_stat_t), dlen));
     }
#endif
     return 0;
}

int
dba_clearstats(void)
{
#if defined(IP19) || defined(IP25) || defined(IP27) || defined(IP33)   /* DBA supported on big servers only */
     int i;

     /* make sure we have the correct permissions */
     if (!_CAP_ABLE(CAP_SYSINFO_MGT) || !_CAP_ABLE(CAP_DEVICE_MGT)) {
	  return(EPERM);
     }
     /* Clear on all CPUs */
     for (i = 0; i < maxcpus; i++) {
	  if ((pdaindr[i].CpuId == -1) || !(pdaindr[i].pda->dbastat))
	       continue;
	  bzero(pdaindr[i].pda->dbastat, sizeof(dba_stat_t));
	  kaio_clearstats();
     }
#endif
     return(0);
}
