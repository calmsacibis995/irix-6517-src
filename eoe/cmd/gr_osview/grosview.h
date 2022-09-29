/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * Run string structures.  Used to define the layout and order of
 * the display window.
 */

# ifndef	_GROSVIEW_H
# define	_GROSVIEW_H

# include	<sys/types.h>
# include	<sys/statfs.h>
# include	<sys/fstyp.h>
# include	<sys/fsid.h>
# include	<stddef.h>
# include	<stdio.h>

# define	VERS_1		1	/* old program version */
# define	VERS_2		2	/* older program version */
# define	VERS_3		3	/* older program version */
# define	CURVERS		4	/* latest version */
# define	eq(x,y)		(strcmp(x,y)==0)

#include	"rtbar.h"

/*
 * Allowable types of bars.
 * CAUTION: These are used as an index into the initialization table
 *	    in initbar.c.  If you change either the table or these
 *	    numbers, you must keep them both in sync.
 */
# define bcpu		0	/* cpu display */
# define bmem		1	/* memory display */
# define bwait		2	/* wait bar */
# define bgfx		3	/* context switch bar */
# define bsyscall	4	/* system call bar */
# define biothru	5	/* I/O throughput bar */
# define bdisk		6	/* disk space bar */
# define bbdev		7	/* block device activity bar */
# define bintr		8	/* interrupt activity bar */
# define bfault		9	/* page fault activity */
# define btlb		10	/* TLB activity */
# define bswap		11	/* swap activity */
# define bnet		12	/* network activity */
# define bnetip		13	/* IP network activity */
# define bmemc		14	/* memory display w/ free memory cache stats */
# define bswps		15	/* total swap space */
# define bnetudp	16	/* udp packets */
# define bnetipr	17	/* packets through IP layer */
#ifdef bufh_bar
# define bbufh		??	/* buffer headers */
#endif

typedef char	 bar_t;

/*
 * The basic element of the linked list of displayable bars.
 * all information needed to manage the bar is here.
 */
# define	CPU_ALL		-1	/* display for all cpus */
# define	CPU_SUM		-2	/* sum cpu usage */
# define	IP_SUM		-1	/* sum IP usage */
# define	IP_ALL		-2	/* individual IP usage */

# define	SF_NOBORD	01	/* no bar border */
# define	SF_STRIP	02	/* strip chart */
# define	SF_MAX		04	/* display maximum seen */
# define	SF_NUM		010	/* numeric display */
# define	SF_SUM		020	/* tracking display */
# define	SF_FREEZE	040	/* freeze absolute scale */
# define	SF_SLOCK	0100	/* lock scale value */
# define	SF_TICK		0200	/* add tick marks */
# define	SF_BIGTICK	0400	/* add big tick marks */
# define	SF_AVERAGE	01000	/* compute average */
# define	SF_CREEP	02000	/* scale only increases */

typedef struct tmp10 {
	bar_t	s_btype;		/* type of bar */
	int	s_subtype;		/* subtype of bar (if needed) */
	struct colorblock_s	*s_cb;	/* associated block */
	char	*s_name;		/* special name (if needed) */
	char	*s_info;		/* special info (if needed) */
	int	s_colors[MAXSECTS];	/* bar colors, if needed */
	int	s_ndcolors;		/* number of direct-mapped colors */
	void	(*s_draw)(colorblock_t *);/* drawing function */
	int	s_flags;		/* control flags */
	int	s_nsamps;		/* number of samples in strip-chart */
	int	s_nsects;		/* number of remote sections */
	float	s_upmove;		/* attack response */
	int	s_stevery;		/* strip chart tick marks */
	int	s_stbig;		/* strip chart big tick marks */
	int	s_interval;		/* individual bar interval */
	int	s_limit;		/* set bar limit */
	int	s_count;		/* countdown value */
	int	s_disk;			/* disk index */
	int	s_average;		/* average rate */
	int	s_max;			/* max rate */
	int	s_rcb;			/* remote color block index */
	numcol_t	s_limcol;	/* limit colors */
	numcol_t	s_maxcol;	/* max value colors */
	numcol_t	s_sumcol;	/* sum colors */
	struct tmp10 *s_next;		/* next bar in list */
} barlist_t;

# define	CNULL		((Colorindex) -1)

/*
 * Remote protocol definitions.
 */
