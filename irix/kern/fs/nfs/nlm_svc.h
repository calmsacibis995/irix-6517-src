void dup_netobj(netobj *, netobj *);
void free_netobj(netobj *);
void release_all_locks(struct vfs *);
void lockd_cleanlocks(__uint32_t sysid);
int get_address(u_long, long, struct sockaddr_in *, bool_t);
nlm4_stats errno_to_nlm4(int error);
int local_lock(struct exportinfo *, fhandle_t *, struct flock *, int);
int local_unlock(struct exportinfo *, fhandle_t *, struct flock *, int);
int nlm_test_exi(struct svc_req *, union nlm_args_u *, struct exportinfo **);
int proc_nlm_test(struct svc_req *, union nlm_args_u *, union nlm_res_u *,
	struct exportinfo *, nlm4_stats *);
int nlm_lock_exi(struct svc_req *, union nlm_args_u *, struct exportinfo **);
int proc_nlm_lock(struct svc_req *, union nlm_args_u *, union nlm_res_u *,
	struct exportinfo *, nlm4_stats *);
int nlm_unlock_exi(struct svc_req *, union nlm_args_u *, struct exportinfo **);
int proc_nlm_unlock(struct svc_req *, union nlm_args_u *, union nlm_res_u *,
	struct exportinfo *, nlm4_stats *);
int nlm_cancel_exi(struct svc_req *, union nlm_args_u *, struct exportinfo **);
int proc_nlm_cancel(struct svc_req *, union nlm_args_u *, union nlm_res_u *,
	struct exportinfo *, nlm4_stats *);
int nlm_free_all(struct svc_req *, union nlm_args_u *,
	union nlm_res_u *, struct exportinfo *, nlm4_stats *);
int nlm4_test_exi(struct svc_req *, union nlm_args_u *, struct exportinfo **);
int proc_nlm4_test(struct svc_req *, union nlm_args_u *, union nlm_res_u *,
	struct exportinfo *, nlm4_stats *);
int nlm4_lock_exi(struct svc_req *, union nlm_args_u *, struct exportinfo **);
int proc_nlm4_lock(struct svc_req *, union nlm_args_u *, union nlm_res_u *,
	struct exportinfo *, nlm4_stats *);
int nlm4_unlock_exi(struct svc_req *, union nlm_args_u *, struct exportinfo **);
int proc_nlm4_unlock(struct svc_req *, union nlm_args_u *, union nlm_res_u *,
	struct exportinfo *, nlm4_stats *);
int nlm4_cancel_exi(struct svc_req *, union nlm_args_u *, struct exportinfo **);
int proc_nlm4_cancel(struct svc_req *, union nlm_args_u *, union nlm_res_u *,
	struct exportinfo *, nlm4_stats *);
int proc_nlm4_share(struct svc_req *, union nlm_args_u *, union nlm_res_u *,
	struct exportinfo *, nlm4_stats *);
int nlm4_share_exi(struct svc_req *, union nlm_args_u *, struct exportinfo **);
int nlm4_free_all(struct svc_req *, union nlm_args_u *, union nlm_res_u *,
	struct exportinfo *, nlm4_stats *);
void nlmsvc_init(void);
int nlm_reply(struct svc_req *, xdrproc_t, union nlm_res_u *);
void export_unlock(void);
