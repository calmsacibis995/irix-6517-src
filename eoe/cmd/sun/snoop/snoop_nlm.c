/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/snoop_nlm.c,v 1.1 1996/06/19 20:34:02 nn Exp $"

#include <sys/types.h>
#include <setjmp.h>
#include <string.h>

#ifdef notdef
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/rpc_msg.h>
#endif /* notdef */
#ifdef sgi
#include <rpc/rpc.h>
#endif
#include <rpcsvc/nlm_prot.h>
#include "snoop.h"

extern char *dlc_header;
extern jmp_buf xdr_err;

extern void check_retransmit();
static void interpret_nlm_1();
static void interpret_nlm_3();
static void interpret_nlm_4();
static char *nameof_access();
static char *nameof_mode();
static char *nameof_stat();
static char *nameof_stat4();
static void show_cancargs();
static void show_cancargs4();
static void show_lock();
static void show_lock4();
static void show_lockargs();
static void show_lockargs4();
static void show_netobj();
static void show_nlm_access();
static void show_nlm_mode();
static void show_notify();
static void show_res();
static void show_res4();
static void show_share();
static void show_shareargs();
static void show_shareres();
static void show_shareres4();
static enum nlm_stats show_stat();
static enum nlm4_stats show_stat4();
static void show_testargs();
static void show_testargs4();
static void show_testres();
static void show_testres4();
static void show_unlockargs();
static void show_unlockargs4();
static void skip_netobj();
static char *sum_lock();
static char *sum_lock4();
static char *sum_netobj();
static char *sum_notify();
static char *sum_share();

void
interpret_nlm(flags, type, xid, vers, proc, data, len)
	int flags, type, xid, vers, proc;
	char *data;
	int len;
{
	switch (vers) {
	case 1:	interpret_nlm_1(flags, type, xid, vers, proc, data, len);
		break;
	case 3:	interpret_nlm_3(flags, type, xid, vers, proc, data, len);
		break;
	case 4:	interpret_nlm_4(flags, type, xid, vers, proc, data, len);
		break;
	}
}


/* ------------  V E R S I O N   1  ---------------------------------- */

static char *procnames_short_1[] = {
	"Null1",	/* 0 */
	"TEST1",	/* 1 */
	"LOCK1",	/* 2 */
	"CANCEL1",	/* 3 */
	"UNLOCK1",	/* 4 */
	"GRANTED1",	/* 5 */
	"TEST MSG1",	/* 6 */
	"LOCK MSG1",	/* 7 */
	"CANCEL MSG1",	/* 8 */
	"UNLOCK MSG1",	/* 9 */
	"GRANTED MSG1",	/* 10 */
	"TEST RES1",	/* 11 */
	"LOCK RES1",	/* 12 */
	"CANCEL RES1",	/* 13 */
	"UNLOCK RES1",	/* 14 */
	"GRANTED RES1",	/* 15 */
};

static char *procnames_long_1[] = {
	"Null procedure",	/* 0 */
	"Test",			/* 1 */
	"Lock",			/* 2 */
	"Cancel",		/* 3 */
	"Unlock",		/* 4 */
	"Granted",		/* 5 */
	"Test message",		/* 6 */
	"Lock message",		/* 7 */
	"Cancel message",	/* 8 */
	"Unlock message",	/* 9 */
	"Granted message",	/* 10 */
	"Test result",		/* 11 */
	"Lock result",		/* 12 */
	"Cancel result",	/* 13 */
	"Unlock result",	/* 14 */
	"Granted result",	/* 15 */
};

/* Highest procedure number that officially belongs to version 1. */
#define	MAXPROC_1	15

