/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995 Silicon Graphics, Inc.        *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.2 $"

/*
 *	This tool provides information about NUMA placement
 *	structures for a given process. Information is obtained
 *	from the kernel through the ioctl /proc channel using
 *	some of the CPR specific codes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <strings.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <invent.h>
#include <sys/procfs.h>
#include <sys/ckpt_procfs.h>
#include <sys/pmo.h>

/*
 *	Data structures and macros
 */
typedef struct set_info {
	int			mldset_size;	/* size of mld's array */
	void			*mldset;	/* array of mld's */
	mldset_placement_info_t	placement;	/* mldset placement info */
} set_info_t;

typedef struct numa_info {
	struct numa_info	*next;		/* next list element */
	int			id;		/* id / handle of element */
	int			type;		/* type of element */
	int			size;		/* size of data buffer */
	void			*addr;		/* address of data buffer */
} numa_info_t;

typedef struct mem_info {
	int			rsize;		/* size of mem info array */
	void			*raddr;		/* mem map info array */
	int			pmsize;		/* size of page info array */
	void			*pmaddr;	/* page info array */
} mem_info_t;

typedef	struct proc_info {
	int			pid;		/* process id */
	int			fd;		/* fd to /proc */
	prpsinfo_t		binfo;		/* basic info on process */
	ckpt_psi_t		mcinfo;		/* misc info on process */
	numa_info_t		*ninfo;		/* head of numa info on proc */
	mem_info_t		minfo;		/* memory information */
} proc_info_t;

proc_info_t	proc;				/* process information */
int		affnode;			/* affinity node for process */
int		bug=0;				/* debugging only - indicates algorithm failure */
int		f_w=0;				/* flag memory not on nodes specified
						   by mld */

/*
 *	The following macros provide some basic operation on
 *	the above structures.
 *
 *	NEW_NUMA	constructor
 *	ADD_NUMA	add to list
 *	FIND_NUMA	locate in list
 *	MY_NUMA		select by type
 *	NEXT_NUMA	list iterator
 */
#define	NEW_NUMA(P,ID,TYPE,SIZE) {					\
	P = (numa_info_t*)malloc(sizeof(numa_info_t));			\
	assert(P);							\
	P->next = NULL;							\
	P->id = (ID);							\
	P->type = (TYPE);						\
	P->size = (SIZE);						\
	P->addr = (void*)malloc((SIZE));				\
	memset(P->addr, 0, (SIZE));					\
	assert(P->addr);						\
}

#define	ADD_NUMA(P) {							\
	numa_info_t	*N;						\
									\
	if (proc.ninfo) {						\
		N = proc.ninfo;						\
		while (N->next)		N = N->next;			\
		N->next = (P);						\
	} else {							\
		proc.ninfo = (P);					\
	}								\
}

#define	FIND_NUMA(P, ID) {						\
	numa_info_t	*N;						\
									\
	N = proc.ninfo;							\
	while (N && N->id != (ID))	N = N->next;			\
	P = N;								\
}

#define	MY_NUMA(P,TYPE) ((P)->type == (TYPE))

#define	NEXT_NUMA(N)	(N = (N) == NULL ? proc.ninfo : (N)->next)


/*
 *	Other definitions
 */
#define	Usage	"Usage: %s [-l] [-a] [-w] pid\n"
#define	CMDOPTS	"alw"

#define HWGRAPH_NODENUM	"/hw/nodenum"
#define CNODEID_NONE	(cnodeid_t)-1
#define	PG_BUF_SIZE	100

#define	BUMMER	"Unable to obtain memory management status for process"

#define	PMO_H1	"POLICY Placement MLD/SET  Pagesize Flags"
#define	PMO_H2	"====== ========= ======== ======== =========================="

#define	SET_H1	"MLDSET Topology Mode      Resource Affinity"
#define SET_H2 \
"====== ======== ========= =================================="
#define	SET_H3	"                        "

#define	MLD_H1	"MLD    MLDSET Radius Size     MLDlink  Location"
#define MLD_H2 \
"====== ====== ====== ======== ======== =================================="

#define	MEM_H1	\
"       Address Range          Mode Pages  Size  Policy Location"  
#define	MEM_H2	\
"============================= ==== ===== ====== ====== ========================="



/*
 *	Print usage message
 */
