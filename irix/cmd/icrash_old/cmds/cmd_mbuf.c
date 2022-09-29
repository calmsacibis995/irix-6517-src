#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_mbuf.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"
#define _KERNEL 0
#include <sys/file.h>
#include <sys/socket.h>
#undef _KERNEL

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#include <sys/mbuf.h>
#include <sys/tcpipstats.h>
#include "icrash.h"
#include "extern.h"
#include <search.h>
#include <varargs.h>

#define HOWMANY(n, u)   (((n)+(u)-1)/(u))
#define ROUNDUP(n, u)   (HOWMANY(n, u) * (u))
#define GET_UINT(SYMBOL_STR,KPOINTER,VAR,DEFAULT_VALUE)               \
{                                                                     \
	if(K_SYM_ADDR(K)((void *)NULL, SYMBOL_STR,&KPOINTER) < 0      \
	   || kl_get_block(K,KPOINTER,4,&VAR,SYMBOL_STR)              \
	   == NULL)                                                   \
	{                                                             \
		VAR = DEFAULT_VALUE;                                  \
	}                                                             \
}

char *plural(int n);
char *pluraly(int n);
char *plurales(int n);

#define MBUF 1
#define OTHER 2
	
static struct mbtypes {
	        int mt_type;
	        int     mt_flag;
	        char    *(*mt_func)(int);
	        char    *mt_name;
} mbtypes[] = {
	{ MT_FREE,      MBUF, plural, "free mbuf" },
	{ MT_DATA,      MBUF, plural, "data mbuf" },
	{ MT_HEADER,    MBUF, plural, "packet header mbuf" },
	{ MT_SONAME,    MBUF, plural, "socket names and address mbuf" },
	{ MT_SOOPTS,    MBUF, plural, "socket option mbuf" },
	{ MT_FTABLE,    MBUF, plural, "fragment reassembly queue header mbuf"},
	{ MT_RIGHTS,    MBUF, plural, "access right mbuf" },
	{ MT_MRTABLE,   MBUF, plural, "multicast routing structure mbuf" },
	{ MT_IPMOPTS,   MBUF, plural, "internet multicast option mbuf" },
	{ MT_SOCKET,    OTHER, plural, "socket structure" },
	{ MT_PCB,       OTHER, plural, "protocol control block" },
	{ MT_RTABLE,    OTHER, pluraly, "routing table entr" },
	{ MT_HTABLE,    OTHER, pluraly, "IMP host table entr" },
	{ MT_ATABLE,    OTHER, plural, "address resolution table" },
	{ MT_SAT,       OTHER, plural, "security audit trail buffer" },
	{ MT_IFADDR,    OTHER, plurales, "interface address" },
	{ MT_DN_DRBUF,  OTHER, plural, "4DDN driver buffer" },
	{ MT_DN_BLK,    OTHER, plural, "4DDN block allocator" },
	{ MT_MCAST_RINFO, OTHER, plural, "multicast route info structure" },
	{ MT_SESMGR,    OTHER, plural, "trusted networking session manager" },
	{ 0, 0 }
};

typedef struct mbuf_queue {
	element_t elem;				       /* should be first.   */
	kaddr_t mbuf;				       /* chain of mbufs.    */
	char *title;				       /* name of mbuf type. */
} mbuf_queue_t;


extern kaddr_t **find_all_vsockets(int ,FILE *,
				   int (*)(kaddr_t,kaddr_t,char *,int ,int ,
					   FILE *,list_of_ptrs_t *,int *),
				   k_ptr_t **);

extern int test_vfile_mbufsocket(kaddr_t,kaddr_t caddr,char *fbuf,
				 int flags,int j,
				 FILE *ofp,list_of_ptrs_t *list,
				 int *first_time);

extern void print_kaddrt(kaddr_t **val,int level,void *arg);
extern void free_kaddrt(kaddr_t **val,int level,void *arg);
	
/*
 * Return the number of mbufs in an mbuf chain...following the m_next ptr.
 * as opposed to an m_act ptr.
 * m is a kernel address of the first mbuf.
 */
int 
icrash_mbuf_count(kaddr_t m,int msize)
{
	int n = 0;
	k_ptr_t Pmbuf=alloc_block(msize,B_TEMP);

	while (m && kl_get_struct(K,m,msize,Pmbuf,"mbuf"))
	{
		n++;
		m=kl_kaddr(K,Pmbuf,"mbuf","m_next");
	}

	K_BLOCK_FREE(K)(NULL,Pmbuf);
	return n;
}

void ifq_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) 
	{
		fprintf(ofp,"IFQUEUE MBUF LIST:\n");
		fprintf(ofp,"             MBUF  TYPE      COUNT\n");
	}
	
	if (flags & SMAJOR)
	{
		fprintf(ofp,
			"==========================="
			"=========================\n");
		
	}

	if (flags & SMINOR)
	{
		fprintf(ofp,
			"---------------------------"
			"-------------------------\n");		
	}
		
}