/* ARGSUSED */
static void
interpret_nlm_1(flags, type, xid, vers, proc, data, len)
	int flags, type, xid, vers, proc;
	char *data;
	int len;
{
	char *line;

	if (proc < 0 || proc > MAXPROC_1)
		return;

	if (flags & F_SUM) {
		if (setjmp(xdr_err)) {
			return;
		}

		line = get_sum_line();

		if (type == CALL) {
			(void) sprintf(line,
				"NLM C %s",
				procnames_short_1[proc]);
			line += strlen(line);
			switch (proc) {
			case NLM_TEST:
			case NLM_GRANTED:
			case NLM_TEST_MSG:
			case NLM_GRANTED_MSG:
				/* testargs */
				(void) strcat(line, sum_netobj("OH"));
				(void) getxdr_bool();	/* Excl */
				(void) strcat(line, sum_lock());
				break;
			case NLM_LOCK:
			case NLM_LOCK_MSG:
				/* lockargs */
				(void) strcat(line, sum_netobj("OH"));
				(void) getxdr_bool();	/* Block */
				(void) getxdr_bool();	/* Excl */
				(void) strcat(line, sum_lock());
				break;
			case NLM_CANCEL:
			case NLM_CANCEL_MSG:
				/* cancargs */
				(void) strcat(line, sum_netobj("OH"));
				(void) getxdr_bool();	/* Block */
				(void) getxdr_bool();	/* Excl */
				(void) strcat(line, sum_lock());
				break;
			case NLM_UNLOCK:
			case NLM_UNLOCK_MSG:
				/* unlockargs */
				(void) strcat(line, sum_netobj("OH"));
				(void) strcat(line, sum_lock());
				break;
			case NLM_TEST_RES:
				/* testres */
				(void) strcat(line, sum_netobj("OH"));
				(void) strcat(line, " ");
				(void) strcat(line,
				    nameof_stat(getxdr_u_long()));
				break;
			case NLM_LOCK_RES:
			case NLM_CANCEL_RES:
			case NLM_UNLOCK_RES:
			case NLM_GRANTED_RES:
				/* res */
				(void) strcat(line, sum_netobj("OH"));
				(void) strcat(line, " ");
				(void) strcat(line,
					nameof_stat(getxdr_u_long()));
				break;
			}
			check_retransmit(line, (u_long)xid);
		} else {
			(void) sprintf(line, "NLM R %s",
				procnames_short_1[proc]);
			line += strlen(line);
			switch (proc) {
			case NLM_TEST:
				/* testres */
				(void) strcat(line, sum_netobj("OH"));
				(void) strcat(line, " ");
				(void) strcat(line,
				    nameof_stat(getxdr_u_long()));
				break;
			case NLM_LOCK:
			case NLM_CANCEL:
			case NLM_UNLOCK:
			case NLM_GRANTED:
				/* res */
				(void) strcat(line, sum_netobj("OH"));
				(void) strcat(line, " ");
				(void) strcat(line,
					nameof_stat(getxdr_u_long()));
				break;
			}
		}
	}

	if (flags & F_DTAIL) {
		show_header("NLM:  ", "Network Lock Manager", len);
		show_space();
		if (setjmp(xdr_err)) {
			return;
		}
		(void) sprintf(get_line(0, 0),
			"Proc = %d (%s)",
			proc, procnames_long_1[proc]);
		if (type == CALL) {
			switch (proc) {
			case NLM_TEST:
			case NLM_GRANTED:
			case NLM_TEST_MSG:
			case NLM_GRANTED_MSG:
				show_testargs();
				break;
			case NLM_LOCK:
			case NLM_LOCK_MSG:
				show_lockargs();
				break;
			case NLM_CANCEL:
			case NLM_CANCEL_MSG:
				show_cancargs();
				break;
			case NLM_UNLOCK:
			case NLM_UNLOCK_MSG:
				show_unlockargs();
				break;
			case NLM_TEST_RES:
				show_testres();
				break;
			case NLM_LOCK_RES:
			case NLM_CANCEL_RES:
			case NLM_UNLOCK_RES:
			case NLM_GRANTED_RES:
				show_res();
				break;
			}
		} else {
			switch (proc) {
			case NLM_TEST:
				show_testres();
				break;
			case NLM_LOCK:
			case NLM_CANCEL:
			case NLM_UNLOCK:
			case NLM_GRANTED:
				show_res();
				break;
			case NLM_TEST_MSG:
			case NLM_LOCK_MSG:
			case NLM_CANCEL_MSG:
			case NLM_UNLOCK_MSG:
			case NLM_GRANTED_MSG:
			case NLM_TEST_RES:
			case NLM_LOCK_RES:
			case NLM_CANCEL_RES:
			case NLM_UNLOCK_RES:
			case NLM_GRANTED_RES:
				break;
			}
		}
		show_trailer();
	}
}