void
usage(char *name)
{
	fprintf(stderr, Usage, name);
}


/*
 *	Find associated mldset for given mld
 */
numa_info_t*
find_mldset(int mld)
{
	int		i, mlds;
	set_info_t	*mldset;
	pmo_handle_t	*mldlist;
	numa_info_t	*cn;

	cn = NULL;
	while (NEXT_NUMA(cn)) {
		if (!MY_NUMA(cn, __PMO_MLDSET))	continue;

		mldset = (set_info_t*)cn->addr;
		mlds = mldset->mldset_size;
		mldlist = mldset->mldset;
		for (i = 0; i < mlds; i++) {
			if (mldlist[i] == mld)
				return cn;
		}
	}
	return 0;
}


/*
 *	Find associated memory region
 */
prmap_sgi_t*
find_region(pm_pginfo_t *cp)
{
	prmap_sgi_t	*cr;
	int		i;

	for (i = 0, cr = (prmap_sgi_t*)proc.minfo.raddr; 
	     i < proc.minfo.rsize; i++, cr++) {
		if (cp->vaddr >= cr->pr_vaddr &&
		    cp->vaddr < (cr->pr_vaddr + cr->pr_size)) {
			return cr;
		}
	}
	return NULL;
}


/*
 *	Return node name
 */
char*
nodename(int n)
{
	static char	nn[PATH_MAX];
	char		name[PATH_MAX];
	int		num;

	if (n == CNODEID_NONE)	return "-";

	sprintf(nn, "%s/%d", HWGRAPH_NODENUM, n);
	if ( (num=readlink(nn, name, PATH_MAX)) > 0) {
		name[num] = '\0';
		sprintf(nn,"%d = %s",n, &name[4]);
	}

	return nn;
}


/*
 *	Retrieve information about the process from the kernel
 */
int
get_proc(int pid)
{
	char		cpid[PATH_MAX];

	/*
	 * General initialization
	 */
	memset(&proc, 0, sizeof(proc));
	proc.pid = pid;

	/*
	 * Open /proc
	 */
	sprintf(cpid, "/proc/%d", proc.pid);
	if ((proc.fd = open(cpid, O_RDONLY)) < 0) {
		perror(cpid);
		return 1;
	}

	/*
	 * Get basic proc information
	 */
	if (ioctl(proc.fd, PIOCPSINFO, &proc.binfo) < 0) {
		perror("ioctl(misc proc info)");
		return 1;
	}

	/*
	 * Get misc proc information
	 */
	if (ioctl(proc.fd, PIOCCKPTPSINFO, &proc.mcinfo) < 0) {
		perror("ioctl(misc proc info)");
		return 1;
	}
	return 0;
}


/*
 *	Retrieve information on policy modules from kernel
 */
int
get_pmos(void)
{
	numa_info_t		*cn;
	pmo_handle_t		ch;
	pmo_handle_t		pmohandle;
	pm_info_t		pmoinfo;
	pm_info_t		*ci;

	ckpt_getpmonext_t	ckpmonext;
	ckpt_pminfo_arg_t	ckpmoinfo;

	/*
	 * Scan through all policies
	 */
	ckpmonext.pmo_type = __PMO_PM;
	ckpmonext.pmo_handle = 0;

	ckpmoinfo.ckpt_pminfo = &pmoinfo;
	ckpmoinfo.ckpt_pmo = &pmohandle;

	ch = 0;
	while(ch >= 0) {

		/*
		 * Get handle to policy module
		 */
		ch = ioctl(proc.fd, PIOCCKPTPMOGETNEXT, &ckpmonext);
		if (ch < 0) {
			if (oserror() != ESRCH) {
				perror("ioctl(next pmo)");
				return 1;
			}
			break;
		}

		/*
		 * Get actual policy module
		 */
		ckpmoinfo.ckpt_pmhandle = ch;
		if (ioctl(proc.fd, PIOCCKPTPMINFO, &ckpmoinfo) < 0) {
			perror("ioctl(read pmo)");
			return 1;
		}

		/*
		 * Add to numa list
		 */
		NEW_NUMA(cn, ch, __PMO_PM, sizeof(pm_info_t));
		ci = (pm_info_t*)cn->addr;
		*ci = pmoinfo;
		ADD_NUMA(cn);

		/*
		 * Advance
		 */
		ckpmonext.pmo_handle = ch + 1;
	}

	return 0;
}


