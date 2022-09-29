/*
 * We are making an assumption here that all of the fields of the union
 * start at the same place.
 */
union nlm_args_u {
	caddr_t			alignment;		/* force pointer alignment */
	nlm_lockargs	lockargs_1;
	nlm_cancargs	cancelargs_1;
	nlm_testargs	testargs_1;
	nlm_unlockargs	unlockargs_1;
	nlm4_shareargs	shareargs_1;
	nlm_notify		notify_1;
	nlm_res			statres_1;
	nlm_testres		testres_1;
	nlm4_lockargs	lockargs_4;
	nlm4_cancargs	cancelargs_4;
	nlm4_testargs	testargs_4;
	nlm4_unlockargs	unlockargs_4;
	nlm4_shareargs	shareargs_4;
	nlm4_notify		notify_4;
	nlm4_res		statres_4;
	nlm4_testres	testres_4;
};

union nlm_res_u {
	caddr_t			alignment;		/* force pointer alignment */
	nlm_testres		testres_1;
	nlm_res			statres_1;
	nlm_shareres	shareres_1;
	nlm4_testres	testres_4;
	nlm4_res		statres_4;
	nlm4_shareres	shareres_4;
};

/*
 * async NLM call waiting
 */
struct nlm_wait {
	struct nlm_wait	*nw_next;
	struct nlm_wait	*nw_prev;
	u_int			nw_state;
	long			nw_cookie;
	nlm4_stats		nw_status;
	flock_t			nw_flock;
	long			nw_ipaddr;
	sv_t			nw_wait;
};

typedef struct nlm_wait nlm_wait_t;

#define NW_LOCK             0x80000000	/* lock bit for state field */
#define NW_WAITING          0x00000001	/* process waiting for reply */
#define NW_TIMEOUT          0x00000002	/* wait timed out */
#define NW_REPLY			0x00000004	/* reply received */
#define NW_RESPONSE			0x00000010	/* response wait */
#define NW_BLOCKED			0x00000020	/* blocked wait */
#define NW_MASK             (NW_REPLY | NW_WAITING | NW_TIMEOUT)

extern void start_lockd(void);
extern void stop_lockd(void);
extern int lockd_dispatch(struct svc_req *, XDR *, caddr_t, caddr_t);
#ifdef DEBUG
extern void idbg_nlmwait(__psint_t addr);
extern void idbg_waithash(__psint_t addr);
extern void idbg_nlmdup(__psint_t addr);
#endif /* DEBUG */