#define	roundup(sz) ((sz / 4 + (sz % 4 > 0)) * 4)

/*
 * Skip a netobj.
 * Make sure an integral number of words
 * are skipped.
 */
static void
skip_netobj()
{
	int sz = getxdr_u_long();

	xdr_skip(roundup(sz));
}

static char *
sum_netobj(handle)
	char *handle;
{
	int i, l, sz;
	int sum = 0;
	static char buff[32];

	sz = getxdr_u_long();
	for (i = 0; i < sz; i += 4) {
		l =  getxdr_long();
		sum ^= (l >> 16) ^ l;
	}
	(void) sprintf(buff, " %s=%04X", handle, sum & 0xFFFF);
	return (buff);
}

static void
show_netobj(fmt)
	char *fmt;
{
	int sz, chunk;
	char *p;
	char buff[64];
	int needspace;

	sz = getxdr_u_long();		/* size of the netobj */

	if (sz == 0) {
		(void) sprintf(get_line(0, 0), fmt, "<null>");
	} else {
		needspace = sz > 16;
		(void) strcpy(buff, fmt);
		while (sz > 0) {
			chunk = sz > 16 ? 16 : sz;
			sz -= 16;
			(void) showxdr_hex(chunk, buff);
			/*
			 * For every line after the first, blank out
			 * everything in the format string before the "%s".
			 */
			for (p = buff; *p != '%'; p++)
				*p = ' ';
		}
		if (needspace)
			show_space();
	}
}

static char *
sum_lock()
{
	static char buff[LM_MAXSTRLEN + 1];
	char *cp = buff;
	long id;
	u_long off, len;

	(void) getxdr_string(buff, LM_MAXSTRLEN);	/* Caller */
	(void) strcpy(buff, sum_netobj("FH"));		/* Fh */
	cp += strlen(buff);
	skip_netobj();					/* Owner */
	id  = getxdr_long();
	off = getxdr_u_long();
	len = getxdr_u_long();
	(void) sprintf(cp, " PID=%ld Region=%lu:%lu", id, off, len);
	return (buff);
}

static void
show_lock()
{
	showxdr_string(LM_MAXSTRLEN, "Caller = %s");
	show_netobj("Filehandle = %s");
	show_netobj("Lock owner = %s");
	showxdr_long("Svid = %ld (process id)");
	showxdr_u_long("Offset = %lu bytes");
	showxdr_u_long("Length = %lu bytes");
}

static void
show_cancargs()
{
	show_netobj("Cookie = %s");
	showxdr_bool("Block = %s");
	showxdr_bool("Exclusive = %s");
	show_lock();
}

static void
show_lockargs()
{
	show_netobj("Cookie = %s");
	showxdr_bool("Block = %s");
	showxdr_bool("Exclusive = %s");
	show_lock();
	showxdr_bool("Reclaim = %s");
	showxdr_long("State = %ld");
}

static void
show_unlockargs()
{
	show_netobj("Cookie = %s");
	show_lock();
}

static void
show_testargs()
{
	show_netobj("Cookie = %s");
	showxdr_bool("Exclusive = %s");
	show_lock();
}

static void
show_res()
{
	show_netobj("Cookie = %s");
	(void) show_stat();
}

static char *
nameof_stat(s)
	u_long s;
{
	switch ((enum nlm_stats) s) {
	case nlm_granted:	return ("granted");
	case nlm_denied:	return ("denied");
	case nlm_denied_nolocks:return ("denied (no locks)");
	case nlm_blocked:	return ("blocked");
	case nlm_denied_grace_period: return ("denied (grace period)");
	case nlm_deadlck:	return ("deadlock");
	default:		return ("?");
	}
}

static enum nlm_stats
show_stat()
{
	enum nlm_stats s;

	s = (enum nlm_stats) getxdr_u_long();
	(void) sprintf(get_line(0, 0),
		"Status = %d (%s)",
		s, nameof_stat((u_long)s));

	return (s);
}