# define	S_REDRAW	1
# define	S_DRAWABS	2
# define	S_DRAW		3	/* draw a particular bar */
# define	S_DRAWLEGEND	4	/* draw legend */
# define	S_INIT		5
# define	S_TLIMIT	6	/* modify tlimit value */
# define	S_MESSAGE	7
# define	S_LAYOUT	8
# define	S_TAKEDOWN	9
# define	S_INITREPLY	10
# define	S_PUSHCB	11
# define	S_VERSION	12	/* program version */
# define	S_NSLAYOUT	13
# define	S_SETOPT	14	/* set option switches */
# define	S_PAUSE		15
# define	S_GO		16	/* instruct remote to start */

# define	SW_NOBORDER	1	/* turn off bar borders */
# define	SW_WIDTH	2	/* # bars across */
# define	SW_ARBSIZE	3	/* arbitrary window size */
# define	SW_FONT		4	/* font to use */
# define	SW_ORIGIN	5	/* window origin */
# define	SW_WINSIZE	6	/* window size */
# define	SW_BACKCOLOR	7	/* background color */
# define	SW_FRONTCOLOR	8	/* foreground color */
# define	SW_INTERVAL	9	/* global interval */

# define	MAXCPU		256	/* maximum we can deal with */
# define	MAXDISK		40	/* maximum disks we can deal with */
# define	MAXIP		2048	/* maximum net interfaces */
# define	MBYTE		(1024*1024)
# define	RMTBUF		2048

/*
 * Disk status handling.
 */

typedef struct {
	int		d_flags;/* flags */
	struct statfs	d_stat;	/* status */
	int		d_fdes;	/* descriptor, if needed */
	char		*d_sname;/* device name for stat */
	char		*d_name; /* user specified name */
	int		d_fstyp;/* file system type */
} dstat_t;

# define	DF_FSSTAT	01	/* use fstatfs instead */
# define	DF_EFS		02	/* it's an efs filesystem */
# define	DF_NFS		04	/* it's an nfs filesystem */
# define	DF_XFS		010	/* it's an xfs filesystem */
# define	blktomb(x,s)	((x)/((double)MBYTE/s))


/*
 * Remote procedure call handling for drawing.
 */
extern struct cbfunc_s {
	colorblock_t *	(*init)(void);
	void		(*redraw)(void);
	void		(*drawlegend)(colorblock_t *);
	void		(*draw)(colorblock_t *, ...);
	void		(*drawabs)(colorblock_t *);
	void		(*message)(colorblock_t *, char *);
	void		(*layout)(char *);
	void		(*takedown)(void);
} cbfunc;

# define	CBINIT		(cbfunc.init)
# define	CBREDRAW	(cbfunc.redraw)
# define	CBDRAWLEGEND	(cbfunc.drawlegend)
# define	CBDRAW		(cbfunc.draw)
# define	CBDRAWABS	(cbfunc.drawabs)
# define	CBMESSAGE	(cbfunc.message)
# define	CBLAYOUT	(cbfunc.layout)
# define	CBTAKEDOWN	(cbfunc.takedown)

# define	EXPORTSTR	"gr_osview export file"

/* exports from drawbar.c */

extern void		drawbar_text_start(void);
extern void		runit(void);
extern int		event_scan(int);
extern void		drawbar_text_end(void);
extern void		drawcpubar(colorblock_t *);
extern void		drawwaitbar(colorblock_t *);
extern void		drawmembar(colorblock_t *);
extern void		drawmemcbar(colorblock_t *);
extern void		drawgfxbar(colorblock_t *);
extern void		drawsyscallbar(colorblock_t *);
extern void		drawiothrubar(colorblock_t *);
extern void		drawdiskbar(colorblock_t *);
extern void		drawbdevbar(colorblock_t *);
extern void		drawswpsbar(colorblock_t *);
extern void		drawnetudpbar(colorblock_t *);
extern void		drawnetiprbar(colorblock_t *);
extern void		drawnetbar(colorblock_t *);
extern void		drawnetipbar(colorblock_t *);
extern void		drawintrbar(colorblock_t *);
extern void		drawfaultbar(colorblock_t *);
extern void		drawswapbar(colorblock_t *);
extern void		drawtlbbar(colorblock_t *);
extern void		logcbdraw(colorblock_t *, ...);
extern void		logcblayout(char *);
extern void		drawbarreset(void);
extern void		(*drawproc)(colorblock_t *, ...);

