void nlm_testrply_to_flock(nlm_testrply *rep, flock_t *ld);
void nlm4_testrply_to_flock(nlm4_testrply *rep, flock_t *ld);
void nlm_grace_wakeup(sema_t *sp, struct kthread *);
int nlm_rpc_call(lockreq_t *lrp, int nlmvers, cred_t *cred, int retry,
	int signals);
int nlm1_call_setup(int cmd, lockreq_t *lrp, flock_t *ld, long *cookie,
	int reclaim, int async);
int nlm4_call_setup(int cmd, lockreq_t *lrp, flock_t *ld, long *cookie,
	int reclaim, int async);
void nlm4_freeargs(int proc, union nlm4_args_u *args);
void nlm1_freeargs(int proc, union nlm1_args_u *args);
int record_remote_name(char *name);