static void
show_testres()
{
	show_netobj("Cookie = %s");
	if (show_stat() == nlm_denied) {
		showxdr_bool("Exclusive = %s");
		showxdr_long("Svid = %ld (process id)");
		show_netobj("Owner handle = %s");
		showxdr_u_long("Offset = %lu bytes");
		showxdr_u_long("Length = %lu bytes");
	}
}


/* ------------  V E R S I O N   3  ---------------------------------- */

static char *procnames_short_3[] = {
	"SHARE3",	/* 20 */
	"UNSHARE3",	/* 21 */
	"NM_LOCK3",	/* 22 */
	"FREEALL3",	/* 23 */
};

static char *procnames_long_3[] = {
	"Share",		/* 20 */
	"Unshare",		/* 21 */
	"Unmonitored lock",	/* 22 */
	"Free all",		/* 23 */
};

/* Maximum procedure number for version 3. */
#define	MAXPROC_3	23

static void
interpret_nlm_3(flags, type, xid, vers, proc, data, len)
	int flags, type, xid, vers, proc;
	char *data;
	int len;
{
	char *line, *pl;
	u_long i;

	if (proc < 0 || proc > MAXPROC_3)
		return;

	/*
	 * Version 3 is a superset of version 1
	 */
	if (proc >= 0 && proc <= MAXPROC_1) {
		interpret_nlm_1(flags, type, xid, vers, proc, data, len);
		return;
	}

	if (flags & F_SUM) {
		if (setjmp(xdr_err)) {
			return;
		}

		line = get_sum_line();

		if (type == CALL) {
			(void) sprintf(line,
				"NLM C %s",
				procnames_short_3[proc-20]);
			line += strlen(line);
			switch (proc) {
			case NLM_SHARE:
			case NLM_UNSHARE:
				(void) strcat(line, sum_netobj("OH"));
				(void) strcat(line, sum_share());
				break;
			case NLM_NM_LOCK:
				/* lockargs */
				skip_netobj();
				(void) getxdr_u_long(); /* Block */
				(void) getxdr_u_long(); /* Excl */
				(void) strcat(line, sum_lock());
				break;
			case NLM_FREE_ALL:
				(void) sprintf(line,
					" %s", sum_notify());
				break;
			}
			check_retransmit(line, (u_long)xid);
		} else {
			(void) sprintf(line, "NLM R %s",
				procnames_short_3[proc-20]);
			line += strlen(line);
			switch (proc) {
			case NLM_SHARE:
			case NLM_UNSHARE:
				pl = sum_netobj("OH");
				i = getxdr_u_long();
				sprintf(line, "%s %s %ld",
					pl, nameof_stat(i), getxdr_long());
				break;
			case NLM_NM_LOCK:
				/* res */
				(void) strcat(line, sum_netobj("OH"));
				(void) strcat(line, " ");
				(void) strcat(line,
					nameof_stat(getxdr_u_long()));
				break;
			case NLM_FREE_ALL:
				break;
			}
		}
	}

	if (flags & F_DTAIL) {
		show_header("NLM:  ", "Network Lock Manager", len);
		show_space();
		if (setjmp(xdr_err)) {
			return;
		}
		(void) sprintf(get_line(0, 0),
			"Proc = %d (%s)",
			proc, procnames_long_3[proc-20]);
		if (type == CALL) {
			switch (proc) {
			case NLM_SHARE:
			case NLM_UNSHARE:
				show_shareargs();
				break;
			case NLM_NM_LOCK:
				show_lockargs();
				break;
			case NLM_FREE_ALL:
				show_notify();
				break;
			}
		} else {
			switch (proc) {
			case NLM_SHARE:
			case NLM_UNSHARE:
				show_shareres();
				break;
			case NLM_NM_LOCK:
				show_res();
				break;
			case NLM_FREE_ALL:
				break;
			}
		}
		show_trailer();
	}
}

static char *
nameof_mode(m)
	u_int m;
{
	switch ((enum fsh_mode) m) {
	case fsm_DN:	return ("deny none");
	case fsm_DR:	return ("deny read");
	case fsm_DW:	return ("deny write");
	case fsm_DRW:	return ("deny read/write");
	default:	return ("?");
	}
}

