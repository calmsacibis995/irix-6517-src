/*
 * memusage - memory usage accounting
 *
 * This program lists the memory used by processes and the kernel.  It has
 * a terse mode (the default), and a verbose mode.  In verbose mode, it lists
 * the memory allocated to each portion of a process.  Memory for processes is
 * listed in four columns: virtual, unique, shared, weighted.  The virtual size
 * is the total virtual address space.  The unique size is the memory that is
 * exclusively allocated for the particular process.  The shared size is memory
 * that the process is using, but is also being used by others.  The weighted
 * size is the unique memory plus a portion of the shared memory which is
 * calculated as the size of the amount being shared divided by the number of
 * people sharing it.  All calculations are accurate to the 4K page size of the
 * system, but are expressed as kilobytes.  The sum of all of the weighted sizes
 * is the total amount used by all processes.
 *
 * The program uses a data file that contains the pathnames of files likely
 * to be executed.  These are used to provide full names for shared libraries
 * and the verbose format listing.  A program, mkinodetbl, will generate the
 * data file:
 *		mkinodetbl > /usr/tmp/memusage.inodes
 *
 * To determine the working set of a particular system configuration, perform
 * the following steps:
 *
 *	1. Start with a system that has ample memory, like 1 1/2 times as much
 *         as will likely be needed.
 *	2. Bring up all of the applications that you want to test.
 *	3. Run the flushmem program several times to page everything out:
 *	     flushmem & flushmem & flushmem
 *	4. Do whatever tasks you want for your particular test.  This will
 *	   cause the system to page back in whatever pages are needed.
 *	5. Run memusage to collect the working set data.
 *
 *
 * $Revision: 1.5 $
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define _KMEMUSER
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/region.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/var.h>
#include <sys/pmap.h>
#include <sys/immu.h>
#include <sys/pfdat.h>
#include <sys/sysmacros.h>
#include <sys/prctl.h>
#include <sys/mbuf.h>
#include <nlist.h>
#include <getopt.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>

int		kmemfd;		/* File descriptor of /dev/kmem */
struct pfdat *	pfdat;		/* same as kernel */
struct var	vr;		/* same as kernel v struct */

long		maxclick,	/* pfn of last page */
		firstfree,	/* pfn of first pageable page */
		physmem,	/* number of pages of memory in system */
		heapmem,	/* bytes claimed by kern_malloc */
		zonemem,	/* bytes claimed by zone malloc */
		firstclick,	/* maxclick-physmem (128Meg on MC machines) */
		kernmag,	/* contains value of _end */
		bufmem;		/* bytes claimed by fs_bio */

int tflg = 1;			/* terse flag */

char *		unixpath = "/unix";
char *		corepath = "/dev/kmem";
char *		nodepath = "/var/tmp/memusage.inodes";

struct	mbstat	mbstat;		/* network buffer statistics */

pde_t *		kptbl;		/* k2 segment map */

/*
 * As processes are scanned, the memory they reference
 * is marked in the pfdat table with these values.
 */
#define MRK_UPAGE	(pfd_t *)0xcafebabe
#define MRK_PROCESS	(pfd_t *)0xbeefbabe
#define MRK_PTABLE	(pfd_t *)0xbeabface

char *pregtypes[] = {
	"UNUSED", 
	"TEXT",
	"DATA",
	"Stack", 
	"SHMEM",
	"MEM", 	
	"LibTxt",
	"LibDat", 
	"GR", 
	"MapFile",
	"PRDA"};

/*
 * Abbreviated proc for internal use
 */
struct myproc {
	char	u_comm[PSCOMSIZ];	/* name */
	long	p_stat;			/* flags */
	long	p_user;			/* k2 address of u. */
	pde_t	p_upg0;			/* user page */
	pde_t	p_upg1;			/* kernel stack */
	long	p_size;			/* virtual size */
	void *	p_shd;			/* share structure */
	long	nproc;

	struct pregion *p_region;	/* private preg list */
	struct pregion *p_sregion;	/* shared preg list */
};
	
/*
 * Table to correlate inode numbers to path names
 */
struct inrec {
	struct inrec *next;
	long inode;
	dev_t rdev;
	char name[128];
};

struct inrec *inodes;

char *
get_pregiontype(struct pregion *preg)
{
	if (preg->p_type != PT_MAPFILE)
		return pregtypes[preg->p_type];

	if (preg->p_reg->r_flags & RG_TEXT)
		return "Code";

	if (preg->p_reg->r_flags & RG_PHYS)
		return "I/O";

	return "Data";
}