/* exports from getinfo.c */

extern void		getinfo_text_start(void);
extern void		initinfo(void);
extern void		setinfo(void);
extern void		getinfo(void);
extern int		netifinfo(char *);
extern int		diskinfo(char *);
extern void		getinfo_text_end(void);

extern long		nticks;
extern struct tcpstat	*ntcpstat;
extern struct sysinfo	*nsp[MAXCPU];
extern struct gfxinfo	*nGp;
extern struct minfo	*nmp;
extern struct ifreq	*nifreq;
extern struct rminfo	*nrm;
extern struct ipstat	*nipstat;
extern struct udpstat	*nudpstat;
extern long		ticks;
extern int		ncpu;
extern long		physmem;
extern off_t		fswap;
extern off_t		mswap;
extern off_t		tswap;
extern off_t		vswap;
extern off_t		rlswap;	
extern off_t		llswap;	
extern off_t		nlswap;
extern int		nif;
extern int		nifsize;

/* exports from initbar.c */

typedef void	(*drfunc_t)(colorblock_t *);
extern drfunc_t		getdrawfunc(int);
extern void		setinitproc(void);
extern void		reinitbars(void);

extern int		interval;

/* exports from main.c */

extern void		lockmem(int);

extern void		(*getdata)(void);

/* exports from mouse.c */

extern void		mouse(int);

/* exports from readstruct.c */

extern void		stg_open(void);
extern off64_t		stg_getval(char *);
extern void		stg_sread(char *, void *, int);
extern void		stg_sptr_read(char *, off64_t *);
extern void		stg_vread(off64_t, void *, int);

/* exports from remote.c */

extern void		remote_text_start(void);
extern void		passdata(void);
extern int		callremote(int);
extern int		pulldraw(colorblock_t *);
extern int		scanscript(FILE *);
extern int		rgetcb(void);
extern int		nextint(char *);
extern double		nextfloat(char *);
extern void		remote_text_end(void);
extern void		pushbackcb(FILE *);
extern void		pushoptions(FILE *);
extern void		waitstart(void);

extern FILE *		rmtwrite;

/* exports from rtbar.c */

extern void		rtbar_text_start(void);
extern colorblock_t *	cbptr(int);
extern colorblock_t *	nextbar(int);
extern void		cbredraw(void);
extern void		cbdrawabs(colorblock_t *);
extern void		cbdraw(colorblock_t *, ...);
extern colorblock_t *	cbinitindex(int);
extern void		cbmessage(colorblock_t *, char *);
extern colorblock_t *	cbinit(void);
extern void		rtbar_text_end(void);
extern void		cbdrawlegend(colorblock_t *);
extern void		cblayout(char *);
extern void		cbtakedown(void);
extern void		cbpreops(void);
extern void		cbresetbar(colorblock_t *);
extern int		cbindex(colorblock_t *);

extern int		arbsize;
extern Colorindex	backcolor;
extern Colorindex	frontcolor;
extern char *		fontface;

/* exports from setup.c */

extern barlist_t *	newbar(void);
extern void		setup(int, char *[]);
extern int		sgets(char *, int, FILE *);

extern barlist_t *	barlist;
extern barlist_t *	barend;
extern FILE *		logfd;
extern char		sexit;
extern char *		cscript;
extern int		debug;
extern FILE *		validscript;
extern char *		remotehost;
extern FILE *		scriptfd;
extern int		slave;
extern char *		pname;
extern int		prefpos;
extern int		prefxpos;
extern int		prefypos;
extern int		prefwsize;
extern int		prefxsize;
extern int		prefysize;
extern dstat_t	 	dstat[];
extern int		ndisknames;
extern char		gi_swap;
extern char		gi_tcp;
extern char		gi_udp;
extern char		gi_ip;
extern char		gi_if;
extern int		do_border;
extern int		barborder;
extern int		sigfreeze;
extern int		pinning;
extern numcol_t		limcol;
extern numcol_t		maxcol;
extern numcol_t		sumcol;
extern char *		sname;
extern int		width;
extern int		myvers;
extern char		nofont;
extern int		debugger;
extern int		printpos;

# endif		/* _GROSVIEW_H */