static char *
nameof_access(a)
	u_int a;
{
	switch ((enum fsh_access) a) {
	case fsa_NONE:	return ("?");
	case fsa_R:	return ("read only");
	case fsa_W:	return ("write only");
	case fsa_RW:	return ("read/write");
	default:	return ("?");
	}
}

static void
show_nlm_mode()
{
	enum fsh_mode m;

	m = (enum fsh_mode) getxdr_u_long();
	(void) sprintf(get_line(0, 0),
		"Mode = %d (%s)",
		m, nameof_mode((u_int)m));
}

static void
show_nlm_access()
{
	enum fsh_access a;

	a = (enum fsh_access) getxdr_u_long();
	(void) sprintf(get_line(0, 0),
		"Access = %d (%s)",
		a, nameof_access((u_int)a));
}

static char *
sum_share()
{
	static char buff[LM_MAXSTRLEN + 1];
	char *cp = buff;
	u_long mode, access;

	(void) getxdr_string(buff, LM_MAXSTRLEN);	/* Caller */
	(void) strcpy(buff, sum_netobj("FH"));		/* Fh */
	cp += strlen(buff);
	skip_netobj();					/* Owner */
	mode = getxdr_u_long();
	access = getxdr_u_long();
	(void) sprintf(cp, " Mode=%lu Access=%lu", mode, access);
	return (buff);
}

static void
show_share()
{
	showxdr_string(LM_MAXSTRLEN, "Caller = %s");
	show_netobj("Filehandle = %s");
	show_netobj("Lock owner = %s");
	show_nlm_mode();
	show_nlm_access();
}

static void
show_shareargs()
{
	show_netobj("Cookie = %s");
	show_share();
	showxdr_bool("Reclaim = %s");
}

static void
show_shareres()
{
	show_netobj("Cookie = %s");
	(void) show_stat();
	showxdr_long("Sequence = %d");
}

static void
show_notify()
{
	showxdr_string(MAXNAMELEN, "Name = %s");
	showxdr_long("State = %d");
}

static char *
sum_notify()
{
	static char buff[MAXNAMELEN + 16];
	char *cp = buff;
	long state;

	(void) getxdr_string(buff, MAXNAMELEN);
	cp += strlen(buff);
	state  = getxdr_long();
	(void) sprintf(cp, " State=%ld", state);
	return (buff);
}

/* ------------  V E R S I O N   4  ---------------------------------- */

static char *procnames_short_4[] = {
	"Null4",	/* 0 */
	"TEST4",	/* 1 */
	"LOCK4",	/* 2 */
	"CANCEL4",	/* 3 */
	"UNLOCK4",	/* 4 */
	"GRANTED4",	/* 5 */
	"TEST MSG4",	/* 6 */
	"LOCK MSG4",	/* 7 */
	"CANCEL MSG4",	/* 8 */
	"UNLOCK MSG4",	/* 9 */
	"GRANTED MSG4",	/* 10 */
	"TEST RES4",	/* 11 */
	"LOCK RES4",	/* 12 */
	"CANCEL RES4",	/* 13 */
	"UNLOCK RES4",	/* 14 */
	"GRANTED RES4",	/* 15 */
	"PROC 16 v4",	/* 16 */
	"PROC 17 v4",	/* 17 */
	"PROC 18 v4",	/* 18 */
	"PROC 19 v4",	/* 19 */
	"SHARE4",	/* 20 */
	"UNSHARE4",	/* 21 */
	"NM_LOCK4",	/* 22 */
	"FREEALL4",	/* 23 */
};

static char *procnames_long_4[] = {
	"Null procedure",	/* 0 */
	"Test",			/* 1 */
	"Lock",			/* 2 */
	"Cancel",		/* 3 */
	"Unlock",		/* 4 */
	"Granted",		/* 5 */
	"Test message",		/* 6 */
	"Lock message",		/* 7 */
	"Cancel message",	/* 8 */
	"Unlock message",	/* 9 */
	"Granted message",	/* 10 */
	"Test result",		/* 11 */
	"Lock result",		/* 12 */
	"Cancel result",	/* 13 */
	"Unlock result",	/* 14 */
	"Granted result",	/* 15 */
	"Procedure 16",		/* 16 */
	"Procedure 17",		/* 17 */
	"Procedure 18",		/* 18 */
	"Procedure 19",		/* 19 */
	"Share",		/* 20 */
	"Unshare",		/* 21 */
	"Unmonitored lock",	/* 22 */
	"Free all",		/* 23 */
};