/*
 * Get something from /dev/kmem (or core file)
 *
 * If the seek or read fails, give up on whole program
 */
readmem(long loc, void * buf, long buflen)
{
	int ret;

	if ((ret = lseek (kmemfd, loc, SEEK_SET)) == -1) {
		perror ("lseek");
		exit(1);
	}

	if ((ret = read (kmemfd, buf, buflen)) < 0) {
		perror ("read");
		exit(1);
	}

	return ret;
}

void
add_inode(long inode, dev_t rdev, char *str)
{
	struct inrec *new;

	new = (struct inrec *)malloc(sizeof (struct inrec));
	new->next = inodes;
	new->inode = inode;
	new->rdev = rdev;
	strcpy (new->name, str);
	inodes = new;
}

/*
 * Given a vnode at address vloc, find a corresponding path name
 */
char *
get_inode(long vloc)
{
	struct vnode vn;
	dev_t rdev;
	static long first_call = 1;
	FILE *fp;
	long inode;
	static char str[128];
	struct inrec *current;

	if (first_call) {
		first_call = 0;
		if ((fp = fopen (nodepath, "r")) != NULL)
			while (fscanf(fp,"%d %d %s\n",&rdev,&inode,str) == 3)
				add_inode (inode, rdev, str);
	}

	readmem(vloc, &vn, sizeof vn);

	/*
	 * Hack to show /dev/zero which all programs have
	 */
	if (vn.v_type == VCHR) {
		readmem(((long)vn.v_data) + 20, &rdev, sizeof rdev);
		if (major(rdev) == 37 && minor(rdev) == 0)
			return "/dev/zero";
		sprintf(str, "/dev/(%d,%d)", major(rdev), minor(rdev));
		return str;
	}

	/*
	 * Hard coded structure offsets!
	 */
	readmem(((long)vn.v_data) + 28, &inode, sizeof (inode));
	readmem(((long)vn.v_data) + 32, &rdev, sizeof (rdev));

	for (current = inodes; current; current = current->next)
		if (current->inode == inode && current->rdev == rdev)
			return current->name;

	sprintf(str, "%d", inode);
	return str;
}

/*
 * Read in user page at location loc
 */
struct user *
readuser(long loc)
{
	struct user *up;

	if ((up = (struct user *)malloc(sizeof (struct user))) == NULL) {
		fprintf(stderr, "readuser: malloc failed.\n");
		return 0;
	}

	readmem(loc, up, sizeof (struct user));

	return up;
}

/*
 * Read in entire pfdat table
 */
struct pfdat *
readpfdat(long loc)
{
	struct pfdat *pfdat;

	if ((pfdat = (struct pfdat *)malloc(sizeof (struct pfdat)
					* (maxclick - firstfree))) == NULL) {
		fprintf(stderr, "readpfdat: malloc failed.\n");
		return 0;
	}

	readmem(loc + (firstfree * sizeof (struct pfdat)),
		pfdat, sizeof (struct pfdat) * (maxclick - firstfree));

	return pfdat;
}

/*
 * mark a pfdat at address kv with mark
 * kv can be either a K0, K1, or K2 seg address.
 */
void
markpfdat(void * kv, pfd_t * mark)
{
	unsigned pfn;

	if (IS_KSEGDM(kv))
                pfn =  pnum(KDM_TO_PHYS(kv));
	else
        if (pnum((__psunsigned_t)kv - K2SEG) < vr.v_syssegsz)
                pfn =  pdetopfn(kvtokptbl(kv));
	else {
		fprintf(stderr, "Funny mark address %#x\n", kv);
		return;
	}

	if (pfn < firstfree) {
		fprintf(stderr, "Funny mark address %#x\n", kv);
		return;
	}

	pfdat[pfn-firstfree].pf_hchain = mark;
}

/*
 * read in a pmap and all of its page tables
 */
