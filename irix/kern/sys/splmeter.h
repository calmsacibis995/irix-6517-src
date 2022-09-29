typedef struct stkfr {
	int	ra;
	int	func;
	int	args[4];
	struct  stkfr	*next;
} stkfr_t;

typedef struct spl {
	inst_t	*hipc;		/* routine that call splhi or spsemahi */
	inst_t	*lopc;		/* routine that lower intr, or splx that 
				   directly follow splhi */
	inst_t	*cur_hipc;	/* currently active pc that raises spl */
	uint	tickmax;	/* longest time in clock tick holding splhi */
	uint	realtickmax;	/* minus the suspended time by hier intr */
	struct spl *next;	/* next in the list */
	struct spl *active;	/* next in the active list */
	uint	splcount;	/* how often splhi was called by this routine */
	uint	spxcount;	/* splx counter used to match with splcount */
	uint	latbust;	/* count of instances with busted latency */
	uint	timestamp;	

	inst_t	*callingpc;	/* the key for this entry */
	uint	tickstart;	/* starting splhi tick, no recursive calls */
	uint	suspendtick;	/* suspended tick by hier intr */
	uint	resumetick;	/* resume tick */
	uint	ishicount;	/* if splhi is called and already at hi */
	uint	mxishi;		/* set if at hi for the max case */
	uint	curishi;	/* set if current splhi is aleardy hi */
	short	cpustart;	/* the cpu where this timer is started */
	short	cpuend;		/* the cpu where this timer is stopped */
#if BTRACE
	struct stkfr *hibt;	/* stack backtrace for splhi */
	struct stkfr *lobt;	/* stack backtrace for splx */
	struct stkfr *curbt;	/* cur stack backtrace for splhi */
#endif
} splmeter_t;

#define IDBG_BTRACE	0
#define SPL_BTRACE	1