/* Maximum procedure number for version 4. */
#define	MAXPROC_4	23

#ifdef sgi
/*
 * Looks like jmy mapped all the NLMPROC4_XX to NLMPROC_XX so
 * I'll remap here so the below references will work.
 */
#define	NLMPROC4_NULL	NLMPROC_NULL
#define	NLMPROC4_TEST	NLMPROC_TEST
#define	NLMPROC4_LOCK	NLMPROC_LOCK
#define	NLMPROC4_CANCEL	NLMPROC_CANCEL
#define	NLMPROC4_UNLOCK	NLMPROC_UNLOCK
#define	NLMPROC4_GRANTED	NLMPROC_GRANTED
#define	NLMPROC4_TEST_MSG	NLMPROC_TEST_MSG
#define	NLMPROC4_LOCK_MSG	NLMPROC_LOCK_MSG
#define	NLMPROC4_CANCEL_MSG	NLMPROC_CANCEL_MSG
#define	NLMPROC4_UNLOCK_MSG	NLMPROC_UNLOCK_MSG
#define	NLMPROC4_GRANTED_MSG	NLMPROC_GRANTED_MSG
#define	NLMPROC4_TEST_RES	NLMPROC_TEST_RES
#define	NLMPROC4_LOCK_RES	NLMPROC_LOCK_RES
#define	NLMPROC4_CANCEL_RES	NLMPROC_CANCEL_RES
#define	NLMPROC4_UNLOCK_RES	NLMPROC_UNLOCK_RES
#define	NLMPROC4_GRANTED_RES	NLMPROC_GRANTED_RES
#define	NLMPROC4_SHARE	NLMPROC_SHARE
#define	NLMPROC4_UNSHARE	NLMPROC_UNSHARE
#define	NLMPROC4_NM_LOCK	NLMPROC_NM_LOCK
#define	NLMPROC4_FREE_ALL	NLMPROC_FREE_ALL
#endif