pmap_t *
readpmap(long loc)
{
	pmap_t *pmap;
	pte_t **ptt, *pte;
	int idx;

	/* get pmap struct */
	if ((pmap = (pmap_t *)malloc(sizeof (pmap_t))) == NULL) {
		fprintf(stderr, "readpmap: malloc failed.\n");
		return 0;
	}

	readmem(loc, pmap, sizeof (pmap_t));

	/* Segment table */
	if ((ptt = (pte_t **)malloc(NPGTBLS * sizeof (pde_t *))) == NULL) {
		fprintf(stderr, "readpmap: malloc failed.\n");
		return 0;
	}

	readmem((long)pmap->pmap_ptr, ptt, NPGTBLS * sizeof (pde_t *));

	/* Read each page table */
	for (idx = 0; idx < NPGTBLS; idx++)
		if (ptt[idx]) {
			pte = (pte_t *)malloc(NBPP);

			markpfdat(ptt[idx], MRK_PTABLE);

			readmem((long)ptt[idx], pte, NBPP);
			ptt[idx] = pte;
		}

	pmap->pmap_ptr = (void *)ptt;
	return pmap;
}

struct pregion *
readregions(long loc)
{
	struct pregion *preg, *head, *current;
	struct region *reg;
	pmap_t *pmap, *previous_pmap;

	head = 0;
	pmap = 0;
	while (loc != 0) {
		if (!(preg =(struct pregion *)malloc(sizeof(struct pregion)))) {
			fprintf(stderr, "readregion: malloc failed.\n");
			return 0;
		}

		readmem(loc, preg, sizeof (struct pregion));

		if (head)
			current->p_forw = preg;
		else {
			head = preg;
			previous_pmap = preg->p_pmap;
		}
		current = preg;
		loc = (long)current->p_forw;
		if (!pmap || (previous_pmap != current->p_pmap)) {

			if ((pmap = readpmap((long)current->p_pmap)) == NULL)
				return 0;
		}

		previous_pmap = current->p_pmap;
		current->p_pmap = pmap;

		if (!(reg = (struct region *)malloc(sizeof (struct region)))) {
			fprintf(stderr, "readregion: malloc failed.\n");
			return 0;
		}

		readmem((long)current->p_reg, reg, sizeof (struct region));
		current->p_reg = reg;
	}

	return head;
}


/*
 * get pregion stats
 *
 * This routine calculates three size values: total valid pages,
 * valid non-shared pages, and a weighted combination of the two.
 */
double
get_weighted_size(preg_t *preg, long *rsize, int *unique)
{
	pte_t **ptt;
	long idx, jdx;
	long bucket, entry, len;
	double size;

	/* ptt is the segment table for the pmap belong to preg */
	ptt = (pte_t **)preg->p_pmap->pmap_ptr;

	/* starting offset into the segment table */
	bucket = btost(preg->p_regva);

	/* starting offset into the first page table */
	entry = ((unsigned)preg->p_regva % NBPS) / NBPC;

	/* total number of virtual pages in this pregion */
	len = preg->p_pglen;

	size = 0.0;
	*rsize = 0;

	jdx = entry;
	for (idx = bucket; idx < NPGTBLS && len > 0; idx++) {

		/* If page table is not yet allocated, skip it */
		if (!ptt[idx]) {
			len -= (NBPP / sizeof (pte_t) - jdx);
			jdx = 0;
			continue;
		}

		/* For each pte, find the corresponding pfdat and mark it. */
		for (; jdx * sizeof (pte_t) < NBPP && len > 0; jdx++, len--)
			if (ptt[idx][jdx].pte_sv) {

				pfd_t *pfd = &pfdat[
				      ptt[idx][jdx].pte_pfn-firstfree];

				if (pfd->pf_use == 0) {
					/* I don't understand how this
					 * can happen, but it does.
					 */
					continue;
				}

				pfd->pf_hchain = MRK_PROCESS;

				if (pfd->pf_use == 1)
					*unique += 1;

				(*rsize)++;

				size = size + (NBPP/pfd->pf_use);
			}

		jdx = 0;
	}

	return size / NBPP;
}

/*
 * Read the user process name
 */
char *
read_ucomm(long loc, char *comm)
{
	struct user *user;

	if (!loc || ((user = readuser(loc)) == NULL)) {
		strcpy(comm, "defunct");
		return comm;
	}
	strcpy(comm, user->u_comm);
	free(user);
	return comm;
}

/*
 * Read the proc table entry at loc and map it into our own private
 * style.
 */