/*
 * print the mbufs in the ifqueue structure. Follow the higher level 
 * mbuf ptr -- m_act. User can always use mbuf -n on each mbuf printed
 * out.
 */
int print_mbufs_ifq(kaddr_t KPifq, k_ptr_t Pifq, int flags, FILE *ofp,
		    char *Pchar,int *firsttime) 
{
	int len,i;
	char str[20];
	kaddr_t KPmbuf;
	k_ptr_t Pmbuf;

	Pmbuf  = alloc_block(mbufconst.m_msize,B_TEMP);
	strcpy(str,Pchar);
	len=strlen(Pchar);
#define LENGTH_TO_PRETTY_PRINT 9
	if(len < LENGTH_TO_PRETTY_PRINT)
	{
		for(i=len;i<LENGTH_TO_PRETTY_PRINT;i++)
			strcat(str," ");
	}
	else
	{
		str[LENGTH_TO_PRETTY_PRINT] = 0;
	}
	KPmbuf=kl_kaddr(K,Pifq,"ifqueue","ifq_head");
	i=0;
	while(KPmbuf)
	{
		if(!kl_get_struct(K,KPmbuf,mbufconst.m_msize,
				  Pmbuf,"mbuf"))
		{
			break;
		}
		i++;
		if(!*firsttime)
		{
			(*firsttime)++;
			ifq_banner(ofp,flags);
		}
		
		fprintf(ofp," %16llx  %s  %4d\n",KPmbuf,str,
			icrash_mbuf_count(KPmbuf, mbufconst.m_msize));
		KPmbuf = kl_kaddr(K,Pmbuf,"mbuf","m_act");
	}
	K_BLOCK_FREE(K)(NULL,Pmbuf);
	
	return i;
}

char *
plural(n)
	        int n;
{

	return (n != 1 ? "s" : "");
}

char *
pluraly(n)
	        int n;
{
	return (n != 1 ? "ies" : "y");
}

char *
plurales(n)
	        int n;
{
	return (n != 1 ? "es" : "");
}


		
/*
 * mbuf_cmd() -- Dump out mbuf structure(s).
 */