/* ARGSUSED */
static void
interpret_nlm_4(flags, type, xid, vers, proc, data, len)
	int flags, type, xid, vers, proc;
	char *data;
	int len;
{
	char *line;
	char *pl;
	u_long i;

	if (proc < 0 || proc > MAXPROC_4)
		return;

	if (flags & F_SUM) {
		if (setjmp(xdr_err)) {
			return;
		}

		line = get_sum_line();

		if (type == CALL) {
			(void) sprintf(line,
				"NLM C %s",
				procnames_short_4[proc]);
			line += strlen(line);
			switch (proc) {
			case NLMPROC4_TEST:
			case NLMPROC4_GRANTED:
			case NLMPROC4_TEST_MSG:
			case NLMPROC4_GRANTED_MSG:
				/* testargs */
				(void) strcat(line, sum_netobj("OH"));
				(void) getxdr_bool();	/* Excl */
				(void) strcat(line, sum_lock4());
				break;
			case NLMPROC4_LOCK:
			case NLMPROC4_LOCK_MSG:
				/* lockargs */
				(void) strcat(line, sum_netobj("OH"));
				(void) getxdr_bool();	/* Block */
				(void) getxdr_bool();	/* Excl */
				(void) strcat(line, sum_lock4());
				/* ignore reclaim, state fields */
				break;
			case NLMPROC4_CANCEL:
			case NLMPROC4_CANCEL_MSG:
				/* cancargs */
				(void) strcat(line, sum_netobj("OH"));
				(void) getxdr_bool();	/* Block */
				(void) getxdr_bool();	/* Excl */
				(void) strcat(line, sum_lock4());
				break;
			case NLMPROC4_UNLOCK:
			case NLMPROC4_UNLOCK_MSG:
				/* unlockargs */
				(void) strcat(line, sum_netobj("OH"));
				(void) strcat(line, sum_lock4());
				break;
			case NLMPROC4_TEST_RES:
				/* testres */
				(void) strcat(line, sum_netobj("OH"));
				(void) strcat(line, " ");
				(void) strcat(line,
				    nameof_stat4(getxdr_u_long()));
				break;
			case NLMPROC4_LOCK_RES:
			case NLMPROC4_CANCEL_RES:
			case NLMPROC4_UNLOCK_RES:
			case NLMPROC4_GRANTED_RES:
				/* res */
				(void) strcat(line, sum_netobj("OH"));
				(void) strcat(line, " ");
				(void) strcat(line,
					nameof_stat4(getxdr_u_long()));
				break;
			case NLMPROC4_SHARE:
			case NLMPROC4_UNSHARE:
				(void) strcat(line, sum_netobj("OH"));
				(void) strcat(line, sum_share());
				break;
			case NLMPROC4_NM_LOCK:
				/* lockargs */
				skip_netobj();		/* Cookie */
				(void) getxdr_bool();	/* Block */
				(void) getxdr_bool();	/* Excl */
				(void) strcat(line, sum_lock4());
				/* skip reclaim & state fields */
				break;
			case NLMPROC4_FREE_ALL:
				(void) sprintf(line,
					" %s", sum_notify());
				break;
			}
			check_retransmit(line, (u_long)xid);
		} else {
			(void) sprintf(line, "NLM R %s",
				procnames_short_4[proc]);
			line += strlen(line);
			switch (proc) {
			case NLMPROC4_TEST:
				/* testres */
				(void) strcat(line, sum_netobj("OH"));
				(void) strcat(line, " ");
				(void) strcat(line,
				    nameof_stat4(getxdr_u_long()));
				break;
			case NLMPROC4_LOCK:
			case NLMPROC4_CANCEL:
			case NLMPROC4_UNLOCK:
			case NLMPROC4_GRANTED:
			case NLMPROC4_NM_LOCK:
				/* res */
				(void) strcat(line, sum_netobj("OH"));
				(void) strcat(line, " ");
				(void) strcat(line,
					nameof_stat4(getxdr_u_long()));
				break;
			case NLMPROC4_SHARE:
			case NLMPROC4_UNSHARE:
				/* shareres */
				pl = sum_netobj("OH");
				i = getxdr_u_long();
				sprintf(line, "%s %s %ld",
					pl, nameof_stat4(i), getxdr_long());
				break;
			case NLMPROC4_FREE_ALL:
				break;
			}
		}
	}

	if (flags & F_DTAIL) {
		show_header("NLM:  ", "Network Lock Manager", len);
		show_space();
		if (setjmp(xdr_err)) {
			return;
		}
		(void) sprintf(get_line(0, 0),
			"Proc = %d (%s)",
			proc, procnames_long_4[proc]);
		if (type == CALL) {
			switch (proc) {
			case NLMPROC4_TEST:
			case NLMPROC4_GRANTED:
			case NLMPROC4_TEST_MSG:
			case NLMPROC4_GRANTED_MSG:
				show_testargs4();
				break;
			case NLMPROC4_LOCK:
			case NLMPROC4_LOCK_MSG:
			case NLMPROC4_NM_LOCK:
				show_lockargs4();
				break;
			case NLMPROC4_CANCEL:
			case NLMPROC4_CANCEL_MSG:
				show_cancargs4();
				break;
			case NLMPROC4_UNLOCK:
			case NLMPROC4_UNLOCK_MSG:
				show_unlockargs4();
				break;
			case NLMPROC4_TEST_RES:
				show_testres4();
				break;
			case NLMPROC4_LOCK_RES:
			case NLMPROC4_CANCEL_RES:
			case NLMPROC4_UNLOCK_RES:
			case NLMPROC4_GRANTED_RES:
				show_res4();
				break;
			case NLMPROC4_SHARE:
			case NLMPROC4_UNSHARE:
				show_shareargs();
				break;
			case NLMPROC4_FREE_ALL:
				show_notify();
				break;
			}
		} else {
			switch (proc) {
			case NLMPROC4_TEST:
				show_testres4();
				break;
			case NLMPROC4_LOCK:
			case NLMPROC4_CANCEL:
			case NLMPROC4_UNLOCK:
			case NLMPROC4_GRANTED:
			case NLM_NM_LOCK:
				show_res4();
				break;
			case NLMPROC4_TEST_MSG:
			case NLMPROC4_LOCK_MSG:
			case NLMPROC4_CANCEL_MSG:
			case NLMPROC4_UNLOCK_MSG:
			case NLMPROC4_GRANTED_MSG:
			case NLMPROC4_TEST_RES:
			case NLMPROC4_LOCK_RES:
			case NLMPROC4_CANCEL_RES:
			case NLMPROC4_UNLOCK_RES:
			case NLMPROC4_GRANTED_RES:
				break;
			case NLM_SHARE:
			case NLM_UNSHARE:
				show_shareres4();
				break;
			case NLM_FREE_ALL:
				break;
			}
		}
		show_trailer();
	}
}