/*
 *	Translate filedescriptor to name
 */
char*
get_raff_name(int fd)
{
	char		*rv = 0;
	char		*cwd;
	struct stat	sbuf;

	if (fstat(fd, &sbuf) < 0)	return 0;

	switch (sbuf.st_mode & S_IFMT) {
	case S_IFDIR:
		/*
		 * We can recover names of directories.
		 * Fortunatly, files in /hw/module/... are directories.
		 */
		cwd = getcwd(NULL, PATH_MAX);
		if (cwd == NULL)	break;
		if (fchdir(fd) < 0)	break;
		rv = getcwd(NULL, PATH_MAX);
		chdir(cwd);
		free(cwd);
		break;
	default:
		/*
		 * For now and here: ignore all other cases
		 */
		return 0;
	}
	return rv;
}

/*
 *	Retrieve information on affinitiy resources from the kernel
 */
int
get_raff(int fd, pmo_handle_t mldset, raff_info_t *raff, int len)
{
	int		i, raff_fd;
	raff_info_t	*cr;
	ckpt_raffopen_t	raffopen;

	raffopen.mldset_handle = mldset;

	for (i = 0, cr = raff; i < len; i++, cr++) {
		raffopen.mldset_element = i;

		raff_fd = ioctl(fd, PIOCCKPTRAFFOPEN, &raffopen);
		if (raff_fd < 0)	continue;

		cr->resource = get_raff_name(raff_fd);
		if (cr->resource) {
			cr->reslen = (ushort)strlen(cr->resource);
			cr->restype = RAFFIDT_NAME;
		}

		close(raff_fd);
	}

	return 0;
}


/*
 *	Retrieve information on mldset's from kernel
 */
int
get_mldsets(void)
{
	int			i;
	numa_info_t		*cn;
	pmo_handle_t		ch;
	set_info_t		*cs;

	ckpt_getpmonext_t	ckpmonext;
	ckpt_mldset_info_arg_t	ckmldset;
	mldset_placement_info_t ckplace;


	/*
	 * Scan through all mldsets
	 */
	ckpmonext.pmo_type = __PMO_MLDSET;
	ckpmonext.pmo_handle = 0;

	ch = 0;
	while(ch >= 0) {

		/*
		 * Get handle to mldset
		 */
		ch = ioctl(proc.fd, PIOCCKPTPMOGETNEXT, &ckpmonext);
		if (ch < 0) {
			if (oserror() != ESRCH) {
				perror("ioctl(next mldset)");
				return 1;
			}
			break;
		}

		/*
		 * Add mldset to numa list
		 */
		NEW_NUMA(cn, ch, __PMO_MLDSET, sizeof(set_info_t));
		ADD_NUMA(cn);

		/*
		 * Get mld list associated with mldset
		 */
		ckmldset.mldset_handle = ch;
		ckmldset.mldset_info.mldlist_len = 0;
		ckmldset.mldset_info.mldlist = NULL;
		i = ioctl(proc.fd, PIOCCKPTMLDSETINFO, &ckmldset);
		if (i < 0) {
			perror("ioctl(size mldset)");
			return 1;
		}
		assert (i > 0);

		ckmldset.mldset_info.mldlist_len = i;
		ckmldset.mldset_info.mldlist = 
			(pmo_handle_t *)malloc(i * sizeof(pmo_handle_t));
		assert(ckmldset.mldset_info.mldlist);
		i = ioctl(proc.fd, PIOCCKPTMLDSETINFO, &ckmldset);
		if (i < 0) {
			perror("ioctl(read mldset)");
			return 1;
		}

		/*
		 * Add mld list to mldset
		 */
		if (i == ckmldset.mldset_info.mldlist_len) {

			cs = (set_info_t*)cn->addr;
			cs->mldset_size = i;
			cs->mldset = ckmldset.mldset_info.mldlist;
		}

		/*
		 * Get placement information
		 */
		ckplace.mldset_handle = ch;
		ckplace.rafflist = NULL;
		ckplace.rafflist_len = 0;

		i = ioctl(proc.fd, PIOCCKPTMLDPLACEINFO, &ckplace);
		if (i < 0) {
			perror("ioctl(read placement)");
			return 1;

		}
		ckplace.rafflist_len = i;

		/*
		 * Add placement to mldset
		 */
		if (ckplace.mldset_handle == ch) {
			cs = (set_info_t*)cn->addr;
			cs->placement = ckplace;
			cs->placement.rafflist = 0;
			cs->placement.rafflist_len = 0;
		}

		/*
		 * Get affinity resource information
		 */
		if (ckplace.rafflist_len > 0) {

			ckplace.rafflist = (raff_info_t *)malloc(
						ckplace.rafflist_len * 
						sizeof(raff_info_t));

			i = ioctl(proc.fd, PIOCCKPTMLDPLACEINFO, &ckplace);
			if (i != ckplace.rafflist_len) {

				/*
				 * The resource information has changed
				 * (or we couldn't read it);
				 * just ignore it.
				 */
				free(ckplace.rafflist);
				ckplace.rafflist_len = 0;
			} else {
				get_raff(proc.fd, ch,
					ckplace.rafflist,
					ckplace.rafflist_len);
				cs->placement.rafflist = 
					ckplace.rafflist;
				cs->placement.rafflist_len = 
					ckplace.rafflist_len;
			}
		}

		/*
		 * Advance
		 */
		ckpmonext.pmo_handle = ch + 1;
	}

	return 0;
}