int
mbuf_cmd(command_t cmd)
{
	unsigned int i, j, k, mbuf_cnt = 0;
	kaddr_t mbuf, next = (kaddr_t)NULL;
	k_ptr_t m;
	
	k_ptr_t Pmbcache = NULL,Pmb;
	kaddr_t KPvoid;
	int totmbufs, totmem, totfree, nmbtypes;
	int smlfree,medfree,bigfree,bytesfree;
	int mcbpages, pcbpages, totpages, freepages;
	int mbnum;
	mbuf_queue_t *mbuf_queue,*head_mbuf_queue=NULL;
        struct mbstat *Pmbstat;
	struct mbtypes *mp;
	char string[80];	 /* We don't want more than a line anyway. */
	int seen[256];
	k_ptr_t *Pvsockets_hashbase,*Psockets_hashbase;
	list_of_ptrs_t *list;
        k_ptr_t sp,rcvp,sndp,Pvoid;
	int firsttime=0;

	int frag_buckets,ifqueue_size,size;
	kaddr_t KPipq,KPbucketipq,KPmbuf;
	k_ptr_t Pipq,Pipq_next,Pmbuf;

       /* 
	* This check --> kl_struct_len is only temporary until struct mb 
	* gets moved to a header file which gets included in space.c
	*
	* In retrospect we should just document that the addtype command
	* can be used to include this structure.
	*/
	if(cmd.nargs == 0 && kl_struct_len(K,"mb"))
	{
		/*
		 * If you see an eerie resemblence between the code here and
		 * m_allocstats.. then you are on the right track !.
		 *
		 * First do only the mbstat stuff.
		 */
	       /*
		* Macro in kern_mbuf.c .. 
		* #if SN0 ..
		* #define MBNUM .. 
		* #elif EVEREST...
		* #define MBNUM...
		* #endif
		*/
		if (PLATFORM_SNXX || PLATFORM_EVEREST)
		{
			mbnum = K_MAXCPUS(K);
		}
		else
		{
			mbnum = 1;
		}
			
		Pmbcache = alloc_block((mbnum * kl_struct_len(K,"mb")),B_TEMP);
		if(K_SYM_ADDR(K)((void *)NULL, "mbcache",&KPvoid) < 0 ||
		   kl_get_block(K,KPvoid,K_NBPW(K)*mbnum,Pmbcache,"mbcache") 
		   == NULL)
		{
			K_BLOCK_FREE(K)(NULL,Pmbcache);
			return -1;
		}
			

	        /*
		 * Let's put some debug statements and some assert's here.
		 */
		if (DEBUG(DC_GLOBAL, 1)) {
			fprintf(cmd.ofp,"DEBUG: MBNUM = %d\n",mbnum);
		}
		Pmbstat = (struct mbstat *)alloc_block(sizeof (struct mbstat),
						       B_TEMP);

		Pmb     = (struct mbstat *)alloc_block(kl_struct_len(K,"mb"),
						       B_TEMP);

		bzero(Pmbstat, sizeof (struct mbstat));	/* Let's be safe.. */
		bytesfree = 0;
	       /*
		* Print out info. on all the mbufs we can find. Just print out
		* the head of the list of mbufs. Follow them if the -n flag is
		* specified.
		*/
		for(i=0;i<mbnum;i++)
		{
			if(!((kaddr_t *)Pmbcache)[i] ||
			    kl_get_struct(K,((kaddr_t) PTRSZ32(K) ?
					     ((kaddr_t)
					      ((__uint32_t *)Pmbcache)[i]) :
					     ((kaddr_t *)Pmbcache)[i]),
					  kl_struct_len(K,"mb"),Pmb,"mb") ==
				NULL)
			{
				continue;
			}
			
			
			/*
			 *  small mbufs.
			 */
			if(KPvoid = kl_kaddr(K,Pmb,"mb","mb_sml"))
			{
				smlfree = icrash_mbuf_count(KPvoid,
							    mbufconst.m_msize);
				bytesfree += smlfree * mbufconst.m_msize;

				if(PLATFORM_SNXX) 
				{
					if(!firsttime)
					{
						firsttime++;
						fprintf(cmd.ofp,
							"FREE MBUF LIST:\n");
						fprintf(cmd.ofp,
							"             MBUF"
							"  TYPE   NODE  "
							"COUNT\n");
						fprintf(cmd.ofp,
							"================="
							"======="
							"=================\n");
					}
					fprintf(cmd.ofp," %16llx  SMALL  "
						"%4d   %4d\n",
						KPvoid,i,smlfree);
					sprintf(string,
						"Free small mbuf list for node"
						" %d",
						i);
				} 
				else if (mbnum == 1)
				{
					if(!firsttime)
					{
						firsttime++;
						fprintf(cmd.ofp,
							"FREE MBUF LIST:\n");
						fprintf(cmd.ofp,
							"             MBUF"
							"  TYPE  COUNT\n");
						fprintf(cmd.ofp,
							"================="
							"======="
							"=================\n");
					}
					fprintf(cmd.ofp," %16llx  SMALL  "
						"%4d\n",
						KPvoid,smlfree);
					sprintf(string,
						"Free small mbuf list number");
				}
				else
				{
					if(!firsttime)
					{
						firsttime++;
						fprintf(cmd.ofp,
							"FREE MBUF LIST:\n");
						fprintf(cmd.ofp,
							"             MBUF"
							"  TYPE  MBNUM  "
							"COUNT\n");
						fprintf(cmd.ofp,
							"================="
							"======="
							"=================\n");
					}
					fprintf(cmd.ofp," %16llx  SMALL  "
						"%4d   %4d\n",
						KPvoid,i,smlfree);
					sprintf(string,
						"Free small mbuf list number"
						" %d",
						i);
				}
					
				mbuf_queue = (mbuf_queue_t *)
					alloc_block(sizeof(struct mbuf_queue),
						    B_TEMP);
				
				mbuf_queue->title = (char *)
					alloc_block((strlen(string)+1)*
						    sizeof(char),B_TEMP);
				strcpy(mbuf_queue->title,string);
				mbuf_queue->mbuf = KPvoid;
				ENQUEUE(&head_mbuf_queue,mbuf_queue);
			}
			/*
			 *  medium mbufs.
			 */
			if(KPvoid = kl_kaddr(K,Pmb,"mb","mb_med"))
			{
				medfree = icrash_mbuf_count(KPvoid,
							    mbufconst.m_msize);
				bytesfree += 
					medfree * (mbufconst.m_msize + 
						   mbufconst.m_mbufclsize);

				if(PLATFORM_SNXX) 
				{
					if(!firsttime)
					{
						firsttime++;
						fprintf(cmd.ofp,
							"FREE MBUF LIST:\n");
						fprintf(cmd.ofp,
							"             MBUF"
							"  TYPE   NODE  "
							"COUNT\n");
						fprintf(cmd.ofp,
							"================="
							"======="
							"=================\n");
					}
					fprintf(cmd.ofp," %16llx  MEDIUM "
						"%4d   %4d\n",
						KPvoid,i,medfree);
					sprintf(string,
						"Free medium mbuf list for"
						" node %d",
						i);
				} 
				else if (mbnum == 1)
				{
					if(!firsttime)
					{
						firsttime++;
						fprintf(cmd.ofp,
							"FREE MBUF LIST:\n");
						fprintf(cmd.ofp,
							"             MBUF"
							"  TYPE  COUNT\n");
						fprintf(cmd.ofp,
							"================="
							"======="
							"=================\n");
					}
					fprintf(cmd.ofp," %16llx  MEDIUM "
						"%4d\n",
						KPvoid,medfree);
					sprintf(string,
						"Free medium mbuf "
						"list number");
				}
				else
				{
					if(!firsttime)
					{
						firsttime++;
						fprintf(cmd.ofp,
							"FREE MBUF LIST:\n");
						fprintf(cmd.ofp,
							"             MBUF"
							"  TYPE  MBNUM  "
							"COUNT\n");
						fprintf(cmd.ofp,
							"================="
							"======="
							"=================\n");
					}
					fprintf(cmd.ofp," %16llx  MEDIUM "
						"%4d   %4d\n",
						KPvoid,i,medfree);
					sprintf(string,
						"Free medium mbuf list number"
						" %d",
						i);
				}
				mbuf_queue = (mbuf_queue_t *)
					alloc_block(sizeof(struct mbuf_queue),
						    B_TEMP);
			
				mbuf_queue->title = (char *)
					alloc_block((strlen(string)+1)*
						    sizeof(char),B_TEMP);
				strcpy(mbuf_queue->title,string);
				mbuf_queue->mbuf = KPvoid;
				ENQUEUE(&head_mbuf_queue,mbuf_queue);
			}

			/*
			 *  large mbufs.
			 */
			if(KPvoid = kl_kaddr(K,Pmb,"mb","mb_big"))
			{
				bigfree = icrash_mbuf_count(KPvoid,
							    mbufconst.m_msize);
				bytesfree += bigfree * 
					(mbufconst.m_msize + 
					 mbufconst.m_mclbytes);

				if(PLATFORM_SNXX) 
				{
					if(!firsttime)
					{
						firsttime++;
						fprintf(cmd.ofp,
							"FREE MBUF LIST:\n");
						fprintf(cmd.ofp,
							"             MBUF"
							"  TYPE   NODE\n");
						fprintf(cmd.ofp,
							"================="
							"======="
							"=================\n");
					}
					fprintf(cmd.ofp," %16llx  LARGE  "
						"%4d   %4d\n",
						KPvoid,i,bigfree);
					sprintf(string,
						"Free large mbuf list for node"
						" %d",
						i);
				} 
				else if(mbnum == 1)
				{
					if(!firsttime)
					{
						firsttime++;
						fprintf(cmd.ofp,
							"FREE MBUF LIST:\n");
						fprintf(cmd.ofp,
							"             MBUF"
							"  TYPE\n");
						fprintf(cmd.ofp,
							"================="
							"======="
							"=================\n");
					}
					fprintf(cmd.ofp," %16llx  LARGE  "
						"%4d\n",
						KPvoid,bigfree);
					sprintf(string,
						"Free large mbuf list number");
				}
				else
				{
					if(!firsttime)
					{
						firsttime++;
						fprintf(cmd.ofp,
							"FREE MBUF LIST:\n");
						fprintf(cmd.ofp,
							"             MBUF"
							"  TYPE      MBNUM\n");
						fprintf(cmd.ofp,
							"================="
							"======="
							"=================\n");
					}
					fprintf(cmd.ofp," %16llx  LARGE  "
						"%4d   %4d\n",
						KPvoid,i,bigfree);
					sprintf(string,
						"Free large mbuf list number"
						" %d",
						i);
				}
				mbuf_queue = (mbuf_queue_t *)
					alloc_block(sizeof(struct mbuf_queue),
						    B_TEMP);
			
				mbuf_queue->title = (char *)
					alloc_block((strlen(string)+1)*
						    sizeof(char),B_TEMP);
				strcpy(mbuf_queue->title,string);
				mbuf_queue->mbuf = KPvoid;
				ENQUEUE(&head_mbuf_queue,mbuf_queue);
			}

			Pmbstat->m_mbufs += 
				KL_UINT(K,Pmb,"mb","mb_smltot") + 
				KL_UINT(K,Pmb,"mb","mb_medtot") + 
				KL_UINT(K,Pmb,"mb","mb_bigtot");
			
			Pmbstat->m_pcbtot += KL_UINT(K,Pmb,"mb","mb_pcbtot");
			Pmbstat->m_pcbbytes += 
				KL_UINT(K,Pmb,"mb","mb_pcbbytes");
			Pmbstat->m_mcbtot += 
				KL_UINT(K,Pmb,"mb","mb_mcbtot");
			Pmbstat->m_mcbfail += 
				KL_UINT(K,Pmb,"mb","mb_mcbfail");
			Pmbstat->m_mcbbytes += 
				KL_UINT(K,Pmb,"mb","mb_mcbbytes");

			Pmbstat->m_drops += 
				KL_UINT(K,Pmb,"mb","mb_drops");
			Pmbstat->m_wait  += 
				KL_UINT(K,Pmb,"mb","mb_wait"); 

			for (j = MT_DATA; j < MT_MAX; j++)
			{
				Pmbstat->m_mtypes[j] += *(int *)
					((__psunsigned_t)
					 ADDR(K,Pmb,"mb","mb_types") + 
					 (__psunsigned_t)
					 (j * sizeof(int)));
			}
			
			Pmbstat->m_mtypes[MT_FREE] += 
				smlfree + medfree + bigfree;
		}
		if(firsttime)
		{
			fprintf(cmd.ofp,
				"======="
				"==================================\n");
			firsttime = 0;
		}

		/*
		 * Find all vsockets with mbufs in them.
		 */
		find_all_vsockets(cmd.flags,cmd.ofp,test_vfile_mbufsocket,
				  &Pvsockets_hashbase);
		sp = alloc_block(SOCKET_SIZE(K), B_TEMP);
		rcvp = (k_ptr_t)((uint)sp + FIELD("socket", "so_rcv"));
	        sndp = (k_ptr_t)((uint)sp + FIELD("socket", "so_snd"));
		
		for(i=0;i<VFILE_HASHSIZE;i++)
		{
			while(Pvsockets_hashbase[i])
			{
				list=(list_of_ptrs_t *)
					DEQUEUE(&Pvsockets_hashbase[i]);
				get_socket(list->val64, sp, cmd.flags);
				K_BLOCK_FREE(K)(NULL,list);
				/* Receive mbufs on the socket.*/
				if(KPvoid=kl_kaddr(K,rcvp,"sockbuf","sb_mb"))
				{
					if(!firsttime)
					{
						firsttime++;
						fprintf(cmd.ofp,
							"SOCKET MBUF LIST:\n");
						fprintf(cmd.ofp,
							"             MBUF"
							"            SOCKET"
							"  TYPE    COUNT\n");
						fprintf(cmd.ofp,
							"==================="
							"========"
							"==================="
							"======\n");
					}
					fprintf(cmd.ofp," %16llx  %16llx"
						"  RECV-Q   %4d\n",
						KPvoid,list->val64,
						icrash_mbuf_count(KPvoid,
								  mbufconst.
								  m_msize));
					sprintf(string,"Receive mbuf list"
						" for socket 0x%llx",
						list->val64);
					mbuf_queue = (mbuf_queue_t *)
						alloc_block(sizeof(struct mbuf_queue),
							    B_TEMP);
					mbuf_queue->title = 
						(char *)alloc_block(
							(strlen(string)+1)*
							sizeof(char),B_TEMP);
					strcpy(mbuf_queue->title,string);
					mbuf_queue->mbuf = KPvoid;
					ENQUEUE(&head_mbuf_queue,mbuf_queue);
				}
				/* Transmit mbufs on the socket.*/
				if(KPvoid=kl_kaddr(K,sndp,"sockbuf","sb_mb"))
				{
					if(!firsttime)
					{
						firsttime++;
						fprintf(cmd.ofp,
							"SOCKET MBUF LIST:\n");
						fprintf(cmd.ofp,
							"             MBUF"
							"            SOCKET"
							"  TYPE   COUNT\n");
						fprintf(cmd.ofp,
							"==================="
                                                        "========"
							"==================="
							"======\n");
					}
					fprintf(cmd.ofp," %16llx  %16llx"
						"  SEND-Q   %4d\n",
						KPvoid,list->val64,
						icrash_mbuf_count(KPvoid,
								  mbufconst.
								  m_msize));

					sprintf(string,"Transmit mbuf list"
						" for socket 0x%llx",
						list->val64);
					mbuf_queue = (mbuf_queue_t *)
						alloc_block(sizeof(struct mbuf_queue),
							    B_TEMP);
					mbuf_queue->title = 
						(char *)alloc_block(
							(strlen(string)+1)*
							sizeof(char),B_TEMP);
					strcpy(mbuf_queue->title,string);
					mbuf_queue->mbuf = KPvoid;
					ENQUEUE(&head_mbuf_queue,mbuf_queue);
				}
			}
		}
		if(firsttime) 
		{
			fprintf(cmd.ofp,
				"==================="
				"========"
				"==================="
				"======\n");
			firsttime = 0;
		}

		K_BLOCK_FREE(K)(NULL,sp);
		K_BLOCK_FREE(K)(NULL,Pvsockets_hashbase);
		while(mbuf_queue = (mbuf_queue_t *)
		      DEQUEUE(&head_mbuf_queue))
		{
			/*
			 * We will find some use for these queued mbuf's later.
			 */
			K_BLOCK_FREE(K)(NULL,mbuf_queue->title);
			K_BLOCK_FREE(K)(NULL,mbuf_queue);
		}

		/*
		 * Now handle the mbufs in the ip input queue.
		 * These are obsoleted by the below netproc_q's.. It is 
		 * still kept around mainly for nostalgic reasons.
		 * It will be gone very soon from the kernel...and consequently
		 * from icrash too.
		 */
		if((ifqueue_size=kl_struct_len(K,"ifqueue")) &&
		   ((KPvoid = kl_sym_addr(K,"ipintrq")) > 0))
		{
			Pvoid  = alloc_block(ifqueue_size,B_TEMP);
			
			if(kl_get_struct(K,KPvoid,ifqueue_size,
					  Pvoid,"ifqueue"))
			{
				print_mbufs_ifq(KPvoid,Pvoid,
						(SMAJOR|BANNER),
						cmd.ofp,"ipintrq",&firsttime);
			}
			K_BLOCK_FREE(K)(NULL,Pvoid);
		}

		/*
		 * Now handle the mbufs in the input q's of the netproc's.
		 */
		if((size=kl_struct_len(K,"per_netproc")) &&
		   ((KPvoid = kl_sym_pointer(K,"netproc_data")) > 0))
		{
			Pvoid = alloc_block(size,B_TEMP);

			if (K_SYM_ADDR(K)((void*)NULL, "max_netprocs", &next) 
			    >= 0) 
			{
				kl_get_block(K, next, 4, &j, "max_netprocs");
			}
			else
			{
				j=K_NUMNODES(K);
			}
			for(i=0;i<j;i++)
			{
				/*
				 * Cycle through each netproc 
				 */
				if(kl_get_kaddr(K,KPvoid,&KPipq,"per_netproc")
				   && kl_get_struct(K,KPipq,size,Pvoid,
						    "per_netproc"))
				{
					/*
					 * We can do this as the ifqueue
					 * structure is the first one in 
					 * the per_netproc structure.
					 */
					sprintf(string,"netproc%-2d",i);
					print_mbufs_ifq(
						KPipq,Pvoid,
						(SMAJOR|BANNER),
						cmd.ofp,string,&firsttime);
				}
				KPvoid += K_NBPW(K);
			}
			K_BLOCK_FREE(K)(NULL,Pvoid);
		}
		/*
		 * Now handle the mbufs in the nfs q's.
		 */
		if((size=kl_struct_len(K,"pernfsq")) &&
		   ((KPvoid = kl_sym_pointer(K,"nfsq_table")) > 0))
		{
			Pvoid = alloc_block(size,B_TEMP);
			Pmbuf  = alloc_block(mbufconst.m_msize,B_TEMP);

			j=K_NUMNODES(K);
			for(i=0;i<j;i++)
			{
				/*
				 * Cycle through each nfstable 
				 */
				if(kl_get_kaddr(K,KPvoid,&KPipq,"pernfsq")
				   && kl_get_struct(K,KPipq,size,Pvoid,
						    "pernfsq"))
				{
					/*
					 * We can do this as the ifqueue
					 * structure is the first one in 
					 * the pernfsq structure.
					 */
					print_mbufs_ifq(
						KPipq,Pvoid,
						(SMAJOR|BANNER),
						cmd.ofp,"pernfsq",&firsttime);
				}
				KPvoid += K_NBPW(K);
			}
			K_BLOCK_FREE(K)(NULL,Pmbuf);
			K_BLOCK_FREE(K)(NULL,Pvoid);
		}

		if(firsttime)
		{
			firsttime = 0;
			ifq_banner(cmd.ofp,SMAJOR);
		}

		/*
		 * Now handle the mbufs in the ip fragmented queue.
		 */
		if((size=kl_struct_len(K,"ipfrag_bucket")) &&
		   ((KPvoid = kl_sym_addr(K,"ipfrag_qs")) > 0))
		{
			Pvoid  = alloc_block(size,B_TEMP);
			Pipq   = alloc_block(kl_struct_len(K,"ipq"),B_TEMP);
			Pmbuf  = alloc_block(mbufconst.m_msize,B_TEMP);
			if(PLATFORM_SNXX || PLATFORM_EVEREST)
			{
				frag_buckets = 16;
			}
			else
			{
				frag_buckets = 4;
			}
			for(i=0;i<frag_buckets;i++)
			{
				if(!kl_get_struct(K,KPvoid,size,Pvoid,
						  "ipfrag_bucket"))
				{
					continue;
				}
				KPipq = KPbucketipq = KPvoid + 
					FIELD("ipfrag_bucket","ipq");
				Pipq_next = (k_ptr_t)((__psunsigned_t)Pvoid +
						      FIELD("ipfrag_bucket",
							    "ipq"));
				while((KPipq=kl_kaddr(K,Pipq_next,"ipq",
						      "next")) !=  KPbucketipq)
				{
					/*
					 * This is a single fragment.
					 * Find the mbufs here and print them.
					 * Maybe just print the mbufs in each
					 * sub-fragment... i.e, don't follow
					 * the m_next ...
					 */
					if(KPmbuf = kl_kaddr(K,Pipq_next,
							      "ipq",
							      "ipq_mbuf"))
					{
						while(KPmbuf && kl_get_struct(
							K,KPmbuf,
							mbufconst.m_msize,
							Pmbuf,"mbuf"))
						{
							if(!firsttime)
							{
								firsttime++;
								fprintf(cmd.ofp,
									"IP FR"
									"AGMEN"
									"TATIO"
									"N QUE"
									"UE"
									"\n");
								fprintf(cmd.ofp,
									"     "
									"     "
									"   "
									"MBUF "
									"COUNT"
									"\n");
								fprintf(cmd.ofp,
									"====="
									"====="
									"====="
									"====="
									"==="
									"\n");
							}
							fprintf(cmd.ofp,
								"0x%16llx  %4d"
								"\n",
								KPmbuf,
								icrash_mbuf_count(KPmbuf,mbufconst.m_msize));
							
							KPmbuf = kl_kaddr(
								K,Pmbuf,
								"mbuf",
								"m_act");
						}
					}
					if(!kl_get_struct(
						K,KPipq,kl_struct_len(K,"ipq"),
						Pipq,"ipq"))
					{
						break;
					}
					Pipq_next = Pipq;
				}
				KPvoid += size;
			}
			K_BLOCK_FREE(K)(NULL,Pmbuf);
			K_BLOCK_FREE(K)(NULL,Pipq);
			K_BLOCK_FREE(K)(NULL,Pvoid);
		}
		if(firsttime)
		{
			fprintf(cmd.ofp,
				"=======================\n");
		}

		/*
		 * Now print out the summary of the mbufs..
		 */
		GET_UINT("mbpcbtot",  KPvoid,i,0);
		GET_UINT("mcbtot",    KPvoid,j,0);
		Pmbstat->m_mbufs += i + j;

		fprintf(cmd.ofp,"SUMMARY:\n");

		GET_UINT("mbpages",   KPvoid,i,0);
		Pmbstat->m_clusters = i;
		GET_UINT("mbpcbbytes",KPvoid,i,0);
		GET_UINT("mcbbytes",  KPvoid,j,0);
		Pmbstat->m_clusters += (i + j)/K_PAGESZ(K);
		Pmbstat->m_clfree = bytesfree/K_NBPC(K);
		GET_UINT("mbdrain",   KPvoid,i,0);
		Pmbstat->m_drain = i;
				
		nmbtypes = sizeof(Pmbstat->m_mtypes) / 
			sizeof(Pmbstat->m_mtypes[0]);
		fprintf(cmd.ofp,
			"%d/%d mbufs in use:\n",
			(int)(Pmbstat->m_mbufs - Pmbstat->m_mtypes[MT_FREE]), 
			(int)Pmbstat->m_mbufs);
		totmbufs = 0;

		for (mp = mbtypes; mp->mt_name; mp++) 
		{
			if (mp->mt_flag == MBUF) 
			{
				if (Pmbstat->m_mtypes[mp->mt_type]) 
				{
					seen[mp->mt_type] = TRUE;
					fprintf(cmd.ofp, 
						"\t%u %s%s allocated\n",
						(int)
						Pmbstat->m_mtypes[mp->mt_type],
						mp->mt_name,
						mp->mt_func(Pmbstat->m_mtypes[mp->mt_type]));
					totmbufs += 
						Pmbstat->m_mtypes[mp->mt_type];
				}
			}
		}

		fprintf(cmd.ofp,
			"%d other structure%s in use\n",
			(int)(Pmbstat->m_pcbtot + Pmbstat->m_mcbtot),
			plural((int)(Pmbstat->m_pcbtot + Pmbstat->m_mcbtot)));
			
		for (mp = mbtypes; mp->mt_name; mp++)
		{
			if (mp->mt_flag == OTHER)
			{
				if (Pmbstat->m_mtypes[mp->mt_type])
				{
					seen[mp->mt_type] = TRUE;
					fprintf(cmd.ofp,
						"\t%u %s%s allocated\n",
						(int)
						Pmbstat->m_mtypes[mp->mt_type],
						mp->mt_name,
						mp->mt_func(Pmbstat->m_mtypes[mp->mt_type]));
					totmbufs +=
						Pmbstat->m_mtypes[mp->mt_type];
				}
			}
		}
				
		seen[MT_FREE] = TRUE;
		for (i = MT_FREE; i < nmbtypes; i++) {
			if (!seen[i] && Pmbstat->m_mtypes[i]) {
				fprintf(cmd.ofp, 
					"\t%u <type %d> allocated\n",
					i,(int)Pmbstat->m_mtypes[i]);
				totmbufs += Pmbstat->m_mtypes[i];
			}
		}

		
		totmem = Pmbstat->m_clusters * mbufconst.m_mclbytes;
		mcbpages = ROUNDUP(Pmbstat->m_mcbbytes, mbufconst.m_mclbytes);
		pcbpages = ROUNDUP(Pmbstat->m_pcbbytes, mbufconst.m_mclbytes);
		totmem += pcbpages + mcbpages;
		pcbpages /= mbufconst.m_mclbytes;
		mcbpages /= mbufconst.m_mclbytes;
		
		totfree = Pmbstat->m_clfree * mbufconst.m_mclbytes;

		totpages  = pcbpages + mcbpages + Pmbstat->m_clusters;
		freepages = Pmbstat->m_clfree;
			
		fprintf(cmd.ofp,
			"%u total page%s allocated to networking data\n",
			totmem / mbufconst.m_mclbytes,
			plural(totmem / mbufconst.m_mclbytes));

		fprintf(cmd.ofp, "\t%u/%u mapped mbuf pages in use\n",
			(unsigned)(Pmbstat->m_clusters - Pmbstat->m_clfree), 
			(unsigned)Pmbstat->m_clusters);

		fprintf(cmd.ofp,"\t%u page%s of PCBs in use\n",
			pcbpages, plural(pcbpages));

	        fprintf(cmd.ofp,
			"\t%u page%s of other networking data in use\n",
			mcbpages, plural(mcbpages));
		

		fprintf(cmd.ofp, 
			"\t%u Kbytes allocated to network (%d%% in use)\n",
			totmem / 1024, (totpages-freepages) * 100 / totpages);
		fprintf(cmd.ofp, 
			"%u request%s for memory denied\n", 
			(unsigned)Pmbstat->m_drops,
			plural(Pmbstat->m_drops));
		fprintf(cmd.ofp, 
			"%u request%s for memory delayed\n", 
			(unsigned)Pmbstat->m_wait,
			plural(Pmbstat->m_wait));
		fprintf(cmd.ofp, 
			"%u call%s to protocol drain routines\n", 
			(unsigned)Pmbstat->m_drain,
			plural(Pmbstat->m_drain));
		fprintf(cmd.ofp,"%llu request%s for non-mbuf memory denied\n", 
		       Pmbstat->m_mcbfail,
		       plural(Pmbstat->m_mcbfail));

	       /*
		* Ok, Time to go home.
		*/
		K_BLOCK_FREE(K)(NULL,Pmbcache);
		K_BLOCK_FREE(K)(NULL,Pmbstat);
		K_BLOCK_FREE(K)(NULL,Pmb);

	}
	else if (cmd.nargs > 0) 
	{
		m = alloc_block(MBUF_SIZE(K), B_TEMP);

		if (!(cmd.flags & C_FULL)) {
			mbuf_banner(cmd.ofp, BANNER|SMAJOR);
		}

		for (i = 0; i < cmd.nargs; i++) {
			if (cmd.flags & C_FULL) {
				mbuf_banner(cmd.ofp, BANNER|SMAJOR);
			}
			GET_VALUE(cmd.args[i], &mbuf);
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_MBUF;
				kl_print_error(K);
				continue;
			}
			kl_get_struct(K, mbuf, MBUF_SIZE(K), m, "mbuf");
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_MBUF;
				kl_print_error(K);
			}
			else {
				print_mbuf(mbuf, m, cmd.flags, cmd.ofp);
				mbuf_cnt++;
				if (cmd.flags & C_NEXT) {
					next = kl_kaddr(K, m, "mbuf",
							"m_next");
					while (next) {
						kl_get_struct(K, next,
							      MBUF_SIZE(K), m,
							      "mbuf");
						if (KL_ERROR) {
							if (DEBUG(DC_GLOBAL, 1))
							{
								kl_print_debug(K, "mbuf_cmd");
							}
							break;
						}
						if (cmd.flags & C_FULL) {
							fprintf(cmd.ofp, "\n");
							mbuf_banner(cmd.ofp, 
								    BANNER|
								    SMINOR);
						}
						print_mbuf(next, m, cmd.flags, 
							   cmd.ofp);
						mbuf_cnt++;
						next = kl_kaddr(K, m, "mbuf", 
								"m_next");
					}
				}
				if (cmd.flags & C_FULL) {
					fprintf(cmd.ofp, "\n");
				}
			}
		}
		mbuf_banner(cmd.ofp, SMAJOR);
		PLURAL("mbuf struct", mbuf_cnt, cmd.ofp);
		free_block(m);
	}
	return(0);
}

#define _MBUF_USAGE "[-n] [-w outfile] [-a | -f mbuf_list]"

/*
 * mbuf_usage() -- Print the usage string for the 'mbuf' command.
 */
void
mbuf_usage(command_t cmd)
{
	CMD_USAGE(cmd, _MBUF_USAGE);
}

/*
 * mbuf_help() -- Print the help information for the 'mbuf' command.
 */
void
mbuf_help(command_t cmd)
{
	CMD_HELP(cmd, _MBUF_USAGE,
		 "The -f option will display the mbuf structure located at "
		 "each virtual address included in mbuf_list.\n"
		 "The -a option will display a summary of mbuf usage and "
		 "the following type of mbufs:\n"
		 "\tFree mbufs from the mb structure.\n"
		 "\tMbufs found on sockets.\n"
		 "\tMbufs found on the nfs queue.\n"
		 "\tMbufs found on the ip fragmentation queue.\n"
		 "\tMbufs found on the netproc and ip input queues.\n");

}

/*
 * mbuf_parse() -- Parse the command line arguments for 'mbuf'.
 */
int
mbuf_parse(command_t cmd)
{
	return (C_WRITE|C_FULL|C_ALL|C_NEXT|C_MAYBE);
}