static char *
sum_lock4()
{
	static char buff[LM_MAXSTRLEN + 1];
	char *cp = buff;
	long id;
	u_longlong_t off, len;

	(void) getxdr_string(buff, LM_MAXSTRLEN);	/* Caller */
	(void) strcpy(buff, sum_netobj("FH"));		/* Fh */
	cp += strlen(buff);
	skip_netobj();					/* Owner */
	id  = getxdr_long();
	off = getxdr_u_longlong();
	len = getxdr_u_longlong();
	(void) sprintf(cp, " PID=%ld Region=%llu:%llu", id, off, len);
	return (buff);
}

static void
show_lock4()
{
	showxdr_string(LM_MAXSTRLEN, "Caller = %s");
	show_netobj("Filehandle = %s");
	show_netobj("Lock owner = %s");
	showxdr_long("Svid = %ld (process id)");
	showxdr_u_longlong("Offset = %llu bytes");
	showxdr_u_longlong("Length = %llu bytes");
}

static void
show_cancargs4()
{
	show_netobj("Cookie = %s");
	showxdr_bool("Block = %s");
	showxdr_bool("Exclusive = %s");
	show_lock4();
}

static void
show_lockargs4()
{
	show_netobj("Cookie = %s");
	showxdr_bool("Block = %s");
	showxdr_bool("Exclusive = %s");
	show_lock4();
	showxdr_bool("Reclaim = %s");
	showxdr_long("State = %ld");
}

static void
show_unlockargs4()
{
	show_netobj("Cookie = %s");
	show_lock4();
}

static void
show_testargs4()
{
	show_netobj("Cookie = %s");
	showxdr_bool("Exclusive = %s");
	show_lock4();
}

static void
show_res4()
{
	show_netobj("Cookie = %s");
	(void) show_stat4();
}

static char *
nameof_stat4(s)
	u_long s;
{
	switch ((enum nlm4_stats) s) {
	case NLM4_GRANTED:	return ("granted");
	case NLM4_DENIED:	return ("denied");
	case NLM4_DENIED_NOLOCKS:return ("denied (no locks)");
	case NLM4_BLOCKED:	return ("blocked");
	case NLM4_DENIED_GRACE_PERIOD: return ("denied (grace period)");
	case NLM4_DEADLCK:	return ("deadlock");
	case NLM4_ROFS:		return ("read-only fs");
	case NLM4_STALE_FH:	return ("stale fh");
	case NLM4_FBIG:		return ("file too big");
	case NLM4_FAILED:	return ("failed");
	default:		return ("?");
	}
}

static enum nlm4_stats
show_stat4()
{
	enum nlm4_stats s;

	s = (enum nlm4_stats) getxdr_u_long();
	(void) sprintf(get_line(0, 0),
		"Status = %d (%s)",
		s, nameof_stat4((u_long)s));

	return (s);
}

static void
show_testres4()
{
	show_netobj("Cookie = %s");
	if (show_stat() == NLM4_DENIED) {
		showxdr_bool("Exclusive = %s");
		showxdr_long("Svid = %ld (process id)");
		show_netobj("Owner handle = %s");
		showxdr_u_longlong("Offset = %llu bytes");
		showxdr_u_longlong("Length = %llu bytes");
	}
}

static void
show_shareres4()
{
	show_netobj("Cookie = %s");
	(void) show_stat4();
	showxdr_long("Sequence = %d");
}
