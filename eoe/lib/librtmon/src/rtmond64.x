struct taskSpawnRec { /* UNKNOWN AND UNIMPLEMENTED BY SGI */
    char *name;
    u_int priority;
    u_int options;
    u_int stackSize;
    u_int entry;
    u_int arg[10];
};

struct callFuncRec { /* UNKNOWN AND UNIMPLEMENTED BY SGI */
    u_int funcPtr;
    u_int arg[20];
};

struct evtLogRec { /* control rtmon CPU event data collection stream */
    u_int mode;		/* CPU# for log control */
    u_int state;	/* > 0: setup/resume stream; <= 0: pause stream */
    u_int portNo;	/* client port number of data stream */
};

#if defined(RPC_HDR)
/*
 * RPC support for 64-bit parameters is somewhat flakey.  The XDR routines
 * provided in the default library are for ``u_longlong_t'' and ``uint64.''
 * Unfortunately neither of those is a defined type in any header file and,
 * in fact, even <rpc/xdr.h> was forced top work around this by declaring
 * the actual parameter types as __uint64_t ... (sigh)
 */
%typedef __uint64_t u_longlong_t;
#endif

struct rtmond64_get_connection_args {
    u_int protocol;	/* rtmond data stream protocol */
    u_int cpu;		/* CPU# to collect data from */
    u_longlong_t mask;	/* mask of events to collect */
    u_int port;		/* port number client is waiting in accept() on */
};

struct rtmond64_suspend_resume_args {
    u_int cpu;		/* CPU# to suspend/resume data flow on */
    u_int port;		/* client's connection port number */
};

program RTMOND_SVC
{
    version RTMOND_WINDVIEW {
	u_int WVPROC_SYMTAB_LOOKUP(string) = 1;
	u_int WVPROC_TASK_SPAWN(taskSpawnRec) = 2;
	u_int WVPROC_CALL_FUNCTION(callFuncRec) = 3;
	u_int WVPROC_EVT_LOG_CONTROL(evtLogRec) = 4;
    } = 1;
    version RTMOND_SGI1 {
	u_int RTMOND64_GET_NCPU(void) = 1;
	u_int RTMOND64_GET_CONNECTION(rtmond64_get_connection_args) = 2;
	u_int RTMOND64_SUSPEND_CONNECTION(rtmond64_suspend_resume_args) = 3;
	u_int RTMOND64_RESUME_CONNECTION(rtmond64_suspend_resume_args) = 4;
    } = 2;
} = 50000;