struct myproc *
readproc(struct proc *loc)
{
	struct proc pp;
	struct myproc *mproc;

	if (!(mproc = (struct myproc *)calloc(1, sizeof (struct myproc)))) {
		fprintf(stderr, "readproc: malloc failed.\n");
		return 0;
	}

	readmem((long)loc, &pp, sizeof (struct proc));

	mproc->p_stat = pp.p_stat;
	mproc->p_region = pp.p_region;
	mproc->p_user = (long)pp.p_user;
	mproc->p_upg0 = pp.p_upgs[0];
	mproc->p_upg1 = pp.p_upgs[1];
	mproc->p_size = pp.p_size;
	read_ucomm(mproc->p_user, &(mproc->u_comm[0]));

	/*
	 * If the process is part of a share group, get the shaddr struct
	 * so we can get the shared pregion list
	 */
	if (pp.p_shaddr && (pp.p_shmask & PR_SADDR)) {
		struct shaddr_s shaddr;
		readmem((long)pp.p_shaddr, &shaddr, sizeof shaddr);
		mproc->p_shd = mproc->p_sregion = shaddr.s_region;
	}

	return mproc;
}

void
usage()
{
	fprintf(stderr, "usage: memusage [-v] [-n unixpath] [-c corefile]\n");
	exit(1);
}

