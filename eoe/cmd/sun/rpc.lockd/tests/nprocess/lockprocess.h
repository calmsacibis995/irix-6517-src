struct proclist {
	pid_t			pl_pid;
	int				pl_pipefd;
	struct proclist	*pl_next;
};

typedef struct proclist proclist_t;

pid_t newprocess(unsigned, proclist_t **);
int reap_processes(proclist_t *);