/*
 *	Retrieve information on mld's from kernel
 */
int
get_mlds(void)
{
	numa_info_t		*cn;
	pmo_handle_t		ch;
	ckpt_mldinfo_t		mldinfo;
	ckpt_mldinfo_t		*ci;

	ckpt_getpmonext_t	ckmldnext;
	ckpt_mldinfo_arg_t	ckmldinfo;

	/*
	 * Scan through all mlds
	 */
	ckmldnext.pmo_type = __PMO_MLD;
	ckmldnext.pmo_handle = 0;

	ckmldinfo.ckpt_mldinfo = &mldinfo;

	ch = 0;
	while(ch >= 0) {

		/*
		 * Get handle to mld
		 */
		ch = ioctl(proc.fd, PIOCCKPTPMOGETNEXT, &ckmldnext);
		if (ch < 0) {
			if (oserror() != ESRCH) {
				perror("ioctl(next mld)");
				return 1;
			}
			break;
		}

		/*
		 * Get actual mld
		 */
		ckmldinfo.ckpt_mldhandle = ch;
		if (ioctl(proc.fd, PIOCCKPTMLDINFO, &ckmldinfo) < 0) {
			perror("ioctl(read pmo)");
			return 1;
		}

		/*
		 * Add to numa list
		 */
		NEW_NUMA(cn, ch, __PMO_MLD, sizeof(ckpt_mldinfo_t));
		ci = (ckpt_mldinfo_t*)cn->addr;
		*ci = mldinfo;
		ADD_NUMA(cn);

		/*
		 * Advance
		 */
		ckmldnext.pmo_handle = ch + 1;
	}
	return 0;
}


/*
 *	Get memory status
 */
