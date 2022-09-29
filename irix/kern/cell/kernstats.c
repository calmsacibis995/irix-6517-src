/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifdef CELL

#ident  "$Revision: 1.5 $"
#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/idbgentry.h>
#include <sys/ktrace.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/runq.h>
#include <sys/sema.h>
#include <sys/systm.h>
#include <sys/ksa.h>
#include <sys/cmn_err.h>
#include <sys/kopt.h>
#include <sys/syssgi.h>
#include <ksys/cell.h>
#include <ksys/sthread.h>
#include <ksys/kqueue.h>
#include <ksys/partition.h>
#include <sys/alenlist.h>
#include <ksys/cell/mesg.h>
#include <ksys/cell/service.h>
#include <ksys/cell/subsysid.h>
#include <ksys/cell/membership.h>
#include <ksys/cell/subsysid.h>
#include <ksys/cell/remote.h>
#include "../cell/cms/cms_base.h"
#include "../cell/cms/cms_message.h"
#include "../cell/cms/cms_info.h"


extern void *kmem_alloc(size_t , int );
extern struct mesgstat *mesgstat[MAX_SUBSYSID + 1];
extern struct mesgstat *mesgstat_pool;
extern int mesgstat_index;
extern lock_t mesgstat_lock;
extern time_t mesgstat_start;  


#include <sys/cellkernstats.h>


#define MAX_MESGSTAT	400

/* Return message statistics for each message that has occurred.
 * If the first argument is a '0' it resets the associated data structures.
 */
void
get_mesg_stats(__psint_t x, int tot_msgs , void  *DS)
{
	struct mesgstat *mesgp;
	int i, y, tot_count = 0;
	time_t tdiff;
	int mesgsnt, mesgrcv;
	uint64_t tot_etime = 0;
	__psunsigned_t offset;
	char * ptr_c;
	mesgstatsinfo_t *ptrmsg_t;
        mesginfo_t *ptr_t;



	if (!mesgstat_pool) {
		return;
	}
	switch(x)
	{

		case SGI_RESET_COUNT:
	     		/* Zero out the stats */
	     		bzero(mesgstat_pool, 
				mesgstat_index * sizeof(struct mesgstat));
	     		bzero(mesgstat, 
				sizeof(struct mesgstat *) * (MAX_SUBSYSID + 1));
			mesgstat_index = 0;
			mesgstat_start = lbolt;
	     
	     		for (y = 0; y < maxcpus; y++) {
		  		if (pdaindr[y].CpuId == -1) {
		       			continue;
		  		}
		  	pdaindr[y].pda->ksaptr->si.mesgsnt = 0;
		  	pdaindr[y].pda->ksaptr->si.mesgrcv = 0;
	     		}
	     		return;
	     
		case SGI_REPORT_MESSAGE_COUNT:
			/* get the message count */
	     		ptrmsg_t = (mesgstatsinfo_t *)DS;
	     		tdiff = (lbolt - mesgstat_start) / HZ;
	     		ptrmsg_t->tdiff = tdiff;
	     		if (!tdiff) 
	     		{
		  
		  		/* Stats have just been set to zero */
		 		ptrmsg_t->tot_msgs = 0;
		  		return; 
	     		}
	     		mesgsnt = mesgrcv = 0;
	     		for (y = 0; y < maxcpus; y++) {
		  		if (pdaindr[y].CpuId == -1) {
		       			continue;
		  		}
		  	mesgsnt += pdaindr[y].pda->ksaptr->si.mesgsnt;
		  	mesgrcv += pdaindr[y].pda->ksaptr->si.mesgrcv;
	     		}
	     		ptrmsg_t->cellid = cellid();
	     		ptrmsg_t->mesgsnt = mesgsnt;
	     		ptrmsg_t->mesgrcv = mesgrcv;
	     		for (i = 0; i <= MAX_MESGSTAT; i++) 
	     		{
		  		mesgp = &mesgstat_pool[i];
		  		if (!mesgp->name[0]) 
		  		{
		       			/* End of the current list */
		       			break;
		  		}
		  		tot_etime += mesgp->etime;
		  		tot_count += mesgp->count;
	     		}
	     		ptrmsg_t->tot_msgs = i;
	     		ptrmsg_t->tot_count = tot_count;
	     		if(tot_count)
		 	ptrmsg_t->tot_etime =(uint64_t)
		       		(((long long)tot_etime * timer_unit) /
					(1000 * tot_count));
	     		return;

		case SGI_REPORT_MESSAGE_STATS:
	     		ptr_t = (mesginfo_t *)DS;
	     		for (i = 0; (i <= MAX_MESGSTAT)&& (i<tot_msgs); i++) 
	     		{
		  		mesgp = &mesgstat_pool[i];
		  		if (!mesgp->name[0]) 
		  		{
		       			/* End of the current list */
		       			break;
		  		}
		  		strcpy(ptr_t[i].message,mesgp->name);
		  		ptr_t[i].calls=mesgp->count;
		  		if(mesgp->count)
		       		ptr_t[i].etime = (uint64_t)
			    		(((long long)mesgp->etime *
			      		timer_unit) / (1000 * mesgp->count));
		  		ptr_c = fetch_kname(mesgp->return_addr, 
						&offset);
		  		strcpy(ptr_t[i].caller ,ptr_c);
	     		}
	     		return;

		default:
			/* never comes here */
	     		return;
	}	

}