main(int argc, char *argv[])
{
	int i;
	int c;
	struct nlist nl[32];
	struct myproc **proc;
	struct proc *procloc;
	long nprocs, pfdatloc;
	long totalsize;
	long totalhash;
	long totalanonhash;
	double totalweighted;
	long pregtotal;
	struct pregion *preg;
	struct region *reg;
	int totalfree;
	int totalchunked;
	int totalunacct;
	int totalupages;
	int ktotal = 0;
	int ftotal = 0;
	int total_pagetables = 0;

	while ((c = getopt(argc,argv,"tvn:c:")) != EOF)
		switch (c) {
			case 'n':
				unixpath = optarg;
				break;
			case 'c':
				corepath = optarg;
				break;
			case 't':
				tflg = 1;
				break;
			case 'v':
				tflg = 0;
				break;
			case '?':
				usage();
				break;
		}
	
	if (optind != argc)
		usage();

	if ((kmemfd = open(corepath, O_RDONLY)) < 0) {
		perror("open");
		exit(1);
	}

#define NL_PROC		0
#define NL_V		1
#define NL_MAXCLICK	2
#define NL_PFDAT	3
#define NL_FIRSTFREE	4
#define NL_PHYSMEM	5
#define NL_BUFMEM	6
#define NL_KERNMAG	7
#define NL_MBSTAT	8
#define NL_KPTBL	9
#define NL_END		10
#define NL_MEMBASE	11
#define NL_LAST		12

	nl[NL_PROC].n_name	= "proc";
	nl[NL_V].n_name		= "v";
	nl[NL_MAXCLICK].n_name	= "maxclick";
	nl[NL_PFDAT].n_name	= "pfdat";
	nl[NL_FIRSTFREE].n_name	= "firstfree";
	nl[NL_PHYSMEM].n_name	= "physmem";
	nl[NL_BUFMEM].n_name	= "bufmem";
	nl[NL_KERNMAG].n_name	= "kernel_magic";
	nl[NL_MBSTAT].n_name	= "mbstat";
	nl[NL_KPTBL].n_name	= "kptbl";
	nl[NL_END].n_name       = "end";
	nl[NL_MEMBASE].n_name   = "_physmem_start";

	nl[NL_LAST].n_name	= 0;

	if (nlist(unixpath, nl) < 0) {
		perror("nlist");
		exit(1);
	}

	for ( i = 0; nl[i].n_name; i++ )
		if (!nl[i].n_type) {
			printf("%s not found\n", nl[i].n_name);
			exit(1);
		}

	readmem(     nl[NL_PROC].n_value, &procloc,   sizeof procloc);
	readmem(        nl[NL_V].n_value, &vr,        sizeof vr);
	readmem( nl[NL_MAXCLICK].n_value, &maxclick,  sizeof maxclick);
	readmem(    nl[NL_PFDAT].n_value, &pfdatloc,  sizeof pfdatloc);
	readmem(nl[NL_FIRSTFREE].n_value, &firstfree, sizeof maxclick);
	readmem(   nl[NL_BUFMEM].n_value, &bufmem,    sizeof bufmem);
	readmem(  nl[NL_KERNMAG].n_value, &kernmag,   sizeof kernmag);
	readmem(  nl[NL_PHYSMEM].n_value, &physmem,   sizeof physmem);
	readmem(   nl[NL_MBSTAT].n_value, &mbstat,    sizeof mbstat);

	firstclick = btoc(nl[NL_MEMBASE].n_value);

	if (nl[NL_END].n_value != kernmag) {
		fprintf(stderr,"%s: %s does not match %s, try -n option\n",
			argv[0], unixpath, corepath);
		exit(1);
	}
	if ((pfdat = readpfdat(pfdatloc)) == NULL)
		exit(1);

	/* Read in system segment (K2) table */
	{
	pde_t *akptbl;

	kptbl = (pde_t *)malloc(sizeof kptbl[0] * vr.v_syssegsz);
	readmem(nl[NL_KPTBL].n_value, &akptbl, sizeof akptbl);
	readmem((unsigned long)akptbl, kptbl, sizeof kptbl[0] * vr.v_syssegsz);
	}

	if (!(proc = (struct myproc **)malloc(vr.v_proc*sizeof *proc))) {
		fprintf(stderr, "memusage: malloc of proc array failed.\n");
		exit(1);
	}

	/* Read in all of the procs, regions, pmaps and page tables */
	for (i = 0, nprocs = 0; i < vr.v_proc; i++, procloc += 1) {
		if ((proc[nprocs] = readproc(procloc)) == NULL)
			exit(1);
		proc[nprocs]->nproc = nprocs;
		if (proc[nprocs]->p_stat == 0)
			free(proc[nprocs]);
		else {
			proc[nprocs]->p_region =
				readregions((long)proc[nprocs]->p_region);

			/* Eliminate all but the first proc
			 * in a shared address group
			 */
			if (proc[nprocs]->p_sregion) {
				int j;
				for ( j = 0; j < nprocs; j++ )
					if (proc[j]->p_shd ==
						     proc[nprocs]->p_sregion) {
						proc[nprocs]->p_sregion = 0;
						break;
					}
				if (proc[nprocs]->p_sregion)
					proc[nprocs]->p_sregion =
						readregions((long)proc[nprocs]->p_sregion);
			}
			nprocs++;
		}
	}

	totalweighted = 0.0;
	totalsize = 0;
	totalupages = 0;
	for (i = 0; i < nprocs; i++) {
		double procweighted = 0;

		int procunique = 0;
		int procshared = 0;
		int nregs = 0;

		/*
		 * Mark off upages in pfdat
		 *
		 * Defunct processes don't have these
		 */
		if (proc[i]->p_upg0.pte.pg_pfn) {
			pfdat[proc[i]->p_upg0.pte.pg_pfn-firstfree].pf_hchain =
								MRK_UPAGE;
			pfdat[proc[i]->p_upg1.pte.pg_pfn-firstfree].pf_hchain =
								MRK_UPAGE;
		}

		if (!(preg = proc[i]->p_region))
			preg = proc[i]->p_sregion;
			
		if (!preg)
			continue;

		if (tflg) {
			if (totalsize==0)
			printf("Name              Virtual   Unique    Share    Weight\n");
		}
		else {
			printf(
			"\nName        Address    Type       Virtual  Resident         File"
			"\n                                           Unique   Shared\n");
		}

		printf ("%-10.10s ", proc[i]->u_comm);

dopregs:
		for (; preg; preg = preg->p_forw) {
			double pregweight = 0;
			int pregunique = 0;

			reg = preg->p_reg;
			if (reg->r_flags & RG_PHYS)
				continue;

			if (nregs&&!tflg)
				printf("           ");

			nregs++;

			pregweight    = get_weighted_size(preg,
							  &pregtotal,
							  &pregunique);
			procunique   += pregunique;
			procshared   += pregtotal-pregunique;
			procweighted += pregweight;
			if (!tflg)
				printf (" 0x%08x %-8s ",
					preg->p_regva,
					get_pregiontype(preg));
			if (tflg)
				continue;
			printf ("%8d %8d %8d  %s\n",
				preg->p_pglen * 4,	/* Virtual size */
				pregunique * 4,
				(pregtotal-pregunique) * 4,	/* shared */
				(preg->p_reg->r_vnode == 0) ? "" :
				get_inode((long)preg->p_reg->r_vnode));
		}

		if (preg = proc[i]->p_sregion) {
			proc[i]->p_sregion = 0;
			goto dopregs;
		}

		if (tflg)
			printf("      %8d %8d %8d   %7.0f\n",
				proc[i]->p_size * 4,
				procunique*4,
				procshared*4,
				procweighted*4);
		else
			printf("          %3d page tables       %8d "
			       "%8d %8d   Weighted Size: %.0f\n",
				proc[i]->p_region?
				proc[i]->p_region->p_pmap->pmap_scount: 0,
				proc[i]->p_size * 4,
				procunique*4,
				procshared*4,
				procweighted*4);

		totalsize     += proc[i]->p_size;
		totalweighted += procweighted;
	}

	printf("\nProcess totals     %6d                     %7.0f\n\n",
		totalsize * 4,
		totalweighted * 4);

	/* Scan entire pfdat list to pick up strays */
	totalhash = 0;
	totalanonhash = 0;
	totalfree = 0;
	totalchunked = 0;
	totalunacct = 0;
	for (i = 0; i < (maxclick - firstfree); i ++)
		if (pfdat[i].pf_use == -1)	/* memory hole */
			continue;
		else
		if (pfdat[i].pf_hchain == MRK_PROCESS)
			continue;
		else
		if (pfdat[i].pf_hchain == MRK_UPAGE)
			totalupages += 1;
		else
		if (pfdat[i].pf_hchain == MRK_PTABLE)
			total_pagetables += 1;
		else
		if ((pfdat[i].pf_flags & (P_HASH|P_ANON)) ==
							(P_HASH|P_ANON))
			totalanonhash += 1;
		else
		if ((pfdat[i].pf_flags & (P_HASH|P_QUEUE)) ==
							(P_HASH|P_QUEUE))
			totalhash += 1;
		else
		if ((pfdat[i].pf_flags & (P_HASH|P_SQUEUE)) ==
							(P_HASH|P_SQUEUE))
			totalhash += 1;
		else
		if ((pfdat[i].pf_flags & (P_HASH|P_QUEUE)) ==
							(P_QUEUE))
			totalfree += 1;
		else
		if ((pfdat[i].pf_flags & (P_HASH|P_SQUEUE)) ==
							(P_SQUEUE))
			totalfree += 1;
		else
		if (pfdat[i].pf_flags & P_HASH)
			totalchunked += 1;
		else
			totalunacct += 1;
	
	{
	int ktmp;

	printf("Kernel\n");

	printf("    Load size                                %8d\n",
		ktmp = (btoc(K0_TO_PHYS(kernmag)) - firstclick) * 4);
	ktotal += ktmp;

	printf("    Fixed tables                             %8d\n",
		ktmp = (firstfree - btoc(K0_TO_PHYS(kernmag))) *4);
	ktotal += ktmp;

	{
		int minfosz;
		struct minfo *mi;

		minfosz = sysmp(MP_SASZ, MPSA_MINFO);
		if (minfosz != sizeof *mi)
			printf("Bad minfosz\n");
		mi = calloc(1, sizeof *mi);

		sysmp(MP_SAGET, MPSA_MINFO, mi, minfosz);
		heapmem = mi->heapmem;
		zonemem = mi->zonemem;
	}

	printf("    Dynamic tables                           %8d\n",
		ktmp = (heapmem/1024)+(zonemem/1024));
	ktotal += ktmp;
	totalunacct -= ktmp/4;

	printf("    Network buffers                          %8d\n",
		ktmp = mbstat.m_clusters * 4);
	ktotal += ktmp;

	totalunacct -= bufmem;

	printf("    Unaccounted                              %8d\n",
		ktmp = totalunacct * 4);
	ktotal += ktmp;

	printf("    Process overhead                         %8d\n",
		ktmp = (totalupages * 4) + (total_pagetables * 4));
	ktotal += ktmp;

	printf("\nKernel total:                                %8d\n", ktotal);
	}

	printf("\nMemory pool:\n");
	printf("    Page cache:                              %8d\n", totalhash *4);
	ftotal += totalhash * 4;
	printf("    Free pages:                              %8d\n", totalfree *4);
	ftotal += totalfree * 4;
	printf("    Swap cache:                              %8d\n", totalanonhash *4);
	ftotal += totalanonhash * 4;
	printf("    Buffer cache:                            %8d\n", totalchunked *4);
	ftotal += totalchunked * 4;
	printf("    Filesystem meta cache:                   %8d\n", bufmem *4);
	ftotal += bufmem * 4;

	printf("\nMemory pool total:                           %8d\n", ftotal);


	printf("\n\nAccounted memory total:          %20.0f\n", ftotal+ktotal+(totalweighted*4));
	if ((physmem * 4) - (ftotal+ktotal+(totalweighted*4)) < 0)
	printf("Over accounted                   %20.0f\n",
			 (ftotal+ktotal+(totalweighted*4)) - (physmem * 4));
	else
	printf("Unaccounted                      %20.0f\n",
			(physmem * 4) - (ftotal+ktotal+(totalweighted*4)));

	return 0;
}
