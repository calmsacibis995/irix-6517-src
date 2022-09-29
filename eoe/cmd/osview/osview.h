/*
 * Protoypes and external declarations for osview
 */

#include <sgidefs.h>

/*
 * List type used to initialize the display.
 */

/*
 * Some fundamental constants.  This makes the programming easier (and
 * the program smaller) but it loses some flexibility.
 */

# define	FIELDSIZE	20
# define	TEXTSIZE	18
# define	MARGIN		((FIELDSIZE - TEXTSIZE)/2)

/*
 * Main list element.  Holds all information about a thing that we are
 * displaying.
 */

typedef struct tmp10 {
	int	flags;		/* action flags */
	int	oflags;		/* detect state change */
	int	xpos;		/* screen x origin */
	int	ypos;		/* screen y origin */
	char	name[TEXTSIZE];		/* field name */
	char	format[TEXTSIZE];	/* format string */
	void	(*fillfunc)();	/* function when re-display called */
	long	fillarg;	/* single argument to function */
	struct tmp10	*next;	/* next element in list */
} Reg_t;

typedef struct {
	int	flags;		/* what flags should be */
	char	name[TEXTSIZE];		/* name of field */
	void	(*fillfunc)();	/* called only if header */
	long	fillarg;	/* only if header */
	Reg_t	*(*expandfunc)();/* function on expand request for header */
} initReg_t;

# define	RF_ON		01	/* logically visible */
# define	RF_VISIBLE	02	/* physically visible */
# define	RF_BOLD		04	/* bold highlight item */
# define	RF_EXPAND	010	/* expand header definition */
# define	RF_SUPRESS	020	/* elements hidden */
# define	RF_HDRSYS	0100
# define	RF_HDRCPU	0200
# define	RF_HDRMEM	0400
# define	RF_HDRNET	01000
# define	RF_HDRMSC	02000
# define	RF_HEADER	(RF_HDRSYS|RF_HDRCPU|RF_HDRMEM|		\
				 RF_HDRNET|RF_HDRMSC)
# define	RF_USERF1	010000	/* user flag 1 */
# define	RF_USERF2	020000	/* user flag 2 */
# define	RF_USERF3	040000	/* user flag 3 */


/* exports from getinfo.c */

extern void		initinfo(void);
extern void		setinfo(void);
extern void		getinfo(void);

extern struct gfxinfo	*lGp;
extern struct gfxinfo	*nGp;
extern struct sysinfo	*lsp[];
extern struct sysinfo	*nsp[];
extern struct sysinfo	*lSp;
extern struct sysinfo	*nSp;
extern struct sysinfo	*lCSp;
extern struct sysinfo	*nCSp;
extern struct ipstat	*nipstat;
extern struct ipstat	*lipstat;
extern struct udpstat	*nudpstat;
extern struct udpstat	*ludpstat;
extern struct tcpstat	*ntcpstat;
extern struct tcpstat	*ltcpstat;
extern struct mbstat	*nmbstat;
extern struct mbstat	*lmbstat;
extern int		nif;
extern struct ifreq	*nifreq;
extern struct ifreq	*lifreq;
extern struct minfo	*lmp;
extern struct minfo	*nmp;
extern struct rminfo	rminfo;
#ifdef TILES_TO_LPAGES
extern struct tileinfo	*ltileinfo, *ntileinfo;
#endif
extern struct igetstats	*nigetstat;
extern struct igetstats	*ligetstat;
extern struct xfsstats	*nxfsstat;
extern struct xfsstats	*lxfsstat;
extern struct getblkstats	*ngetblkstat;
extern struct getblkstats	*lgetblkstat;
extern struct vnodestats	*nvnodestat;
extern struct vnodestats	*lvnodestat;
extern struct lpg_stat_info	*llpg_stat;
extern struct lpg_stat_info	*nlpg_stat;
extern struct nodeinfo		*node_info;
extern struct ncstats	*nncstat;
extern struct ncstats	*lncstat;
extern struct pda_stat	pdastat[];
extern long		ticks;
extern int		avenrun[3];
extern int		ncpu;
extern int		interval;
extern int		categ;
extern int		persec;
extern int		counts;
extern int		updatelast;
extern long		vn_vnodes;
extern int		vn_epoch, vn_nfree;
extern int		min_file_pages;
extern int		syssegsz;
extern int		nscpus;
extern int		numnodes;
extern int		numcells;
extern int		cpuvalid[];
extern cell_t		cellid;
extern int		ps;

/* exports from main.c */

extern int		lines;
extern int		counts;

/* exports from panel.c */

extern void		pbegin(initReg_t *, void (*)(void), void (*)(void),
			      char *, int);
extern void		pbyebye(void);
extern Reg_t *		pfind(int, int);
extern Reg_t *		pfollow(Reg_t *);

/* exports from readstruct.c */

extern void		stg_sread(char *, void *, int);
extern void		stg_vread(__psunsigned_t, void *, int);
extern void		stg_sysget(int, char *, int);
extern int 		stg_sysget_cpu1(int, char *, int, int);
extern void		stg_sysget_cpus(int, char *, int);
extern void		stg_sysget_cells(int, char *, int);

extern char *		vmunix;

/* exports from run.c */

extern void		runit(void);
extern void		suppress_node_info(void);
extern void		suppress_cell_info(void);

