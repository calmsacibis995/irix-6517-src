/*
 * $Id: spindle.h,v 1.17 1999/08/26 07:31:48 tes Exp $
 *
 * struct exported by spindle_stats.
 * There is one struct per physical disk spindle.
 */
#include <sys/types.h>

/* values for s_type */
#define SPINDLE_DIRECT_DISK	0	/* standard direct disk */
#define SPINDLE_FC_DISK		1	/* fibre channel disk */

typedef struct {
    unsigned int	s_id;		/* unique identifier for disk drive */
    int			s_type;		/* direct disk or FC disk */
    unsigned int	s_ctlr;		/* major dev number (i.e. controller) */
    char		*s_dname;	/* name of device, e.g. dksCdU, dksCdUlL */
    char		*s_hwgname;	/* path to node in hwgfs */
    unsigned int	s_ops;		/* I/O count (reads + writes) */
    unsigned int	s_wops;		/* number of writes */
    unsigned int	s_bops;		/* block I/O count (reads + writes) */
    unsigned int	s_wbops;	/* number of blocks written */
    unsigned int	s_act;		/* active time (ticks) */
    unsigned int	s_resp;		/* response time (ticks) */
    unsigned int	s_qlen;		/* instantaneous queue length */
    unsigned int	s_sum_qlen;	/* summed queue length on each request completion */
} spindle;


typedef struct {
    unsigned int	c_id;		/* unique identifier for controller */
    unsigned int	c_ctlr;		/* major dev number */
    char		c_dname[16];	/* name of controller, i.e. dksC or fcC */
    int			c_ndrives;	/* no. of attached active disk drives */
    int			*c_drives;	/* indicies of attached drives */
    unsigned long long	c_ops;		/* I/O count (reads + writes) */
    unsigned long long	c_wops;		/* number of writes */
    unsigned long long	c_bops;		/* block I/O count (reads + writes) */
    unsigned long long	c_wbops;	/* number of blocks written */
    unsigned long long	c_act;		/* active time (ticks) */
    unsigned long long	c_resp;		/* response time (ticks) */
    unsigned long long	c_qlen;		/* instantaneous queue length */
    unsigned long long	c_sum_qlen;	/* summed queue length on each request completion */
} controller;


/*
* Returns count of spindles or controllers
* spindle_stats_init() must be called _before_ controller_stats_init()
*/
extern int spindle_stats_init(int);
extern int controller_stats_init(int);

/*
* Returns ptr to array of spindles or controllers
* alternates buffers on successive calls
*/
extern spindle *spindle_stats(int *); 
extern controller *controller_stats(int *); 