int 
export_message_stat(struct syssgia  *uap)
{
 	switch((int)uap->arg2)
	{
		       
		mesgstatsinfo_t kmesgsinfo;
	       	mesginfo_t *ptr_t;
	       	int debugflag;
		extern void reset_mesgsize_stats(void);
	  	case SGI_RESET_COUNT:
		{
#ifdef DEBUG
			get_mesg_stats(SGI_RESET_COUNT,0, NULL);
			reset_mesgsize_stats();
#else
			debugflag = -1;
			if(copyout((void *)&debugflag, (caddr_t)uap->arg3,
			   sizeof(int )))
				return EFAULT;
#endif /*DEBUG */
		break;
		}
		       
		case SGI_REPORT_MESSAGE_COUNT:
		{
#ifdef DEBUG			  
			get_mesg_stats(SGI_REPORT_MESSAGE_COUNT,
				0,(void *)&kmesgsinfo );
			if (copyout((void *)&kmesgsinfo, 
				(mesgstatsinfo_t *)uap->arg3,
				   sizeof(mesgstatsinfo_t )))
			{
				return EFAULT;
			}
#else
			debugflag = -1;
			if(copyout((void *)&debugflag, (caddr_t)uap->arg4,
			   sizeof(int )))
				return EFAULT;
#endif /* DEBUG */
			break;
		}
		case SGI_REPORT_MESSAGE_STATS:
		{
#ifdef DEBUG
			ptr_t  = kmem_alloc(sizeof(mesginfo_t)*(int)uap->arg3,
					   KM_SLEEP);
			get_mesg_stats(SGI_REPORT_MESSAGE_STATS,
				(int)uap->arg3,(void *)ptr_t);
			if((copyout((void *)ptr_t, (caddr_t)uap->arg4,
			   sizeof(mesginfo_t )*(int)uap->arg3)))
				return EFAULT;
			kmem_free((void *)ptr_t, 
				sizeof(mesginfo_t)*(int)uap->arg3);
			break;
#endif /*DEBUG */
		}
		case SGI_REPORT_MESSAGESIZE_COUNT:
		{
#ifdef DEBUG
			int psize[4];
			extern mesgsize_grab_stats(int , void *, void *, int);
			mesgsize_grab_stats(1, NULL,(void *)psize,0);
			if(copyout((void *)psize, (caddr_t)uap->arg3,
				   sizeof(int)*4))
			    return EFAULT;
#else
			int debugflag = -1;
		       if(copyout((void *)&debugflag, (caddr_t)uap->arg4,
				  sizeof(int )))
			    return EFAULT;
#endif /* DEBUG */
			break;
		}
		case SGI_REPORT_MESSAGESIZE_STATS:
		{
#ifdef DEBUG
			int psize[4], tot_mem;
			mesgsizestatsinfo_t *mesgsizeinfo;
			extern mesgsize_grab_stats(int , void *, void *, int);
			if (copyin((caddr_t)uap->arg3, psize, 
				sizeof(int)*4))
				return EFAULT;
			tot_mem = psize[0]+psize[1]+psize[2]+psize[3];
			if(tot_mem)
			{
		
				mesgsizeinfo = kmem_alloc(sizeof(mesgsizestatsinfo_t)\
						*tot_mem*2,KM_SLEEP);
				mesgsize_grab_stats(0, (void *)mesgsizeinfo,
					(void *)&psize, tot_mem);
				if(copyout((void *)(mesgsizeinfo ), (void *)uap->arg4,
					sizeof(mesgsizestatsinfo_t)*tot_mem))
					return EFAULT;
				kmem_free((void *)mesgsizeinfo, 
					sizeof(mesgsizestatsinfo_t)*tot_mem*2);
			}
#endif /*DEBUG */
			break;
		}
		default:
		return EINVAL;
	  }
	  return 0;
		  
}

int
export_membership_stat( struct syssgia *uap)
{
	membershipinfo_t meminfo;
	extern cms_info_t *cip;
	int i, count;

		count = 0;
		meminfo.golden_cell = CELL_GOLDEN;
		for( i= 0; i < MAX_CELLS; i++)
		{
	       
			if(meminfo.membership[i] = cell_in_membership(i))
			{
				count++;
				meminfo.cell_age[i] = cip->cms_age[i];
			}
		}
		meminfo.num_active_cells = count;
		if(copyout((void *)&meminfo, (caddr_t)uap->arg2,
			sizeof(membershipinfo_t )))
		{
			qprintf("\n problem s here");
			return EFAULT;
		}
	return 0;
		  
}
#endif  /*CELL */