int
get_mem(void)
{
	int			i, j;
	prmap_sgi_arg_t		mdesc;
	prmap_sgi_t		*cr;
	pm_pginfo_t		pg_buf, *cp;
	ckpt_pm_pginfo_t	pdesc;
	void			*start, *end, *caddr;

	/*
	 * Get number of memory mappings
	 */
	if (ioctl(proc.fd, PIOCNMAP, &i) < 0) {
		perror("ioctl(read nmaps)");
		return 1;
	}
	if (i <= 0)	return 0;

	/*
	 * Get region maps
	 */
	proc.minfo.rsize = i + 10;		/* fudge */
	proc.minfo.raddr = (void*)malloc(proc.minfo.rsize * 
					sizeof(prmap_sgi_t));
	assert(proc.minfo.raddr);

	mdesc.pr_size =  proc.minfo.rsize * sizeof(prmap_sgi_t);
	mdesc.pr_vaddr = proc.minfo.raddr;
	i = ioctl(proc.fd, PIOCMAP_SGI, &mdesc);
	if (i < 0) {
		perror("ioctl(read info)");
		proc.minfo.rsize = 0;
		free(proc.minfo.raddr);
		return 1;
	} else {
		proc.minfo.rsize = i;
	}

	/*
	 * Get page information
	 */
	proc.minfo.pmsize = 0;
	proc.minfo.pmaddr = NULL;
	cr = (prmap_sgi_t*)proc.minfo.raddr;
	for (i = 0; i < proc.minfo.rsize; i++, cr++) {

		if (cr->pr_vsize == 0)	continue;

		caddr = start = cr->pr_vaddr;
		end = cr->pr_vaddr + cr->pr_size;

		while (caddr < end) {

			pdesc.vrange.base_addr = start;
			pdesc.vrange.length = cr->pr_size;
			pdesc.pginfo_list.pm_pginfo = &pg_buf;
			pdesc.pginfo_list.length = 1;

			j = ioctl(proc.fd, PIOCCKPTPMPGINFO, &pdesc);
			if (j < 0) {
				perror("ioctl(read page info)");
				proc.minfo.rsize = 0;
				free(proc.minfo.raddr);
				proc.minfo.pmsize = 0;
				if (proc.minfo.pmaddr) free(proc.minfo.pmaddr);
				return 1;
			}
			proc.minfo.pmaddr = (pm_pginfo_t*)realloc(
						proc.minfo.pmaddr,
						(1 + proc.minfo.pmsize) *
						sizeof(pm_pginfo_t));
			cp = (pm_pginfo_t*)proc.minfo.pmaddr +
				proc.minfo.pmsize;
			*cp = pg_buf;
			proc.minfo.pmsize++;
			start = cp->vaddr + cp->page_size;
			if (start <= caddr)	break;
			caddr = start;
		}
	}

	return 0;
}


/*
 *	Print introduction
 */
void
print_intro(void)
{
	printf("Memory Management Status for Process %d\n", proc.pid);
	printf("\tCommand : %s\n",proc.binfo.pr_psargs);
}


/*
 * 	Check to see if the memory is actually allocated on the
 *	node specified by the mld/mldset.
 *	
 *	Returns 0 - correct node, 1 - wrong node
 *	ZZ
 */
int
check_node(int policy, int node)
{
	numa_info_t	*cn, *ai;
	pm_info_t	*ci;
	ckpt_mldinfo_t	*mi;
	numa_info_t	*mymldset;

	cn = NULL;
	while (NEXT_NUMA(cn)) {
		if (!MY_NUMA(cn, __PMO_PM))	continue;
		if (policy != cn->id)		continue;

		ci = (pm_info_t*)cn->addr;

		if (strcmp("Default", &ci->placement_policy_name[9]) == 0) {
			return (node != affnode);
		}

		FIND_NUMA(ai, (long)(ci->placement_policy_args));
		if (!ai) {
			bug++;
			return 0;
		}

		switch (ai->type) {
		case __PMO_MLDSET: 
			cn = NULL;
			while (NEXT_NUMA(cn)) {
				if (!MY_NUMA(cn, __PMO_MLD))	continue;
				mi = (ckpt_mldinfo_t*)cn->addr;
				if (node != mi->mld_nodeid)	continue;
				mymldset = find_mldset(cn->id);
				if (mymldset && mymldset->id == ai->id) 
					return 0;
			}
			return 1;
		case __PMO_MLD:   
			cn = NULL;
			while (NEXT_NUMA(cn)) {
				if (!MY_NUMA(cn, __PMO_MLD))	continue;
				mi = (ckpt_mldinfo_t*)cn->addr;
				return (node != mi->mld_nodeid);
			}
			bug++;
			return 1;
		default:         
			bug++;
			return 1;
		}

	}
	bug++;
	return 1;
}


/*
 *	Print policy module status
 */
