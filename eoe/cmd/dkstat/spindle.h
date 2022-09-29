/*
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/dkstat/RCS/spindle.h,v 1.5 1997/05/19 09:35:43 markgw Exp $
 * struct exported by spindle_stats.
 */

typedef struct {
    int		id;		/* unique identifier for disk drive */
    int		drivenumber;	/* index */
    char	*dname;		/* name of device */
    int		wname;		/* width of name of device */
    char	*unit;		/* unit name */
    __uint32_t	reads;		/* -- stats -- */
    __uint32_t	writes;
    __uint32_t	breads;
    __uint32_t	bwrites;
    __uint32_t	active;
    __uint32_t	response;
} spindle;

typedef struct {
    int		id;		/* unique identifier for disk controller */
    char	*dname;		/* name of device */
    int		wname;		/* width of name of device */
    int		ndrives;	/* number of spindles on controller */
    int		*drives;	/* indicies of spindles */
} controller;

/* return nspindles or <0 error */
extern int spindle_stats_init(spindle **); /* &(spindle *) */

extern int spindle_stats_profile(int *); /* inclusion_map[] */

/* alternates buffers on successive calls */
extern int spindle_stats(spindle **, struct timeval *); /* &(spindle *), &fetchtime */

/* returns controller info */
extern controller *controller_info(int *); /* &ncontrollers */
