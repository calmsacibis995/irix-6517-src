#ifdef sgi

struct rtuentry {
	u_long	rtu_hash;
	struct	sockaddr rtu_dst;
	struct	sockaddr rtu_router;
	short	rtu_rtflags; /* used by rtioctl */
	short	rtu_wasted[5];
	int	rtu_flags;
	int	rtu_state;
	int	rtu_timer;
	int	rtu_metric;
	int	rtu_ifmetric;
	struct	interface *rtu_ifp;
} rtu_entry;

struct rt_entry {
	struct	rt_entry *rt_forw;
	struct	rt_entry *rt_back;
	union {
#ifndef RTM_ADD
		struct	rtentry rtu_rt;
#endif
		struct rtuentry rtu_entry;
	} rt_rtu;
};
#endif