void
print_pmos(void)
{
	numa_info_t	*cn, *ai;
	pm_info_t	*ci;
	char		tb[PATH_MAX];
	int		header = 1;
	int		flags, nflags;

	cn = NULL;
	while (NEXT_NUMA(cn)) {
		if (!MY_NUMA(cn, __PMO_PM))	continue;

		if (header) {
			printf("\n%s\n%s\n", PMO_H1, PMO_H2);
			header = 0;
		}

		ci = (pm_info_t*)cn->addr;

		if (strcmp("Default", &ci->placement_policy_name[9]) == 0) {
			sprintf(tb,"THD:%-d",(int)(ci->placement_policy_args));
		} else if (strcmp("FirstTouch", &ci->placement_policy_name[9]) == 0) {
			sprintf(tb,"");
		} else {
			FIND_NUMA(ai, (long)(ci->placement_policy_args));
			if (ai) {
				switch (ai->type) {
				case __PMO_MLDSET: sprintf(tb,"SET:%-d",ai->id); break;
				case __PMO_MLD:    sprintf(tb,"MLD:%-d",ai->id); break;
				default:           strcpy(tb, "-"); break;
				}
			} else {
				strcpy(tb, "-");
			}
		}
		printf("%6d %9s %8s %7dk",
			cn->id,
			&ci->placement_policy_name[9],
			tb,
			(int)(ci->page_size>>10));

		flags = ci->policy_flags;
		if (flags == 0) {
			printf(" -");
		} else {
			nflags = 0;
			strcpy(tb," ");
			if (flags & POLICY_CACHE_COLOR_FIRST) 
				strcat(tb, nflags++ ? ":Color" : "Color");
			if (flags & POLICY_PAGE_ALLOC_WAIT) 
				strcat(tb, nflags++ ? ":PWait" : "PWait");
			if (flags & POLICY_DEFAULT_MEM_STACK) 
				strcat(tb, nflags++ ? ":Stack" : "Stack");
			if (flags & POLICY_DEFAULT_MEM_TEXT) 
				strcat(tb, nflags++ ? ":Text" : "Text");
			if (flags & POLICY_DEFAULT_MEM_DATA) 
				strcat(tb, nflags++ ? ":Data" : "Data");
			printf("%s", tb);
		}
		printf("\n");
	}
}


/*
 *	Print mldset status
 */
void
print_mldsets(void)
{
	numa_info_t	*cn;
	set_info_t	*cs;
	raff_info_t	*cr;
	int		i, indent, header = 1;


	cn = NULL;
	while (NEXT_NUMA(cn)) {
		if (!MY_NUMA(cn, __PMO_MLDSET))	continue;

		if (header) {
			printf("\n%s\n%s\n", SET_H1, SET_H2);
			header = 0;
		}
		printf("%6d",cn->id);

		cs = (set_info_t*)cn->addr;
		if (cs) {
			switch (cs->placement.topology_type) {
			case TOPOLOGY_FREE:       printf("     Free"); break;
			case TOPOLOGY_CUBE:       printf("     Cube"); break;
			case TOPOLOGY_CUBE_FIXED: printf(" CubeFixd"); break;
			case TOPOLOGY_PHYSNODES:  printf(" Physical"); break;
			case TOPOLOGY_CPUCLUSTER: printf(" CPUClust"); break;
			default:                  printf("        ?"); break;
			}

			switch (cs->placement.rqmode) {
			case RQMODE_ADVISORY:     printf("  Advisory"); break;
			case RQMODE_MANDATORY:    printf(" Mandatory"); break;
			default:                  printf("        ?"); break;
			}

			/*
			 * Print affinity resources
			 */
			if (cs->placement.rafflist_len > 0) {
				indent = 0;
				cr = cs->placement.rafflist;
				for (i = 0; i < cs->placement.rafflist_len;
								i++, cr++) {
					if (cr->resource) {
						if (indent)
						printf("%s", SET_H3);
						printf(" %s",
							(char*)cr->resource);
						indent = 1;
					}
				}
			} else {
				printf(" -");
			}
		} else {
			printf("        -");
			printf("        -");
			printf("        -");
		}
		printf("\n");
	}
}


/*
 *	Print mld status
 */
void
print_mlds(void)
{
	numa_info_t	*cn;
	numa_info_t	*mymldset;
	ckpt_mldinfo_t	*ci;
	char		tb[32];
	int		header = 1;

	cn = NULL;
	while (NEXT_NUMA(cn)) {
		if (!MY_NUMA(cn, __PMO_MLD))	continue;

		if (header) {
			printf("\n%s\n%s\n", MLD_H1, MLD_H2);
			header = 0;
		}

		ci = (ckpt_mldinfo_t*)cn->addr;
		mymldset = find_mldset(cn->id);
		if (mymldset) {
			sprintf(tb,"%6d", mymldset->id);
		} else {
			strcpy(tb,"-");
		}
	
		printf("%6d %6s %6d %7dk",
			cn->id,
			tb,
			ci->mld_radius,
			(int)(ci->mld_size>>10));

		if (ci->mld_id == proc.mcinfo.ps_mldlink) {
			affnode = ci->mld_nodeid;
			printf(" %8d", proc.pid);
		} else {
			printf(" %8s", "-");
		}

		printf(" %s", nodename(ci->mld_nodeid));
		printf("\n");
	}
}


