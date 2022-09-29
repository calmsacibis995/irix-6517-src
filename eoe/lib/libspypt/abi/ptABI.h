#ifndef ABIDEFS
#define ABIDEFS
struct pt_lib_info_tInfo {
	int	pt_lib_info_t_size;
	int	_ptl_info_version;
	int	_ptl_pt_tbl;
	int	_ptl_pt_count;
};
extern struct pt_lib_info_tInfo *pt_lib_info_tInfo[];

struct pt_tInfo {
	int	pt_t_size;
	int	_pt_vp;
	int	_pt_label;
	int	_pt_state;
	int	_pt_bits;
	int	_pt_blocked;
	int	_pt_occupied;
	int	_pt_stk;
	int	_pt_sync;
	int	_pt_context;
};
extern struct pt_tInfo *pt_tInfo[];

struct vp_tInfo {
	int	vp_t_size;
	int	_vp_state;
	int	_vp_pid;
	int	_vp_pt;
};
extern struct vp_tInfo *vp_tInfo[];

struct mtx_tInfo {
	int	mtx_t_size;
	int	_mtx_attr;
	int	_mtx_owner;
	int	_mtx_waitq;
	int	_mtx_thread;
	int	_mtx_pid;
	int	_mtx_waiters;
};
extern struct mtx_tInfo *mtx_tInfo[];

struct mtxattr_tInfo {
	int	mtxattr_t_size;
	int	_ma_protocol;
	int	_ma_priority;
	int	_ma_type;
};
extern struct mtxattr_tInfo *mtxattr_tInfo[];

struct prda_lib_tInfo {
	int	prda_lib_t_size;
	int	_pthread_data;
};
extern struct prda_lib_tInfo *prda_lib_tInfo[];

struct prda_sys_tInfo {
	int	prda_sys_t_size;
	int	_t_rpid;
};
extern struct prda_sys_tInfo *prda_sys_tInfo[];


#endif
