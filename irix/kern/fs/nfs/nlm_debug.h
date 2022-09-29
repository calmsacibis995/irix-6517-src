
#ifdef DEBUG
/*
 * NLM debugging
 */
#define NLM_DEBUG(level, op)	if (nlm_debug & (level)) op

/*
 * NLM debugging levels
 * these apply to both client and server
 */
#define NLMDEBUG_DISPATCH	0x00000001	/* debug procedure dispatching */
#define NLMDEBUG_DUP		0x00000002	/* debug retransmissions */
#define NLMDEBUG_TRACE		0x00000004	/* trace RPC calls */
#define NLMDEBUG_ERROR		0x00000008	/* debug errors */
#define NLMDEBUG_NOPROG		0x00000010	/* debug no NLM program */
#define NLMDEBUG_CANTSEND	0x00000020	/* debug RPC_CANTSEND errors */
#define NLMDEBUG_NOPROC		0x00000040	/* debug RPC_PROCUNAVAIL */
#define NLMDEBUG_VERSERR	0x00000080	/* debug RPC_VERSMISMATCH and
											RPC_PROGVERSMISMATCH */
#define NLMDEBUG_CANCEL		0x00000100	/* debug cancels */
#define NLMDEBUG_VNODE		0x00000200	/* debug vnode table maintenance */
#define NLMDEBUG_PROCS		0x00000400	/* debug server creation/destruction */
#define NLMDEBUG_REPLY		0x00000800	/* debug server replies */
#define NLMDEBUG_WAIT		0x00001000	/* debug async wait functions */
#define NLMDEBUG_STALE		0x00002000	/* debug stale file handles */
#define NLMDEBUG_NOTIFY		0x00004000	/* debug crash notification */
#define NLMDEBUG_CALL		0x00008000	/* debug NLM RPC calls */
#define NLMDEBUG_BLOCKED	0x00010000	/* debug blocked requests at server */
#define NLMDEBUG_LOCK		0x00020000	/* debug lock requests */
#define NLMDEBUG_CLEAN		0x00040000	/* debug lock cleaning */
#define NLMDEBUG_GRANTED	0x00080000	/* debug lock grants */

#define NLMDEBUG_ALL		0xffffffff	/* full debug printing */

struct km_wrap {
	int kw_size;
	int kw_req;
	void *kw_caller;
	struct km_wrap *kw_other;
	struct km_wrap *kw_next;
	struct km_wrap *kw_prev;
};

#define WRAPSIZE    ((sizeof(struct km_wrap) + sizeof(void *) - 1) & \
					~(sizeof(void *) - 1))

extern struct km_wrap *NLM_memlist;
extern int nlm_mem_usage;
extern int nlm_zone_usage;
extern lock_t nlm_kmem_lock;

/*
 * a valid address is not in kernel text and is not in page 0
 */
extern uint _ftext[];
extern uint _etext[];

#define VALID_ADDR(addr)    ((((u_long)(addr) > (u_long)_etext) || \
							((u_long)(addr) < (u_long)_ftext)) && \
							(pnum(addr) != 0))

#define NLM_KMEM_ALLOC(size, flag)  nlm_kmem_alloc(kmem_alloc, size, flag)
#define NLM_KMEM_ZALLOC(size, flag) nlm_kmem_alloc(kmem_zalloc, size, flag)
#define NLM_KMEM_FREE(addr, len)	nlm_kmem_free((caddr_t)addr, len); \
									addr = NULL
#define NLM_ZONE_ALLOC(zone, flag)  nlm_zone_alloc(zone, flag)
#define NLM_ZONE_ZALLOC(zone, flag) nlm_zone_zalloc(zone, flag)
#define NLM_ZONE_FREE(zone, addr)	nlm_zone_free(zone, (void *)addr); \
									addr = NULL

extern int nlm_debug;

extern char *locktype_to_str(ushort lktype);
extern char *lockcmd_to_str(int cmd);
extern char *netobj_to_str(netobj *obj);
extern char *nlmproc_to_str(int, int);
extern void *nlm_kmem_alloc(void *(*)(size_t, int), int, int);
extern void nlm_kmem_free(caddr_t, int);
extern void *nlm_zone_alloc(zone_t *, int);
extern void *nlm_zone_zalloc(zone_t *, int);
extern void nlm_zone_free(zone_t *, void *);
extern int nlm_zone_validate(caddr_t, int);
extern int nlm_kmem_validate(caddr_t, int);
extern void nlmdebug_init(void);

#else /* DEBUG */
/*
 * NLM debugging is only included with DEBUG defined
 */
#define NLM_DEBUG(level, op)

#define NLM_KMEM_ALLOC(size, flag)  kmem_alloc(size, flag)
#define NLM_KMEM_ZALLOC(size, flag) kmem_zalloc(size, flag)
#define NLM_KMEM_FREE(addr, len)	kmem_free((caddr_t)addr, len); addr = NULL
#define NLM_ZONE_ALLOC(zone, flag)  kmem_zone_alloc(zone, flag)
#define NLM_ZONE_ZALLOC(zone, flag) kmem_zone_zalloc(zone, flag)
#define NLM_ZONE_FREE(zone, addr)	kmem_zone_free(zone, (void *)addr); \
									addr = NULL

#endif /* DEBUG */

extern char *nlmstats_to_str(nlm_stats stat);
extern char *nlm4stats_to_str(nlm4_stats stat);
