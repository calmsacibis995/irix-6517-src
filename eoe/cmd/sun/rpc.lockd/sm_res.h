/*
 * @(#)sm_res.h	1.2 90/07/23 NFSSRC4.0 1.5 88/02/07
 */

struct stat_res{
	res res_stat;
	union {
		sm_stat_res stat;
		int rpc_err;
	}u;
};
#define sm_stat 	u.stat.res_stat
#define sm_state 	u.stat.state