/*
 *	Print memory map status
 */
void
print_mem(int all)
{
#define	PRINT_FLAGS(F,M,M2) {						\
	printf("%s", (myreg->pr_mflags & (F)) ? (M) : (M2));		\
}

#define	PRINT_IT {							\
	if (all || (myreg->pr_mflags & MA_EXEC) == 0) {			\
		if (header) {						\
			printf("\n%s\n%s\n", MEM_H1, MEM_H2);		\
			header = 0;					\
		}							\
		printf("%#12.12llx-%#12.12llx ",			\
			(long long)mypage.vaddr,			\
			(long long)mypage.vaddr + 			\
				pages * mypage.page_size);		\
		PRINT_FLAGS(MA_READ,   "r", "-");			\
		PRINT_FLAGS(MA_WRITE,  "w", "-");			\
		PRINT_FLAGS(MA_EXEC,   "x", "-");			\
		PRINT_FLAGS(MA_SHARED, "s", "-");			\
		printf(" %5d", pages);					\
		printf(" %5dk", mypage.page_size >> 10);		\
		printf(" %6d", mypage.pm_handle);			\
		printf(" %s", nodename((int)mypage.node_dev));		\
		if (f_w && check_node(mypage.pm_handle, (int)mypage.node_dev)) \
			printf(" *");					\
		printf("\n");						\
	}								\
}

	int		i, header = 1;
	prmap_sgi_t	*myreg, *cr;
	pm_pginfo_t	*cp;
	pm_pginfo_t	mypage;
	int		pages = 0;

	cp = (pm_pginfo_t*)proc.minfo.pmaddr;
	for (i = 0; i < proc.minfo.pmsize; i++, cp++) {

		cr = find_region(cp);

		if (pages == 0) {
			mypage = *cp;
			myreg = cr;
			pages = 1;
		} else if (myreg == cr &&
			   cp->node_dev == mypage.node_dev &&
			   cp->page_size == mypage.page_size &&
			   cp->pm_handle == mypage.pm_handle &&
			   cp->vaddr == mypage.vaddr + pages * mypage.page_size)
		{
			pages++;
		} else {

			PRINT_IT;
			pages = 1;
			mypage = *cp;
			myreg = cr;
		}
	}
	if (pages)	PRINT_IT;

#undef	PRINT_FLAGS
#undef	PRINT_IT
}


/*
 *	Main program
 */
void
main(int argc, char *argv[])
{
	char	*me;			/* program name */
	int	pid = -1;
	int	c;
	int	f_a = 0;		/* show all memory (non x only) */
	int	f_l = 0;		/* show memory */
	int	err = 0;

	/*
	 * Parse command options
	 */
	me = argv[0];
	while ((c = getopt(argc, argv, CMDOPTS)) != EOF) switch (c) {
	case 'a':	f_l++; f_a++; break;
	case 'l':	f_l++; break;
	case 'w':	f_w++; break;
	default:	usage(me); exit(1);
	}

	if (optind < argc) {
		pid = atoi(argv[optind]);
	} else {
		usage(me);
		exit(1);
	}

	if (pid == 0)
		pid = getpid();

	/*
	 * Get process status information
	 */
	if (get_proc(pid)) {
		exit(1);
	}

	/*
	 * Get Memory Management Status
	 */
	err = get_pmos() + get_mldsets() + get_mlds();
	if (f_l)
		err += get_mem();

	/*
	 * Close channel to proc (no longer needed)
	 */
	close(proc.fd);

	/*
	 * Print Memory Management Status
	 */
	if (!err) {
		print_intro();
		print_pmos();
		print_mldsets();
		print_mlds();
		if (f_l)
			print_mem(f_a);
	} else {
		printf("%s %d\n", BUMMER, proc.pid);
	}
	if (bug)
		printf("  WARNING: unexpected failures mapping MLDs. Some data may be invalid\n");
}
