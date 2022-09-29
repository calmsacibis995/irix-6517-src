extern void destroy_client_shares(struct svc_req *, char *);
extern void share_init(void);
extern void nlm_share_options(int);
extern void proc_nlm_share(struct svc_req *, nlm4_shareargs *,
	nlm4_shareres *, struct exportinfo *);
