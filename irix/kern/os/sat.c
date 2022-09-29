/*
 * Copyright 1990, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */
#ident	"$Revision: 1.122 $"

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/sat.h>
#include <sys/mac_label.h>
#include <sys/capability.h>
#include <sys/systm.h>
#include <sys/prctl.h>
#include <sys/kmem.h>
#include <sys/uthread.h>
#include <sys/mbuf.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <net/if.h>
#include <sys/acct.h>
#include <sys/extacct.h>
#include <sys/ipc.h>
#include <sys/cred.h>
#include <ksys/cred.h>
#include <sys/ptrace.h>
#include <sys/proc.h>
#include <os/proc/pproc_private.h>
#include <sys/arsess.h>
#include <sys/pathname.h>
#include <sys/mode.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys.s>

#define _THS_ sizeof(sat_token_header_t)
#define _TDS_ 400

struct sat_token {
	struct sat_token	*t_next;	/* If there's more data */
	struct sat_token	*t_act;		/* For linking together */
	char			*t_copyfrom;	/* data copied out to */
	int			t_flags;	/* useage flags */
	int			t_len;		/* length of data */
	union {
		sat_token_header_t	tu_header;	/* token header */
		char			tu_data[_TDS_];	/* token data */
	}			t_union;
};

#define t_header	t_union.tu_header
#define t_data		t_union.tu_data

#define TF_HEADER	0x01
#define TF_NO_HEADER	0x02

extern mac_label *mac_admin_high_lp;

/*
 * lock types -- lock_t is a "spinlock", which implements a busy-wait
 * lock for very short-term waiting.  sema_t is a long-term "sleep" lock
 * used for waiting for resources.
 */

static lock_t sat_qptrlock;	/* sat_queue_head/tail access/modify */
static sv_t sat_qlock;	/* wake sat_read when records are avail */

static sat_token_t sat_queue_head = NULL;
static sat_token_t sat_queue_tail = NULL;

/*
 *  Sat audit information.
 */
static struct zone *sat_proc_zone;
static struct zone *sat_token_zone;

struct uthread_s *satd_uthread = NULL;	/* uthread of the "registered" satd */

static sat_event_mask audit_state;

extern u_char miniroot;

#define min(a,b)	((a<b)? a: b)

static void
sat_free_token(sat_token_t token)
{
	if ((token->t_flags & 0xffff0000) != 0xca5e0000)
		cmn_err(CE_PANIC,"Bad SAT deallocation 0x%x %d",token,__LINE__);
	token->t_flags = 0;
	kmem_zone_free(sat_token_zone, token);
}

static void
sat_free_token_chain(sat_token_t token_chain)
{
	sat_token_t token;

	while (token_chain != NULL) {
		token = token_chain;
		token_chain = token_chain->t_next;
		sat_free_token(token);
	}
}

static sat_token_t
sat_alloc_token(int flags)
{
	sat_token_t token;

	token = kmem_zone_zalloc(sat_token_zone, KM_SLEEP);
	if (token == NULL)
		cmn_err(CE_PANIC, "Null token!");

	token->t_flags = flags | 0xca5e0000;

	return token;
}

static sat_token_t
sat_new_token(int tid)
{
	sat_token_t token;

	token = sat_alloc_token(TF_HEADER);

	token->t_len = sizeof(sat_token_header_t);
	token->t_header.sat_token_id = tid;
	token->t_header.sat_token_size = sizeof(sat_token_header_t);

	return token;
}

static void
sat_append_token(sat_token_t m, void *from, int size)
{
	sat_token_header_t *header = &m->t_header;

	for ( ; m->t_next; m = m->t_next)
		;

	if (size + m->t_len <= _TDS_) {
		bcopy(from, m->t_data + m->t_len, size);
		m->t_len += size;
		header->sat_token_size += size;
		return;
	}

	for ( ; size > 0; size -= _TDS_) {
		m->t_next = sat_alloc_token(TF_NO_HEADER);
		m = m->t_next;
		m->t_len = (size < _TDS_) ? size : _TDS_;
		bcopy(from, m->t_data, m->t_len);
		from = ((char *)from) + m->t_len;
		header->sat_token_size += size;
	}
}

static int
sat_length(sat_token_t token_chain)
{
	int total;

	for (total = 0; token_chain != NULL; token_chain = token_chain->t_next){
		if (token_chain->t_flags & TF_HEADER)
			total += token_chain->t_header.sat_token_size;
	}
	return total;
}

static sat_token_t
sat_copy_token_chain(sat_token_t old_chain)
{
	sat_token_t new_chain = NULL;
	sat_token_t new_token;

	for (; old_chain != NULL; old_chain = old_chain->t_next) {

		if (new_chain == NULL) {
			new_chain = sat_alloc_token(TF_NO_HEADER);
			new_token = new_chain;
		}
		else {
			new_token->t_next = sat_alloc_token(TF_NO_HEADER);
			new_token = new_token->t_next;
		}
		bcopy((caddr_t)old_chain, (caddr_t)new_token,
		    sizeof(struct sat_token));
		new_token->t_next = NULL;
	}
	return new_chain;
}

static void
sat_copy_token_data(sat_token_t token, char *dest)
{
	int copysize = token->t_header.sat_token_size - _THS_;
	int len = token->t_len - _THS_;

	bcopy((caddr_t)token->t_data + _THS_, (caddr_t)dest, len);

	for (copysize -= len; copysize > 0; copysize -= len) {
		if ((token = token->t_next) == NULL)
			break;
		if (token->t_flags & TF_HEADER)
			break;
		dest += len;
		len = token->t_len;
		bcopy((caddr_t)token->t_data, (caddr_t)dest, len);
	}
}

#ifdef UNUSED_IN_IRIX65

static sat_token_t
sat_new_errno_token(uint8_t error_number)
{
	sat_token_t token;

	token = sat_new_token(SAT_ERRNO_TOKEN);
	sat_append_token(token, &error_number, sizeof(error_number));

	return token;
}

static sat_token_t
sat_new_cap_value_token(cap_value_t cap)
{
	sat_token_t token;

	token = sat_new_token(SAT_CAP_VALUE_TOKEN);
	sat_append_token(token, &cap, sizeof(cap));

	return token;
}

static sat_token_t
sat_new_titled_text_token(char *title, char *text)
{
	sat_token_t token;

	token = sat_new_token(SAT_TITLED_TEXT_TOKEN);
	sat_append_token(token, title, SAT_TITLE_SIZE);
	sat_append_token(token, text, strlen(text) + 1);
	return token;
}

static sat_token_t
sat_new_opaque_token(char *text, sat_token_size_t size)
{
	return sat_new_charp_token(SAT_OPAQUE_TOKEN, text, size);
}

#endif	/* UNUSED_IN_IRIX65 */

static sat_token_t
sat_new_pid_token(pid_t pid)
{
	sat_token_t token;

	token = sat_new_token(SAT_PID_TOKEN);
	sat_append_token(token, &pid, sizeof(pid));

	return token;
}

static sat_token_t
sat_new_parent_pid_token(pid_t ppid)
{
	sat_token_t token;

	token = sat_new_token(SAT_PARENT_PID_TOKEN);
	sat_append_token(token, &ppid, sizeof(ppid));

	return token;
}

static sat_token_t
sat_new_binary_token(int8_t bit)
{
	sat_token_t token;

	token = sat_new_token(SAT_BINARY_TOKEN);
	sat_append_token(token, &bit, sizeof(bit));

	return token;
}

static sat_token_t
sat_new_mode_token(mode_t mode)
{
	sat_token_t token;

	token = sat_new_token(SAT_MODE_TOKEN);
	sat_append_token(token, &mode, sizeof(mode));

	return token;
}

static sat_token_t
sat_new_cap_set_token(cap_t cap_set)
{
	sat_token_t token;

	token = sat_new_token(SAT_CAP_SET_TOKEN);
	sat_append_token(token, cap_set, sizeof(cap_set_t));

	return token;
}

static sat_token_t
sat_new_privilege_token(cap_value_t cap, uint8_t how)
{
	sat_token_t token;

	token = sat_new_token(SAT_PRIVILEGE_TOKEN);
	sat_append_token(token, &cap, sizeof(cap));
	sat_append_token(token, &how, sizeof(how));

	return token;
}

static sat_token_t
sat_new_acl_token(acl_t acl)
{
	sat_token_t token;

	token = sat_new_token(SAT_ACL_TOKEN);
	sat_append_token(token, acl, sizeof(struct acl));

	return token;
}

static sat_token_t
sat_new_mac_label_token(mac_label *mac)
{
	sat_token_t token;
	extern int mac_enabled;

	if (!mac_enabled)
		return NULL;

	token = sat_new_token(SAT_MAC_LABEL_TOKEN);
	sat_append_token(token, mac, _MAC_SIZE(mac));

	return token;
}

static sat_token_t
sat_new_socket_token(sat_socket_id_t socket)
{
	sat_token_t token;

	token = sat_new_token(SAT_SOCKET_TOKEN);
	sat_append_token(token, &socket, sizeof(socket));

	return token;
}

static sat_token_t
sat_new_ifname_token(sat_ifname_t ifname)
{
	sat_token_t token;

	token = sat_new_token(SAT_IFNAME_TOKEN);
	sat_append_token(token, ifname, SAT_IFNAME_SIZE);

	return token;
}

static sat_token_t
sat_new_descriptor_list_token(sat_descriptor_t *fds, int count)
{
	sat_token_t token;
	int size = count * sizeof(sat_descriptor_t);

	token = sat_new_token(SAT_DESCRIPTOR_LIST_TOKEN);
	sat_append_token(token, fds, size);

	return token;
}

static sat_token_t
sat_new_sysarg_list_token(sat_sysarg_t *args, int count)
{
	sat_token_t token;
	int size = count * sizeof(sat_sysarg_t);

	token = sat_new_token(SAT_SYSARG_LIST_TOKEN);
	sat_append_token(token, args, size);

	return token;
}

static sat_token_t
sat_new_uid_list_token(uid_t *uidp, int count)
{
	sat_token_t token;
	int size = count * sizeof(uid_t);

	token = sat_new_token(SAT_UID_LIST_TOKEN);
	sat_append_token(token, uidp, size);

	return token;
}

static sat_token_t
sat_new_gid_list_token(gid_t *gidp, int count)
{
	sat_token_t token;
	int size = count * sizeof(gid_t);

	token = sat_new_token(SAT_GID_LIST_TOKEN);
	sat_append_token(token, gidp, size);

	return token;
}


static sat_token_t
sat_new_file_token(ino64_t inode, dev_t device)
{
	sat_token_t token;

	token = sat_new_token(SAT_FILE_TOKEN);
	sat_append_token(token, &inode, sizeof(ino64_t));
	sat_append_token(token, &device, sizeof(dev_t));

	return token;
}

static sat_token_t
sat_new_ugid_token(uid_t uid, gid_t gid)
{
	sat_token_t token;

	token = sat_new_token(SAT_UGID_TOKEN);
	sat_append_token(token, &uid, sizeof(uid_t));
	sat_append_token(token, &gid, sizeof(gid_t));

	return token;
}

static sat_token_t
sat_new_satid_token(uid_t satid)
{
	sat_token_t token;

	token = sat_new_token(SAT_SATID_TOKEN);
	sat_append_token(token, &satid, sizeof(satid));

	return token;
}

static sat_token_t
sat_new_syscall_token(uint8_t major, uint16_t minor)
{
	sat_token_t token;

	token = sat_new_token(SAT_SYSCALL_TOKEN);
	sat_append_token(token, &major, sizeof(major));
	sat_append_token(token, &minor, sizeof(minor));

	return token;
}


static sat_token_t
sat_new_time_token(time_t clock, uint8_t ticks)
{
	sat_token_t token;

	token = sat_new_token(SAT_TIME_TOKEN);
	sat_append_token(token, &clock, sizeof(clock));
	sat_append_token(token, &ticks, sizeof(ticks));

	return token;
}

static sat_token_t
sat_new_protocol_token(uint16_t domain, uint16_t protocol)
{
	sat_token_t token;

	token = sat_new_token(SAT_PROTOCOL_TOKEN);
	sat_append_token(token, &domain, sizeof(domain));
	sat_append_token(token, &protocol, sizeof(protocol));

	return token;
}

static sat_token_t
sat_new_ifreq_token(struct ifreq *ifreq)
{
	sat_token_t token;

	token = sat_new_token(SAT_IFREQ_TOKEN);
	sat_append_token(token, ifreq, sizeof(struct ifreq));

	return token;
}


static sat_token_t
sat_new_record_header_token(
	int32_t magic,
	sat_token_size_t size,
	int8_t rectype,
	int8_t outcome,
	int8_t sequence,
	int8_t errno)
{
	sat_token_t token;

	token = sat_new_token(SAT_RECORD_HEADER_TOKEN);
	sat_append_token(token, &magic, sizeof(magic));
	sat_append_token(token, &size, sizeof(size));
	sat_append_token(token, &rectype, sizeof(rectype));
	sat_append_token(token, &outcome, sizeof(outcome));
	sat_append_token(token, &sequence, sizeof(sequence));
	sat_append_token(token, &errno, sizeof(errno));

	return token;
}

static sat_token_t
sat_new_device_token(dev_t device)
{
	sat_token_t token;

	token = sat_new_token(SAT_DEVICE_TOKEN);
	sat_append_token(token, &device, sizeof(dev_t));

	return token;
}

static sat_token_t
sat_new_uint32_token(sat_token_id_t tid, uint32_t data)
{
	sat_token_t token;

	token = sat_new_token(tid);
	sat_append_token(token, &data, sizeof(data));

	return token;
}

static sat_token_t
sat_new_hostid_token(uint32_t hid)
{
	return sat_new_uint32_token(SAT_HOSTID_TOKEN, hid);
}

static sat_token_t
sat_new_signal_token(uint32_t sig)
{
	return sat_new_uint32_token(SAT_SIGNAL_TOKEN, sig);
}

static sat_token_t
sat_new_status_token(uint32_t status)
{
	return sat_new_uint32_token(SAT_STATUS_TOKEN, status);
}

static sat_token_t
sat_new_port_token(int32_t port)
{
	return sat_new_uint32_token(SAT_PORT_TOKEN, port);
}

static sat_token_t
sat_new_svipc_id_token(int32_t svipc_id)
{
	return sat_new_uint32_token(SAT_SVIPC_ID_TOKEN, svipc_id);
}

static sat_token_t
sat_new_svipc_key_token(int32_t svipc_key)
{
	return sat_new_uint32_token(SAT_SVIPC_KEY_TOKEN, svipc_key);
}

static sat_token_t
sat_new_openmode_token(uint32_t mode)
{
	return sat_new_uint32_token(SAT_OPENMODE_TOKEN, mode);
}

static sat_token_t
sat_new_charp_token(sat_token_id_t tid, char *cp, sat_token_size_t size)
{
	sat_token_t token;

	token = sat_new_token(tid);
	sat_append_token(token, cp, size);
	return token;
}

static sat_token_t
sat_new_pathname_token(char *text, sat_token_size_t size)
{
	return sat_new_charp_token(SAT_PATHNAME_TOKEN, text, size);
}

static sat_token_t
sat_new_lookup_token(char *text, sat_token_size_t size)
{
	return sat_new_charp_token(SAT_LOOKUP_TOKEN, text, size);
}

static sat_token_t
sat_new_ip_header_token(char *header, int count)
{
	return sat_new_charp_token(SAT_IP_HEADER_TOKEN, header, count);
}

static sat_token_t
sat_new_sockadder_token(char *sockadder, int count)
{
	return sat_new_charp_token(SAT_SOCKADDER_TOKEN, sockadder, count);
}

static sat_token_t
sat_new_cwd_token(char *text)
{
	return sat_new_charp_token(SAT_CWD_TOKEN, text, strlen(text) + 1);
}

static sat_token_t
sat_new_root_token(char *text)
{
	return sat_new_charp_token(SAT_ROOT_TOKEN, text, strlen(text) + 1);
}

static sat_token_t
sat_new_text_token(char *text)
{
	return sat_new_charp_token(SAT_TEXT_TOKEN, text, strlen(text) + 1);
}

static sat_token_t
sat_new_command_token(char *text)
{
	return sat_new_charp_token(SAT_COMMAND_TOKEN, text, strlen(text) + 1);
}

static sat_token_t
sat_new_acct_timers_token(struct acct_timers *timers)
{
	return sat_new_charp_token(SAT_ACCT_TIMERS_TOKEN, (char *)timers,
	    sizeof(struct acct_timers));
}

static sat_token_t
sat_new_acct_counts_token(struct acct_counts *counts)
{
	return sat_new_charp_token(SAT_ACCT_COUNTS_TOKEN, (char *)counts,
	    sizeof(struct acct_counts));
}

static sat_token_t
sat_new_acct_proc_token(struct sat_proc_acct *pap)
{
	return sat_new_charp_token(SAT_ACCT_PROC_TOKEN, (char *)pap,
	    sizeof(struct sat_proc_acct));
}

static sat_token_t
sat_new_acct_session_token(struct sat_session_acct *sp)
{
	return sat_new_charp_token(SAT_ACCT_SESSION_TOKEN, (char *)sp,
	    sizeof(struct sat_session_acct));
}

static sat_token_t
sat_new_acct_spi_token(char *sp)
{
	return sat_new_charp_token(SAT_ACCT_SPI_TOKEN, sp,
	    sizeof(struct acct_spi));
}

static sat_token_t
sat_new_acct_spi2_token(sat_token_size_t s, char *sp)
{
	sat_token_t token;

	token = sat_new_token(SAT_ACCT_SPI2_TOKEN);
	sat_append_token(token, &s, sizeof(s));
	sat_append_token(token, sp, s);
	return token;
}

/*
 * Simply print a message to the effect that SAT is enabled.
 */
void
sat_confignote(void)
{
        cmn_err(CE_CONT, "Security Audit Trail Enabled.\n");
}

/*
 * Initialize the Security Audit Trail subsystem.  Note: called
 * after the kernel heap is initialized -- see master.d/kernel.
 */
void
sat_init( void )
{
	/* Let the world know sat is installed */
	sat_enabled = 1;

	spinlock_init(&sat_qptrlock, "satqplk");
	sv_init(&sat_qlock, SV_DEFAULT, "satqlock");

	/* Initialize the zone for the sat_proc structures */
	sat_proc_zone = kmem_zone_init(sizeof(sat_proc_t), "Sat proc");
	sat_token_zone = kmem_zone_init(sizeof(struct sat_token), "Sat token");

	SAT_ZERO(&audit_state);
}

void
sat_init_syscall(void)
{
	if (curuthread == NULL)
		return;

	curuthread->ut_sat_proc->sat_suflag = 0;
}

/*
 * Append the second token chain onto the first.
 */
static void
sat_chain_token(sat_token_t m, sat_token_t t)
{
	if (t == NULL)
		return;

	if (m == NULL) {
		sat_free_token_chain(t);
		return;
	}

	for (; m->t_next; m = m->t_next)
		;

	m->t_next = t;
}

/*
 * Check if the event is interesting. Mark the uthread interest flag
 * if it's to be ignored. Throw away any information already gathered.
 */
static int
sat_check_interest(int type, uthread_t *ut)
{
	sat_proc_t *sp;
	sat_event_mask *emp = &audit_state;

	if (ut && ut->ut_satmask)
		emp = ut->ut_satmask;
	/*
	 * If the audit daemon isn't running we're not interested.
	 * Return if the event is interesting
	 */
	if (satd_uthread != NULL && SAT_ISSET(type, emp))
		return SAT_TRUE;
	/*
	 * If there's no uthread there's nothing to clean up.
	 */
	if (ut == NULL || ut->ut_sat_proc == NULL)
		return SAT_FALSE;
	/*
	 * Event is not interesting. Clean up.
	 */
	sp = ut->ut_sat_proc;
	sp->sat_suflag |= SAT_IGNORE;

	sat_free_token_chain(sp->sat_pn);
	sp->sat_pn = NULL;

	sat_free_token_chain(sp->sat_tokens);
	sp->sat_tokens = NULL;

	return SAT_FALSE;
}

static u_short sequence[SAT_NTYPES];
/*
 * Allocate a record header token. If there's sufficient context,
 * add in the information about the current uthread.
 */
sat_token_t
sat_alloc(int type, uthread_t *ut)
{
	sat_token_t record;
	timespec_t tv;
	cred_t *cr;
	pid_t pid;

	/*
	 * If the event's not interesting we're done.
	 */
	if (sat_check_interest(type, ut) == SAT_FALSE)
		return NULL;
	/*
	 * Start the list of tokens we know the values for.
	 * Time is universal, so it's a good place to start.
	 */
	nanotime(&tv);
	record = sat_new_time_token(tv.tv_sec, tv.tv_nsec * 100 / NSEC_PER_SEC);
	/*
	 * Gather that context information which is available.
	 */
	if (ut) {
		struct sat_proc *sp;
		proc_t *pp;

		cr = ut->ut_cred;
		pp = UT_TO_PROC(ut);
		pid = pp->p_pid;
		sp = ut->ut_sat_proc;
		ASSERT(sp);
		/*
		 * Save information that we're going to need
		 * to put together a header token.
		 */
		sp->sat_sequence = sequence[type]++;
		sp->sat_event = type;
		/*
		 * Add the system call numbers
		 */
		sat_chain_token(record,
		    sat_new_syscall_token(ut->ut_syscallno, sp->sat_subsysnum));
		/*
		 * Add the satid
		 */
		sat_chain_token(record, sat_new_satid_token(sp->sat_uid));
		/*
		 * process name
		 */
		sat_chain_token(record, sat_new_command_token(sp->sat_comm));
		/*
		 * Current working directory and current root directory
		 */
		if (sp->sat_cwd)
			sat_chain_token(record,
			    sat_copy_token_chain(sp->sat_cwd));
		if (sp->sat_root)
			sat_chain_token(record,
			    sat_copy_token_chain(sp->sat_root));
		/*
		 * XXX	fix for cells
		 * XXX:casey - I found the above comment in
		 * the previous version of the code.
		 */
		sat_chain_token(record, sat_new_device_token(cttydev(pp)));
		sat_chain_token(record, sat_new_parent_pid_token(pp->p_ppid));
	}
	else {
		cr = get_current_cred();
		pid = current_pid();
	}

	ASSERT(cr);
	/*
	 * Add the process id
	 */
	sat_chain_token(record, sat_new_pid_token(pid));
	/*
	 * We've gotten the cred and can add information therefrom.
	 *	effective uid and gid
	 *	real uid and gid
	 */
	sat_chain_token(record, sat_new_ugid_token(cr->cr_uid, cr->cr_gid));
	sat_chain_token(record, sat_new_ugid_token(cr->cr_ruid, cr->cr_rgid));

	/*
	 * Include the group list if there is one.
	 */
	if (cr->cr_ngroups)
		sat_chain_token(record,
		    sat_new_gid_list_token(cr->cr_groups, cr->cr_ngroups));
	/*
	 * capability set
	 */
	sat_chain_token(record, sat_new_cap_set_token(&cr->cr_cap));

	/*
	 * MAC process label
	 */
	sat_chain_token(record, sat_new_mac_label_token(cr->cr_mac));

	return record;
}

/*
 * "book" a lookup token.
 * Determine if pathname lookup should be recorded in the audit trail.
 * Set the SAT_PATHLESS flag for the uthread iff none of the type specified,
 * the SAT_ACCESS_FAILED and the SAT_ACCESS_DENIED are enabled.
 */
void
sat_pn_book(int rectype, uthread_t *ut)
{
	sat_event_mask *maskp;
	sat_proc_t *sp;

	ASSERT(ut);

	sp = ut->ut_sat_proc;
	ASSERT(sp);
	ASSERT(sp->sat_pn == NULL);

	/*
	 * Clear the SAT_PATHLESS flag. It'll get set if we want it.
	 */
	sp->sat_suflag &= ~SAT_PATHLESS;

	if (rectype == -1)
		return;

	maskp = (ut->ut_satmask) ? ut->ut_satmask : &audit_state;

	if (SAT_ISSET(SAT_ACCESS_FAILED, maskp))
		return;
	if (SAT_ISSET(SAT_ACCESS_DENIED, maskp))
		return;
	if (SAT_ISSET(rectype, maskp))
		return;
	/*
	 * Under no circumstances will we be interested in any
	 * pathnames looked up in the processing of this record.
	 *
	 * Set the SAT_PATHLESS flag.
	 */
	sp->sat_suflag |= SAT_PATHLESS;
}

#define HEADERSIZE \
	(sizeof(sat_token_header_t) + sizeof(int32_t) + \
	 sizeof(sat_token_size_t) + sizeof(int8_t) + \
	 sizeof(int8_t) + sizeof(int8_t) + sizeof(int8_t))

/*
 * Add the given record to the end of the record queue
 */
void
sat_enqueue(sat_token_t record, int error, uthread_t *ut)
{
	sat_token_t head;
	sat_token_size_t total_size = HEADERSIZE;
	int8_t rectype = SAT_SYSCALL_KERNEL;
	int8_t sequence = 0;
	int8_t outcome = error ? SAT_FAILURE : SAT_SUCCESS;
	int spin;

	/*
	 * Pull information out of the uthread.
	 * Combine in any tokens already known and state variables.
	 */
	if (ut) {
		sat_proc_t *sp = ut->ut_sat_proc;

		ASSERT(sp);

		sat_chain_token(record, sp->sat_tokens);
		sp->sat_tokens = NULL;

		sequence = sp->sat_sequence;
		rectype = sp->sat_event;

		sat_free_token_chain(sp->sat_pn);
		sp->sat_pn = NULL;

		if (sp->sat_suflag & SAT_SUSERCHK) {
			sat_chain_token(record,
			    sat_new_privilege_token(sp->sat_cap,
			    sp->sat_suflag & (SAT_SUSERPOSS | SAT_CAPPOSS)));
			sp->sat_suflag = 0;
		}
	}
	total_size += sat_length(record);
	/*
	 * Allocate the record header token.
	 */
	head = sat_new_record_header_token(SAT_RECORD_MAGIC, total_size,
	    rectype, outcome, sequence, error);
	sat_chain_token(head, record);

	spin = mutex_spinlock(&sat_qptrlock);

	if (sat_queue_head) {
		/*
		 * queue is not empty
		 */
		sat_queue_tail->t_act = head;
		sat_queue_tail = head;
	} else {
		/*
		 * if the queue was previously empty, unlock the
		 * queue, unblocking any waiting (consumer) process.
		 */
		sat_queue_head = head;
		sat_queue_tail = head;
		sv_signal(&sat_qlock);
	}
	mutex_spinunlock(&sat_qptrlock, spin);
}

/*
 * Remove a sat record (if available) from the queue, copy it into
 * user memory, and free it.  Copy as many bytes as will fit in the
 * buffer (i.e. make no attempt to transfer intergral records).
 *
 * Return the number of bytes copied into the buffer.
 *
 * sat_read( buffer, sizeof buffer );
 */
int
sat_read(char *ubuf, unsigned size_ubuf, rval_t *rvp)
{
	sat_token_t record;
	int reclen, len, ubuflen=size_ubuf;
	int spin;

	/*
	 * If not super user, return EPERM.
	 */
	if (!cap_able(CAP_AUDIT_CONTROL))
		return EPERM;

	/* If we don't dominate dbadmin, return EACCES */

	if (! _MAC_DOM(get_current_cred()->cr_mac, mac_admin_high_lp))
		return EACCES;

	/*
	 * If there's a "registered" sat daemon, and we're not it,
	 * return EACCES.
	 */
	if (satd_uthread != NULL && satd_uthread != curuthread)
		return EACCES;

	/*
	 * Lock the queue.  The queue semaphore is managed so that
	 * this will block until the queue is not empty.
	 */
	/*
	 * Additional note: this code used to just do a psema at PZERO,
	 * but we got bitten by the following ASSERTION for some reason.
	 * The following code uses a higher sleep priority (so kills
	 * will work, etc.) which means reworking the logic anyway.
	 */
	spin = mutex_spinlock(&sat_qptrlock);
	while (sat_queue_head == NULL) {
		/* exchange our spinlock for a sleeplock */
		if (sv_wait_sig(&sat_qlock, PSLEP, &sat_qptrlock, spin))
			return EINTR;
		spin = mutex_spinlock(&sat_qptrlock);
	}

	ASSERT(sat_queue_head != NULL);	/* better be a record here.... */
	mutex_spinunlock(&sat_qptrlock, spin);

	/* while there is space in ubuf and ... */

	while ( ubuflen > 0 ) {

		spin = mutex_spinlock(&sat_qptrlock); /* --- LOCKED */

		if (sat_queue_head == NULL) {	/* ... and while there are  */
			mutex_spinunlock(&sat_qptrlock, spin);/* records, */
			break;
		}

		record = sat_queue_head;
		reclen = sat_length(record);

		/* don't break up records that would otherwise fit */
		if (reclen > ubuflen && reclen < size_ubuf) {
			mutex_spinunlock(&sat_qptrlock, spin);
			break;
		}

		/* else pull the next record off the queue */
		sat_queue_head = record->t_act;
		if (sat_queue_head == NULL)
			sat_queue_tail = NULL;

		mutex_spinunlock(&sat_qptrlock, spin); /* --- UNLOCKED */

		while (record != NULL && reclen > 0  &&  ubuflen > 0) {

			/* use minimum length */
			len = min(record->t_len, ubuflen);

			if (record->t_copyfrom == NULL)
				record->t_copyfrom = record->t_data;

			if ( copyout(record->t_copyfrom, ubuf, len) ) {
				sat_free_token_chain( record );
				return EFAULT;
			}
			record->t_copyfrom += len;
			ubuf += len;
			ubuflen -= len;
			reclen -= len;

			/*
			 * If we transferred all of this token, free the
			 * token and get the next token in the chain.
			 */
			if ( len == record->t_len ) {
				sat_token_t tp = record;
				record = record->t_next;
				sat_free_token( tp );
				len = 0;
			}
		}

		if ( ubuflen == 0  &&  reclen > 0 ) {
			/*
			 * The space in ubuf was used up before all token in
			 * the current chain could be transferred.  Adjust
			 * the token, and put it first on the queue.
			 */
			record->t_copyfrom += len;
			record->t_len -= len;
			spin = mutex_spinlock(&sat_qptrlock);
			if (sat_queue_head == NULL)
				sat_queue_tail = record;
			record->t_act = sat_queue_head;
			sat_queue_head = record;
			mutex_spinunlock(&sat_qptrlock, spin);
		}
	}

	/* If more records are available, unlock the queue */

	spin = mutex_spinlock(&sat_qptrlock);
	if (sat_queue_head != NULL)
		sv_signal(&sat_qlock);
	mutex_spinunlock(&sat_qptrlock, spin);

	rvp->r_val1 = size_ubuf - ubuflen;
	return 0;
}


/*
 * Copy the user sat record into kernel memory and add it to the queue.
 *
 * sat_write( event type, outcome, buffer, sizeof buffer );
 */
int
sat_write(int type, int outcome, char * ubuf, unsigned size_ubuf)
{
	char *buffer;
	sat_token_t m;	/* head of token chain		*/
	int error;	/* error return			*/

	/*
	 * If insufficiently privileged, return EPERM.
	 */
	if (!cap_able(CAP_AUDIT_WRITE))
		return EPERM;

	/* range check the user parameters */
	if (!ubuf || size_ubuf > SAT_MAX_USER_REC)
		return EINVAL;

	if ( type < 0 || type >= SAT_NTYPES || type < SAT_USER_RECORDS )
		return EDOM;

	/* obtain a header buffer */
	if ((m = sat_alloc(type, curuthread)) == NULL) {
		/* m == 0 means event not selected, so return with no error */
		return 0;
	}

	buffer = kern_malloc(size_ubuf + 1);
	buffer[size_ubuf] = '\0';

	if ((error = copyin(ubuf, buffer, size_ubuf)) == 0)
		sat_chain_token(m, sat_new_text_token(buffer));

	kern_free(buffer);

	if (error)
		return EFAULT;
	/*
	 * The second parameter to sat_enqueue() is an error code.  In
	 * sat_enqueue(), the error code is mapped into an event outcome.
	 * An error code value of zero is mapped into a successful event
	 * outcome, and a non-zero value is mapped into a failed event
	 * outcome.
	 *
	 * For kernel generated audit records, a zero error code means
	 * the outcome was successful, and a non-zero error code means the
	 * outcome was a failure.
	 *
	 * For records generated with the user level satvwrite function which
	 * uses the syssgi(SGI_SATWRITE,...) system call, the outcome of
	 * the audit event is passed as non-zero for success, and zero for
	 * failure.  This "outcome" parameter is passed to sat_enqueue() as
	 * the "error" parameter.
	 * 
	 * Because of this, for all audit records that are generated by
	 * syssgi(SGI_SATWRITE,...) the contents of the "outcome" parameter
	 * need to be reversed at this point so that when it is passed to
	 * sat_enqueue() as the "error" parameter, it gets mapped correctly
	 * when the audit record "outcome" field is created by sat_enqueue().
	 */

	sat_enqueue(m, outcome ? SAT_FAILURE : SAT_SUCCESS, curuthread);
	return 0;
}

struct sat_u_cmd {
	int	usat_cmd;
	int	usat_arg;
};

static int
sat_uthread_localaudit(uthread_t *ut, void *arg)
{
	struct sat_u_cmd *sc = (struct sat_u_cmd *)arg;

	if (ut->ut_satmask == NULL) {
		/* copy the global mask into the new local one */
		ut->ut_satmask = kern_malloc(sizeof(sat_event_mask));
		bcopy((caddr_t)&audit_state, (caddr_t)ut->ut_satmask,
		      sizeof(sat_event_mask));
	}
	if (sc->usat_cmd == SATCTL_LOCALAUDIT_ON)
		SAT_SET(sc->usat_arg, ut->ut_satmask);
	else
		SAT_CLR(sc->usat_arg, ut->ut_satmask);

	return 0;
}

/*
 * SAT control.
 */
int
sat_ctl( int cmd, int arg, pid_t pid, rval_t *rvp )
{
#ifdef USER_AUDIT
	uid_t sat_uid;
	uid_t sat_suid;
	mac_label *sat_plabel;
	int s;
#endif

	vproc_t *vpr = NULL;
	struct proc *p = NULL;

	/*
	 * If not super user, return EPERM.
	 */
	if (!cap_able(CAP_AUDIT_CONTROL)) {
		sat_control(cmd, arg, pid, EPERM);
		return EPERM;
	}

	/* range check the args */

	switch (cmd) {
	    case SATCTL_AUDIT_ON:
	    case SATCTL_AUDIT_OFF:
	    case SATCTL_AUDIT_QUERY:
	    case SATCTL_LOCALAUDIT_ON:
	    case SATCTL_LOCALAUDIT_OFF:
	    case SATCTL_LOCALAUDIT_QUERY:
		if ( arg < 0 || arg >= SAT_NTYPES ) {
			sat_control(cmd, arg, pid, EDOM);
			return EDOM;
		}
		break;
	    case SATCTL_SET_SAT_ID:
		if ( arg < 0 || arg > MAXUID ) {
			sat_control(cmd, arg, pid, EINVAL);
			return EINVAL;
		}
		break;
	    case SATCTL_GET_SAT_ID:
		break;
	    case SATCTL_REGISTER_SATD:
		break;
	    default:
		return EINVAL;
	}

	/* for per-process ("local") control, find the proc */

	if ( cmd == SATCTL_LOCALAUDIT_ON ||
	     cmd == SATCTL_LOCALAUDIT_OFF ||
	     cmd == SATCTL_LOCALAUDIT_QUERY ) {
		cred_t *cr;

		if (pid == 0)
			pid = current_pid();
		if ((vpr = VPROC_LOOKUP(pid)) == NULL) {
			sat_control(cmd, arg, pid, ESRCH);
			return ESRCH;
		}
		VPROC_GET_PROC(vpr, &p);
		cr = pcred_access(p);
#ifdef USER_AUDIT
		sat_uid = cr->cr_uid;
		sat_suid = cr->cr_suid;
		sat_plabel = cr->cr_mac;
#endif
		if (cmd == SATCTL_LOCALAUDIT_QUERY) {
			if (!_MAC_DOM(get_current_cred()->cr_mac, cr->cr_mac)) {
				sat_control(cmd, arg, pid, EACCES);
				sat_proc_access(SAT_PROC_ATTR_READ, pid,
						cr, EACCES, -1);
				VPROC_RELE(vpr);
				pcred_unaccess(p);
				return EACCES;
			}
		} else {
			if (!_MAC_EQU(get_current_cred()->cr_mac, cr->cr_mac)) {
				sat_control(cmd, arg, pid, EACCES);
				sat_proc_access(SAT_PROC_ATTR_WRITE, pid,
						cr, EACCES, -1);
				VPROC_RELE(vpr);
				pcred_unaccess(p);
				return EACCES;
			}
		}
		pcred_unaccess(p);
	}

	switch (cmd) {
	    uthread_t *ut;
	    struct sat_u_cmd suc;

	    case SATCTL_AUDIT_ON:
		if (miniroot)
			break;
		SAT_SET(arg, &audit_state);
		break;
	    case SATCTL_AUDIT_OFF:
		SAT_CLR(arg, &audit_state);
		break;
	    case SATCTL_AUDIT_QUERY:
		rvp->r_val1 = SAT_ISSET(arg, &audit_state);
		break;
	    case SATCTL_LOCALAUDIT_ON:
	    case SATCTL_LOCALAUDIT_OFF:
		if (!miniroot) {
			suc.usat_cmd = cmd;
			suc.usat_arg = arg;
			uthread_apply(&p->p_proxy, UT_ID_NULL,
				      sat_uthread_localaudit, &suc);
		}
		break;
	    case SATCTL_LOCALAUDIT_QUERY:
		uscan_access(&p->p_proxy);
		ut = prxy_to_thread(&p->p_proxy);	/* they're all the same */
		if (ut->ut_satmask == NULL) {
			VPROC_RELE(vpr);
			sat_control(cmd, arg, pid, ENODATA);
			uscan_unlock(&p->p_proxy);
			return ENODATA;
		}
		rvp->r_val1 = SAT_ISSET(arg, ut->ut_satmask);
		uscan_unlock(&p->p_proxy);
		break;
	    case SATCTL_SET_SAT_ID:
		/*
		 * It used to be illegal to reset the satid.
		 * It's allowed now because of bug 216646.
		 */
		curuthread->ut_sat_proc->sat_uid = arg;
		break;
	    case SATCTL_GET_SAT_ID:
		rvp->r_val1 = curuthread->ut_sat_proc->sat_uid;
		break;
	    case SATCTL_REGISTER_SATD:
		if (satd_uthread)
			return EBUSY;
		else
			satd_uthread = curuthread;
		break;
	}
	if (vpr) {
		sat_control(cmd, arg, pid, 0);
		sat_proc_access(SAT_PROC_ATTR_WRITE, pid, p->p_cred, 0, -1);
		VPROC_RELE(vpr);
	} else
		sat_control(cmd, arg, pid, 0);

	return 0;
}

/*
 *  These functions are used by "non-sat code" to
 *  set the values of items in the sat_proc area.
 */

void
sat_set_subsysnum (u_short subsysnum)
{
	struct sat_proc *si;

	if (curuthread == NULL) {
#ifdef	STHREAD_DEBUG
		cmn_err(CE_WARN, "sat_set_subsysnum: No thread\n");
#endif	/* STHREAD_DEBUG */
		return;
	}
	si = curuthread->ut_sat_proc;

	ASSERT(si != NULL);

	si->sat_subsysnum = subsysnum;
}

void
sat_set_suflag (u_short suflag)
{
	struct sat_proc *si;

	if (curuthread == NULL) {
#ifdef	STHREAD_DEBUG
		cmn_err(CE_WARN, "sat_set_suflag: No thread\n");
#endif	/* STHREAD_DEBUG */
		return;
	}
	si = curuthread->ut_sat_proc;

	ASSERT(si != NULL);

	si->sat_suflag |= suflag;
}

void
sat_set_uid (uid_t uid)
{
	struct sat_proc *si;

	if (curuthread == NULL) {
#ifdef	STHREAD_DEBUG
		cmn_err(CE_WARN, "sat_set_uid: No thread\n");
#endif	/* STHREAD_DEBUG */
		return;
	}
	si = curuthread->ut_sat_proc;

	ASSERT(si != NULL);

	si->sat_uid = uid;
}

void
sat_set_rec (sat_token_t  rec)
{
	struct sat_proc *si;

	if (curuthread == NULL) {
#ifdef	STHREAD_DEBUG
		cmn_err(CE_WARN, "sat_set_rec: No thread\n");
#endif	/* STHREAD_DEBUG */
		return;
	}
	si = curuthread->ut_sat_proc;

	ASSERT(si != NULL);

	si->sat_pn = rec;
}

void
sat_set_comm (char * comm)
{
	struct sat_proc *si;

	if (curuthread == NULL) {
#ifdef	STHREAD_DEBUG
		cmn_err(CE_WARN, "sat_set_comm: No thread\n");
#endif	/* STHREAD_DEBUG */
		return;
	}
	si = curuthread->ut_sat_proc;

	ASSERT(si != NULL);

	ASSERT(strlen(comm) <= PSCOMSIZ);
	strcpy(si->sat_comm,comm);
}

void
sat_set_openfd (int fd)
{
	struct sat_proc *si;

	if (curuthread == NULL) {
#ifdef	STHREAD_DEBUG
		cmn_err(CE_WARN, "sat_set_openfd: No thread\n");
#endif	/* STHREAD_DEBUG */
		return;
	}
	si = curuthread->ut_sat_proc;

	ASSERT(si != NULL);
	si->sat_openfd = fd;
}

void
sat_set_cap (cap_value_t cap)
{
	struct sat_proc *si;

	if (curuthread == NULL) {
#ifdef	STHREAD_DEBUG
		cmn_err(CE_WARN, "sat_set_cap: No thread\n");
#endif	/* STHREAD_DEBUG */
		return;
	}
	si = curuthread->ut_sat_proc;

	ASSERT(si != NULL);

	si->sat_cap = cap;
}

/*
 * Check satflags.
 * SAT_SUSERCHK is set whenever a routine checks for superuser
 * privilege (i.e., u_uid == 0).  If the check was successful,
 * SAT_SUSERPOSS is also set.
 *
 * When an audit record is generated, u_satflag is cleared, so if
 * we get here and it is still set, we need to audit the use of
 * privilege.
 */

void
sat_check_flags(int error)
{
	struct sat_proc *sp;

	if (curuthread == NULL) {
#ifdef	STHREAD_DEBUG
		cmn_err(CE_WARN, "sat_check_flags: No thread\n");
#endif	/* STHREAD_DEBUG */
		return;
	}
	sp = curuthread->ut_sat_proc;

	ASSERT(sp != NULL);

	if (sp->sat_suflag & SAT_SUSERCHK)
		sat_check_priv(sp->sat_suflag & SAT_SUSERPOSS, error);
}

/* 
 *  Setup sat_proc area, and copy information from the
 *  parents sat_proc area to the childs sat_proc area.
*/

void
sat_proc_init(struct uthread_s * ut, struct uthread_s * parent)
{
	struct sat_proc * si;

	ASSERT(ut != parent);
	ASSERT(ut->ut_sat_proc == NULL);

	si = kmem_zone_zalloc(sat_proc_zone, KM_SLEEP);

	if (parent != NULL) {
		struct sat_proc *src = parent->ut_sat_proc;

		ASSERT(src != NULL);

		/* Copy parents entries */
		si->sat_uid = src->sat_uid;
		si->sat_subsysnum = src->sat_subsysnum;
		si->sat_suflag = src->sat_suflag;
		bcopy((caddr_t)src->sat_comm, (caddr_t)si->sat_comm, PSCOMSIZ);

		/* Copy working directories */

		if (src->sat_cwd)
			si->sat_cwd = sat_copy_token_chain(src->sat_cwd);

		if (src->sat_root)
			si->sat_root = sat_copy_token_chain(src->sat_root);

		if (src->sat_abs) {
			si->sat_abs = kern_malloc(strlen(src->sat_abs) + 1);
			strcpy(si->sat_abs, src->sat_abs);
		}

		/* Sat event mask.  In proc entry */
		if (parent->ut_satmask) {
			ut->ut_satmask = kern_malloc(sizeof(sat_event_mask));
			bcopy((caddr_t)parent->ut_satmask,
			    (caddr_t)ut->ut_satmask, sizeof(sat_event_mask));
		}
	}

	/* Point to sat_proc area */
	ut->ut_sat_proc = si;
}

void
sat_proc_exit(uthread_t * ut)
{
	struct sat_proc * sp = ut->ut_sat_proc;

	ASSERT(sp != NULL);

	/* Check if satd itself is exiting */
        if (satd_uthread == ut)
                satd_uthread = NULL;

	/* Free sat event mask if needed */
        if (ut->ut_satmask) {
                kern_free(ut->ut_satmask);
		/* no need to null -- struct is being tossed */
	}

	/*
	 * Free any works in progress.
	 */
	sat_free_token_chain(sp->sat_cwd);
	sp->sat_cwd = NULL;

	sat_free_token_chain(sp->sat_root);
	sp->sat_root = NULL;

	sat_free_token_chain(sp->sat_pn);
	sp->sat_pn = NULL;

	sat_free_token_chain(sp->sat_tokens);
	sp->sat_tokens = NULL;

	if (sp->sat_abs) {
		kern_free(sp->sat_abs);
		sp->sat_abs = NULL;
	}

	kmem_zone_free(sat_proc_zone, sp);
}

/*
 * Save information for SystemV IPC.
 */
struct ipc_perm *
sat_svipc_save (struct ipc_perm *perm)
{
	struct ipc_perm *sisp = kern_malloc(sizeof(struct ipc_perm));

	*sisp = *perm;

	return sisp;
}

static void
back_to_slash(char *string)
{
	char *end = string + strlen(string);

	for (end--; end > string && *end != '/'; end--)
		;
	if (end == string)
		end++;
	*end = '\0';
}

static void
add_component(char *current, char *new)
{
	int current_length = strlen(current);

	if (current[current_length - 1] != '/')
		current[current_length++] = '/';

	strcpy(current + current_length, new);
}

/*
 * resolve path from lookup string with special
 * characters and place in result string.
 */
static void
path_resolve(char *lookup, char *result)
{
	char	*np;
	char	*slashp;
	for (np = lookup; *np == '/'; np++)
		;

	while (*np != '\0') {
		if (strncmp(np, "./", 2) == 0) {
			np += 3;
			continue;
		}
		if (strncmp(np, "../", 3) == 0) {
			np += 4;
			back_to_slash(result);
			continue;
		}
		/*
		 * Get the initial / of the component seperator,
		 * then skip over it to the type indicator.
		 */
		for (slashp = np; *slashp != '/' && *slashp != '\0'; slashp++)
			;

		if (*slashp == '\0')
			break;
		*slashp++ = '\0';

		switch (*slashp) {
		case '@':
			slashp++;
			if (*slashp == '/') {
				slashp++;
				result[1] = '\0';
			}
			np = slashp;
			break;
		case '!':
			*slashp++ = '\0';
			if (*slashp == '/') {
				slashp++;
				result[1] = '\0';
			}
			else
				add_component(result, np);
			np = slashp;
			break;
		case '>':
		case '/':
			*slashp++ = '\0';
			add_component(result, np);
			np = slashp;
			break;
		default:
			break;
		}
	}
}
/*
 * Update the working or root directory.
 */
void
sat_update_rwdir(int rectype, uthread_t *ut)
{
	sat_proc_t *sp;
	int new_length;
	char *new_string;
	char *result_string;
	sat_token_t new;
	sat_token_t old;

	sp = ut->ut_sat_proc;
	ASSERT(sp);
	/*
	 * No token exists if a user attempts to cd to a non-existant
	 * directory.  Call sat_new_lookup_token() to create the necessary
	 * token.
	 */
	if (sp->sat_tokens == NULL) {
		sp->sat_tokens = sat_new_lookup_token("/....//", 7);
	}

	for (new = sp->sat_tokens; new; new = new->t_next)
		if ((new->t_flags & TF_HEADER) &&
		    (new->t_header.sat_token_id == SAT_LOOKUP_TOKEN)) {
			break;
	}
	if (new == NULL)
		return;

	new_length = new->t_header.sat_token_size - _THS_;
	new_string = kern_malloc(new_length + 4);
	sat_copy_token_data(new, new_string);

	new_string[new_length] = '\0';
	new_string[new_length + 1] = '\0';
	new_string[new_length + 2] = '\0';
	new_string[new_length + 3] = '\0';

	old = (rectype == SAT_CHROOT) ? sp->sat_root : sp->sat_cwd;

	if (old == NULL || new_string[0] == '/') {
		/*
		 * If there is no path yet set, or if it begins with "/"
		 *   1.  Clear the absolute path if it exists
		 *   2.  Reset the CWD or ROOT to "/"
		 */
		if (sp->sat_abs) {
			kern_free(sp->sat_abs);
			sp->sat_abs = NULL;
		}
		result_string = kern_malloc(new_length + 2);
		result_string[0] = '/';
		result_string[1] = '\0';

	} else {

		if ((old->t_header.sat_token_size - _THS_) > MAXPATHLEN) {
			int tsize = strlen("/<truncated>/");
			/*
			 * when the path exceeds MAXPATHLEN in size, it is
			 * truncated, and the saved path begins with the
			 * character string "/<truncated>/".  The new element
			 * that is being added to the pathname is then appended
			 * to this string.
			 */

			result_string = kern_malloc(new_length + tsize + 1);
			strcpy(result_string,"/<truncated>/");
			result_string[tsize] = '\0';

		} else if (strncmp(new_string, "./", 2) == 0) {
			int abs_length;
	
			/*
			 * When the path starts with "./"
			 *   1. save the absolute path that has been created up to
			 *      this point if it has not already been saved.
			 *   2. reset the CWD or ROOT to the absolute path.
			 */
			if (sp->sat_abs == NULL) {
				abs_length = old->t_header.sat_token_size - _THS_;
				sp->sat_abs = kern_malloc(abs_length + 1);
				sat_copy_token_data(old, sp->sat_abs);
				sp->sat_abs[abs_length] = '\0';
			} else {
				abs_length = strlen(sp->sat_abs);
			}
			result_string = kern_malloc (abs_length + new_length +1);
			bcopy(sp->sat_abs, result_string, abs_length);
			result_string[abs_length] = '\0';
	
		} else {
			int old_length = old->t_header.sat_token_size - _THS_;
	
			result_string = kern_malloc(new_length + old_length + 1);
			sat_copy_token_data(old, result_string);
			result_string[old_length] = '\0';
		}
	}

	sat_free_token_chain(old);

	path_resolve(new_string, result_string);

	if (rectype == SAT_CHROOT)
		sp->sat_root = sat_new_root_token(result_string);
	else
		sp->sat_cwd = sat_new_cwd_token(result_string);

	kern_free(new_string);
	kern_free(result_string);
}

void
sat_pn_save(struct pathname *pn, uthread_t *ut)
{
	sat_token_t token;
/*
 *	If the pathname that goes into the audit record
 *	token exceeds MAXPATHLEN, then it is truncated.
 *	The characters "/<truncated>" are placed at the
 *	beginning of the pathname token, and the last
 *	element of the path is appended to this string.
 */
	if (pn->pn_pathlen > MAXPATHLEN) {
		char	*sat_pn = pn->pn_path + pn->pn_pathlen;
		char	*trunc_path;
		int	tsize = strlen("/<truncated>");
		int	len_last;

		for (sat_pn--; *sat_pn != '/' && sat_pn > pn->pn_path; sat_pn--)
		;
		len_last = strlen(sat_pn) + 1;
		trunc_path = kern_malloc(tsize + len_last);
		strcpy(trunc_path,"/<truncated>");
		strcpy(trunc_path + tsize, sat_pn);
		token = sat_new_pathname_token(trunc_path, tsize + len_last);	
		kern_free(trunc_path);
	} else {
		token = sat_new_pathname_token(pn->pn_path, pn->pn_pathlen);
	}

	if (ut->ut_sat_proc->sat_tokens)
		sat_chain_token(ut->ut_sat_proc->sat_tokens, token);
	else
		ut->ut_sat_proc->sat_tokens = token;
}
/*
 * Initialize u_satrec to the given sat_pathname token,
 * and fill in the "requested pathname" field.
 *
 * This call should be followed by a call to lookuppn, which
 * will add the "actual pathname" info, and call sat_pn_finalize
 * to finish up.
 */
void
sat_pn_start(uthread_t *ut)
{
	ASSERT(ut);

	if (ut->ut_sat_proc->sat_suflag & (SAT_PATHLESS | SAT_IGNORE))
		return;
	if (ut->ut_sat_proc->sat_pn != NULL) {
		cmn_err_tag(1770, CE_WARN, "sat_pn_start with existing sat_pn");
		return;
	}

	ut->ut_sat_proc->sat_pn = sat_new_lookup_token("", 0);
}

/*
 * Append some text to the end of the lookup token in progress.
 */
void
sat_pn_append(char *from, uthread_t *ut)
{
	int		path_len;
	char		*lookup_path;
	char		*result_path;
	sat_token_t	pn;

	ASSERT(ut);

	if (ut->ut_sat_proc->sat_suflag & (SAT_PATHLESS | SAT_IGNORE))
		return;

	if (ut->ut_sat_proc->sat_pn == NULL) {
		cmn_err_tag(1771, CE_WARN, "sat_pn_start without existing sat_pn");
		return;
	}

	/*
	 * The lookup pathname is the pathname that is generated in lookuppn()
	 *
	 * The resolved pathname is the actual pathname that is being referenced.
	 *
	 * The pathname that goes into the LOOKUP audit record token will be
	 * truncated once the resolved pathname exceeds MAXPATHLEN in size.
	 *
	 * Because it contains special characters for mount points,
	 * symbolic links, and path element seperators, the lookup pathname
	 * is always longer than the resolved pathname.
	 *
	 * Once the lookup path exceeds MAXPATHLEN, then the resolved path
	 * name is evaluated in order to determine if truncation is necessary.
	 *
	 * The truncated lookup pathname starts with the string, "//<truncated>//"
	 * The last pathname lookup element is then appended to this string.
	 */

	pn = ut->ut_sat_proc->sat_pn;
	path_len = pn->t_header.sat_token_size - _THS_;
	if (path_len > MAXPATHLEN) {
		/* 
		 * break token out into character string to work with it
		 */
		lookup_path = kern_malloc(path_len + strlen(from) + 1);
		result_path = kern_malloc(path_len + strlen(from) + 1);
		sat_copy_token_data(pn, lookup_path);
		path_resolve(lookup_path, result_path);

		if (strlen(result_path) >  MAXPATHLEN) {
			int tsize = strlen("//<truncated>//");
		/*
		 * release ut->ut_sat_proc->sat_pn and reallocate
		 * ut->sat_proc->sat_pn to size of truncated path length
		 * form new lookup token with truncated path
		 */
			sat_free_token_chain(pn);
			ut->ut_sat_proc->sat_pn = sat_new_lookup_token("//<truncated>//",tsize);
		}
		kern_free(lookup_path);
		kern_free(result_path);
	}

	sat_append_token(ut->ut_sat_proc->sat_pn, from, strlen(from));
}

static void
sat_acl_save( sat_token_t token, struct vnode *vp )
{
	struct acl access_acl;
	struct acl default_acl;

	if (!acl_enabled)
		return;

	access_acl.acl_cnt = ACL_NOT_PRESENT;
	default_acl.acl_cnt = ACL_NOT_PRESENT;
	(void)_ACL_VTOACL(vp, &access_acl, &default_acl);

	if (access_acl.acl_cnt != ACL_NOT_PRESENT) {
		sat_chain_token(token, sat_new_text_token("access"));
		sat_chain_token(token, sat_new_acl_token(&access_acl));
	}

	if (default_acl.acl_cnt != ACL_NOT_PRESENT) {
		sat_chain_token(token, sat_new_text_token("default"));
		sat_chain_token(token, sat_new_acl_token(&default_acl));
	}
}

static void
sat_cap_save( sat_token_t token, struct vnode *vp )
{
	cap_set_t capset;

	if (cap_vtocap(vp, &capset))
		return;

	sat_chain_token(token, sat_new_cap_set_token(&capset));
}

/*
 * Fill in the attribute portion of the audit record using the given vnode.
 * Also calculate the length of the pathname we've looked up.
 */
void
sat_pn_finalize( struct vnode *vp, uthread_t *ut)
{
	struct vattr vattr;
	int error;
	sat_proc_t *sp;

	/*
	 * XXX:casey - This assertion assumes that only user threads
	 * do path name resolutions.
	 */
	ASSERT(ut);

	sp = ut->ut_sat_proc;
	ASSERT(sp);

	/*
	 * If there's no path gathered there's nothing to do.
	 */
	if (sp->sat_pn == NULL)
		return;
	/*
	 * Move the token-in-progress to the completed tokens list.
	 */
	if (sp->sat_tokens)
		sat_chain_token(sp->sat_tokens, sp->sat_pn);
	else
		sp->sat_tokens = sp->sat_pn;

	sp->sat_pn = NULL;
	
	if (vp == NULL)
		return;
	/*
	 * Add any attribute tokens that are available.
	 */
	vattr.va_mask = AT_NODEID|AT_FSID|AT_UID|AT_GID|AT_TYPE|AT_MODE;
	VOP_GETATTR(vp, &vattr, 0, ut->ut_cred, error);
	if (!error) {
		sat_chain_token(sp->sat_tokens,
		    sat_new_file_token(vattr.va_nodeid, vattr.va_fsid));
		sat_chain_token(sp->sat_tokens,
		    sat_new_ugid_token(vattr.va_uid, vattr.va_gid));
		sat_chain_token(sp->sat_tokens,
		    sat_new_mode_token(VTTOIF(vattr.va_type) | vattr.va_mode));
	}
	sat_chain_token(sp->sat_tokens,
	    sat_new_mac_label_token(_MAC_VTOLP(vp)));
	sat_cap_save(sp->sat_tokens, vp);
	sat_acl_save(sp->sat_tokens, vp);
}

/*
 * Lookup and audit the given path name.  Called after a create, usually,
 * since the file that got created isn't there when the first lookup
 * happens.  Only called if there was no error (i.e. only if the creation
 * succeeded.
 */
int
sat_lookup(char *path, enum symfollow followlink, uthread_t *ut)
{
	vnode_t *vp;
	int error;
	sat_proc_t *sp;

	ASSERT(ut);

	sp = ut->ut_sat_proc;

	sat_free_token_chain(sp->sat_pn);
	sp->sat_pn = NULL;

	sat_pn_book(SAT_FILE_CRT_DEL, ut);

	if (error = lookupname(path,UIO_USERSPACE,followlink,NULLVPP,&vp,NULL))
		return error;

	VN_RELE(vp);
	return 0;
}


/*
 * Common declaration and quick-exit check macro.
 */
#define PROBEHEAD(x) \
	sat_token_t m = sat_alloc((x), curuthread); if (!m) return

void
sat_access_pn( int rectype, int error )
{
	sat_token_t m;

	/* always generate SAT_ACCESS_DENIED for EACCES */

	if (rectype == SAT_ACCESS_FAILED || error == EACCES) {
		ASSERT(error != 0);
		m = sat_alloc((error == EACCES) ?
		    SAT_ACCESS_DENIED : SAT_ACCESS_FAILED, curuthread);
	} else
		m = sat_alloc(rectype, curuthread);

	if (! m)
		return;

	sat_enqueue( m, error, curuthread );
}

void
sat_access2( int rectype, int error )
{
	sat_access_pn(rectype, error);
}

void
sat_access( int rectype, int error )
{
	sat_access_pn(rectype, error);
}


void
sat_chrwdir( int error )
{
	sat_token_t m;
	int rectype;

	if (curuthread == NULL)
		return;

	/* chroot? */
	rectype = (curuthread->ut_syscallno == SYS_chroot-SYSVoffset) ?
	    SAT_CHROOT : SAT_CHDIR;

	/*
	 * Must do this before the sat_alloc so that only the
	 * information about the new directory will have been gathered.
	 */
	sat_update_rwdir(rectype, curuthread);

	m = sat_alloc(rectype, curuthread);

	if (! m)
		return;

	sat_enqueue( m, error, curuthread );
}

void
sat_open( int filedes, int created, int flags, int error )
{
	sat_descriptor_t fd = filedes;
	PROBEHEAD(((flags & (FWRITE|FTRUNC)) == 0) ? SAT_OPEN_RO : SAT_OPEN);

	if (!error)
		sat_chain_token(m, sat_new_descriptor_list_token(&fd, 1));
	if (created)
		sat_chain_token(m, sat_new_text_token("created"));
	sat_chain_token(m, sat_new_openmode_token(flags));

	sat_enqueue( m, error, curuthread );
}

void
sat_chmod( int newmode, int error )
{
	PROBEHEAD(SAT_FILE_ATTR_WRITE);

	sat_chain_token(m, sat_new_mode_token(newmode));
	sat_enqueue( m, error, curuthread );
}


void
sat_chown( int newuid, int newgid, int error )
{
	PROBEHEAD(SAT_FILE_ATTR_WRITE);

	sat_chain_token(m, sat_new_ugid_token(newuid, newgid));
	sat_enqueue( m, error, curuthread );
}


void
sat_setlabel( struct mac_label *newlabel, int error )
{
	PROBEHEAD(SAT_FILE_ATTR_WRITE);

	sat_chain_token(m, sat_new_mac_label_token(newlabel));
	sat_enqueue( m, error, curuthread );
}


void
sat_setcap( cap_t newcap, int error )
{
	PROBEHEAD(SAT_FILE_ATTR_WRITE);

	sat_chain_token(m, sat_new_cap_set_token(newcap));
	sat_enqueue( m, error, curuthread );
}


void
sat_setacl( struct acl *acl, struct acl *dacl, int error )
{
	PROBEHEAD(SAT_FILE_ATTR_WRITE);

	if (acl != NULL) {
		sat_chain_token(m, sat_new_text_token("access"));
		sat_chain_token(m, sat_new_acl_token(acl));
	}

	if (dacl != NULL) {
		sat_chain_token(m, sat_new_text_token("default"));
		sat_chain_token(m, sat_new_acl_token(dacl));
	}

	sat_enqueue( m, error, curuthread );
}


void
sat_utime(time_t *tptr, time_t atime, time_t mtime, int error)
{
	PROBEHEAD(SAT_FILE_ATTR_WRITE);

	if (tptr) {
		sat_chain_token(m, sat_new_time_token(atime, 0));
		sat_chain_token(m, sat_new_time_token(mtime, 0));
	}

	sat_enqueue( m, error, curuthread );
}


void
sat_exec( int error )
{
	cred_t *cr;
	PROBEHEAD(SAT_EXEC);

	if (curuthread == NULL)
		cr = get_current_cred();
	else
		cr = curuthread->ut_cred;

	sat_chain_token(m, sat_new_ugid_token(cr->cr_uid, cr->cr_gid));
	sat_chain_token(m, sat_new_cap_set_token(&cr->cr_cap));
	sat_enqueue( m, error, curuthread );
}


void
sat_acct( char *acct, int error )
{
	PROBEHEAD(SAT_SYSACCT);

	sat_chain_token(m, sat_new_binary_token(acct ? SAT_TRUE : SAT_FALSE));
	sat_enqueue( m, error, curuthread );
}


void
sat_fchdir( int filedes, int error )
{
	sat_descriptor_t fd = filedes;
	PROBEHEAD(SAT_FCHDIR);

	sat_chain_token(m, sat_new_descriptor_list_token(&fd, 1));
	sat_enqueue( m, error, curuthread );
}


void
sat_fd_read( int filedes, int error )
{
	sat_descriptor_t fd = filedes;
	PROBEHEAD(SAT_FD_READ);

	sat_chain_token(m, sat_new_descriptor_list_token(&fd, 1));
	sat_enqueue( m, error, curuthread );
}

void
sat_pfd_read2( struct pollfd *pfd, int npfds, int error )
{
	sat_descriptor_t *fdlist;
	int i, nfds;
	sat_token_t m;

	/*
	 * Audit event only if npfds greater than zero.  Attempting to do
	 * so, otherwise causes a system panic when we allocate zero bytes
	 * below and then dereference the resulting NULL pointer ...  There
	 * is no need from a security standpoint to audit this event if
	 * there are no file descriptors.
	 */
	if (npfds == 0)
		return;

	m = sat_alloc(SAT_FD_READ2, curuthread);
	if (!m)
		return;

	fdlist = (sat_descriptor_t *)kern_malloc(npfds*sizeof(*fdlist));
	/*
	 * Copy in the FDs, but skip all the illegal ones
	 */
	for (i = 0, nfds = 0; i < npfds; i++, pfd++)
		if ((pfd->fd >= 0) && !(pfd->revents & POLLNVAL))
			fdlist[nfds++] = pfd->fd;
	sat_chain_token(m, sat_new_descriptor_list_token(fdlist, nfds));
	kern_free(fdlist);
	sat_enqueue( m, error, curuthread );
}

void
sat_fd_rdwr( int filedes, int mode, int error )
{
	sat_descriptor_t fd = filedes;
	PROBEHEAD((mode == FREAD) ? SAT_FD_READ : SAT_FD_WRITE);

	sat_chain_token(m, sat_new_descriptor_list_token(&fd, 1));
	sat_enqueue( m, error, curuthread );
}


void
sat_fchmod( int filedes, mode_t mode, int error )
{
	sat_descriptor_t fd = filedes;
	PROBEHEAD(SAT_FD_ATTR_WRITE);

	sat_chain_token(m, sat_new_descriptor_list_token(&fd, 1));
	sat_chain_token(m, sat_new_mode_token(mode));
	sat_enqueue( m, error, curuthread );
}


void
sat_fchown( int filedes, uid_t uid, gid_t gid, int error )
{
	sat_descriptor_t fd = filedes;
	PROBEHEAD(SAT_FD_ATTR_WRITE);

	sat_chain_token(m, sat_new_descriptor_list_token(&fd, 1));
	sat_chain_token(m, sat_new_ugid_token(uid, gid));
	sat_enqueue( m, error, curuthread );
}


void
sat_pipe( int read, int write, int error )
{
	sat_descriptor_t fd[2];
	PROBEHEAD(SAT_PIPE);

	fd[0] = read;
	fd[1] = write;

	sat_chain_token(m, sat_new_descriptor_list_token(fd, 2));
	sat_enqueue( m, error, curuthread );
}


void
sat_dup( int old, int new, int error )
{
	sat_descriptor_t fd[2];
	PROBEHEAD(SAT_DUP);

	fd[0] = old;
	fd[1] = new;

	sat_chain_token(m, sat_new_descriptor_list_token(fd, 2));
	sat_enqueue( m, error, curuthread );
}


void
sat_close( int filedes, int error )
{
	sat_descriptor_t fd = filedes;
	PROBEHEAD(SAT_CLOSE);

	sat_chain_token(m, sat_new_descriptor_list_token(&fd, 1));
	sat_enqueue( m, error, curuthread );
}


void
sat_kill( int signal, pid_t pid, uid_t uid, uid_t suid, mac_label *plabel,
	  int error )
{
	uid_t uids[2];
	PROBEHEAD(SAT_PROC_ATTR_WRITE);

	uids[0] = uid;
	uids[1] = suid;
	sat_chain_token(m, sat_new_pid_token(pid));
	sat_chain_token(m, sat_new_uid_list_token(uids, 2));
	sat_chain_token(m, sat_new_signal_token(signal));
	sat_chain_token(m, sat_new_mac_label_token(plabel));
	sat_enqueue( m, error, curuthread );
}


void
sat_ptrace( int req, pid_t pid, struct cred *cr, int error )
{
	sat_token_t m;
	uid_t uids[2];

        switch (req) {
            case PTRC_RD_I:
            case PTRC_RD_D:
            case PTRC_RD_REG:
            case PTRC_RD_GPRS:
            case PTRC_RD_FPRS:
		m = sat_alloc( SAT_PROC_READ, curuthread);
                break;
            case PTRC_WR_I:
            case PTRC_WR_D:
            case PTRC_WR_REG:
            case PTRC_WR_GPRS:
            case PTRC_WR_FPRS:
		m = sat_alloc( SAT_PROC_WRITE, curuthread);
                break;
            case PTRC_CONTINUE:
            case PTRC_TERMINATE:
            case PTRC_STEP:
		m = sat_alloc( SAT_PROC_ATTR_WRITE, curuthread);
                break;
            default:
                return;
        }

	if (! m)
		return;

	sat_chain_token(m, sat_new_pid_token(pid));

	if (cr != NULL) {
		uids[0] = cr->cr_ruid;
		uids[1] = cr->cr_suid;
		sat_chain_token(m, sat_new_uid_list_token(uids, 2));
		sat_chain_token(m, sat_new_mac_label_token(cr->cr_mac));
	}

	sat_enqueue( m, error, curuthread );
}

void
sat_proc_access( int rec_type, pid_t pid, struct cred *cr, int error, int sig)
{
	uid_t uids[2];
	PROBEHEAD(rec_type);

	sat_chain_token(m, sat_new_pid_token(pid));
	sat_chain_token(m, sat_new_signal_token(sig));

	if (cr != NULL) {
		uids[0] = cr->cr_ruid;
		uids[1] = cr->cr_suid;
		sat_chain_token(m, sat_new_uid_list_token(uids, 2));
		sat_chain_token(m, sat_new_mac_label_token(cr->cr_mac));
	}

	sat_enqueue( m, error, curuthread );
}


void
sat_fork( pid_t newpid, int error )
{
	PROBEHEAD(SAT_FORK);

	sat_chain_token(m, sat_new_pid_token(newpid));
	sat_enqueue( m, error, curuthread );
}


void
sat_exit( int status, int error )
{
	PROBEHEAD(SAT_EXIT);

	sat_chain_token(m, sat_new_status_token(status >> 8));
	sat_enqueue( m, error, curuthread );
}


void
sat_setreuid( uid_t real, uid_t effective, int error )
{
	uid_t uids[2];
	PROBEHEAD(SAT_PROC_OWN_ATTR_WRITE);

	uids[0] = effective;
	uids[1] = real;
	sat_chain_token(m, sat_new_uid_list_token(uids, 2));
	sat_enqueue( m, error, curuthread );
}


void
sat_setregid( gid_t real, gid_t effective, int error )
{
	gid_t gids[2];
	PROBEHEAD(SAT_PROC_OWN_ATTR_WRITE);

	gids[0] = effective;
	gids[1] = real;
	sat_chain_token(m, sat_new_gid_list_token(gids, 2));
	sat_enqueue( m, error, curuthread );
}


void
sat_setplabel( mac_label *plabel, int error )
{
	PROBEHEAD(SAT_PROC_OWN_ATTR_WRITE);

	sat_chain_token(m, sat_new_mac_label_token(plabel));
	sat_enqueue( m, error, curuthread );
}

void
sat_setpcap( cap_t cap, int error )
{
	PROBEHEAD(SAT_PROC_OWN_ATTR_WRITE);

	sat_chain_token(m, sat_new_cap_set_token(cap));
	sat_enqueue( m, error, curuthread );
}

void
sat_setgroups( int ngroups, gid_t *grouplist, int error )
{
	PROBEHEAD(SAT_PROC_OWN_ATTR_WRITE);

	sat_chain_token(m, sat_new_gid_list_token(grouplist, ngroups));
	sat_enqueue( m, error, curuthread );
}

void
sat_umask( mode_t mask, int error )
{
	PROBEHEAD(SAT_PROC_OWN_ATTR_WRITE);

	sat_chain_token(m, sat_new_mode_token(mask));
	sat_enqueue( m, error, curuthread );
}

void
sat_svipc_access( mac_label *label, int mode, int status, int error )
{
	struct a {
		int id;
	} *uap;
	PROBEHEAD(SAT_SVIPC_ACCESS);

	sat_chain_token(m, sat_new_status_token(status));

	if (curuthread && curuthread->ut_scallargs) {
		uap = (struct a *)curuthread->ut_scallargs;
		sat_chain_token(m, sat_new_svipc_id_token(uap->id));
	}

	sat_chain_token(m, sat_new_mode_token(mode));
	sat_chain_token(m, sat_new_mac_label_token(label));
	sat_enqueue( m, error, curuthread );
}

void
sat_svipc_create( key_t key, int ipc_id, int flags, int error )
{
	PROBEHEAD(SAT_SVIPC_CREATE);

	sat_chain_token(m, sat_new_svipc_id_token(ipc_id));
	sat_chain_token(m, sat_new_svipc_key_token(key));
	sat_chain_token(m, sat_new_openmode_token(flags));
	sat_enqueue( m, error, curuthread );
}

void
sat_svipc_remove( int ipc_id, int error )
{
	PROBEHEAD(SAT_SVIPC_REMOVE);

	sat_chain_token(m, sat_new_status_token(SAT_SUCCESS));
	sat_chain_token(m, sat_new_svipc_id_token(ipc_id));
	sat_enqueue( m, error, curuthread );
}

void
sat_svipc_change( int ipc_id, struct ipc_perm *old, struct ipc_perm *new,
    int error )
{
	PROBEHEAD(SAT_SVIPC_CHANGE);

	sat_chain_token(m, sat_new_status_token(SAT_SUCCESS));
	sat_chain_token(m, sat_new_svipc_id_token(ipc_id));
	sat_chain_token(m, sat_new_ugid_token(old->uid, old->gid));
	sat_chain_token(m, sat_new_mode_token(old->mode));
	sat_chain_token(m, sat_new_ugid_token(new->uid, new->gid));
	sat_chain_token(m, sat_new_mode_token(new->mode));
	sat_enqueue( m, error, curuthread );
}

void
sat_svipc_ctl( int cmd, int ipc_id, struct ipc_perm *old, struct ipc_perm *new,
    int error)
{
	switch (cmd) {
	case IPC_RMID:
		sat_svipc_remove( ipc_id, error );
		break;
	case IPC_SET:
		ASSERT(old);
		ASSERT(new);
		sat_svipc_change( ipc_id, old, new, error );
		break;
	default:
		break;
	}
	if (new)
		kern_free(new);
}

/*
 *  Code common to bsdipc
 *
 * Interface names always end in a digit.  For some reason, 
 * the name string (ifp->if_name) has the last digit removed.  
 * The value of that digit is stored in if_unit.  
 * We must add it back onto the string for the audit record.
 *
 * XXX if_unit may be > 9 and thus more than a single digit.
 */
static void
sat_bsdipc_copy_ifname( char * target, struct ifnet *ifp ) 
{
	int len;

	(void)strncpy( target, ifp->if_name, SATIFNAMSIZ );
	if ((len = strlen( target )) < SATIFNAMSIZ)
		sprintf(&target[len], "%d", (ifp->if_unit & 0xf));
}

void
sat_bsdipc_create( short dscr, struct socket * socket, short domn, short prot,
		   int error )
{
	sat_descriptor_t fd = dscr;
	PROBEHEAD(SAT_BSDIPC_CREATE);

	sat_chain_token(m, sat_new_socket_token((sat_socket_id_t)socket));
	sat_chain_token(m, sat_new_descriptor_list_token(&fd, 1));
	sat_chain_token(m, sat_new_protocol_token(domn, prot));
	sat_enqueue( m, error, curuthread );
}

void
sat_bsdipc_create_pair( 
	short	dscr, 
	struct socket *	socket, 
	short	domn, 
	short	prot, 
	short	secd,
	struct socket *	socket2,
	int	error)
{
	sat_descriptor_t fd[2];
	PROBEHEAD(SAT_BSDIPC_CREATE_PAIR);

	fd[0] = dscr;
	fd[1] = secd;
	sat_chain_token(m, sat_new_socket_token((sat_socket_id_t)socket));
	sat_chain_token(m, sat_new_socket_token((sat_socket_id_t)socket2));
	sat_chain_token(m, sat_new_protocol_token(domn, prot));
	sat_chain_token(m, sat_new_descriptor_list_token(fd, 2));
	sat_enqueue( m, error, curuthread );
}

void
sat_bsdipc_shutdown( short dscr, struct socket * socket, short how, int error )
{
	sat_descriptor_t fd = dscr;
	sat_sysarg_t hw = how;
	PROBEHEAD(SAT_BSDIPC_SHUTDOWN);

	sat_chain_token(m, sat_new_socket_token((sat_socket_id_t)socket));
	sat_chain_token(m, sat_new_descriptor_list_token(&fd, 1));
	sat_chain_token(m, sat_new_sysarg_list_token(&hw, 1));
	sat_enqueue( m, error, curuthread );
}

void
sat_bsdipc_addr(int dscr, struct socket *socket, struct mbuf *nam, int error)
{
	sat_descriptor_t fd = dscr;
	PROBEHEAD(SAT_BSDIPC_ADDRESS);

	sat_chain_token(m, sat_new_socket_token((sat_socket_id_t)socket));
	sat_chain_token(m, sat_new_descriptor_list_token(&fd, 1));

	if (nam != NULL) {
		sat_token_size_t i;
		sa_family_t family;
		char *bp;

		i = m_datacopy(nam, 0, sizeof(sa_family_t), (caddr_t)&family);
		i = (family == AF_UNIX) ?
		    sizeof(struct sockaddr_un) : sizeof(struct sockaddr);
		bp = kern_malloc(i);
		i = m_datacopy(nam, 0, i, bp);
		sat_chain_token(m, sat_new_sockadder_token(bp, i));
		kern_free(bp);
	}
	sat_enqueue( m, error, curuthread );
}

void 
sat_bsdipc_resvport(int dscr, struct socket * socket, int port, int error)
{
	sat_descriptor_t fd = dscr;
	PROBEHEAD(SAT_BSDIPC_RESVPORT);

	sat_chain_token(m, sat_new_socket_token((sat_socket_id_t)socket));
	sat_chain_token(m, sat_new_descriptor_list_token(&fd, 1));
	sat_chain_token(m, sat_new_port_token(port));
	sat_enqueue( m, error, curuthread );
}

void
sat_bsdipc_if_config(
	int dscr,
	struct socket *socket,
	int cmd,
	struct ifreq *ifr,
	int error)
{
	sat_descriptor_t fd = dscr;
	sat_sysarg_t arg = cmd;
	PROBEHEAD(SAT_BSDIPC_IF_CONFIG);

	sat_chain_token(m, sat_new_socket_token((sat_socket_id_t)socket));
	sat_chain_token(m, sat_new_descriptor_list_token(&fd, 1));
	sat_chain_token(m, sat_new_ifreq_token(ifr));
	sat_chain_token(m, sat_new_sysarg_list_token(&arg, 1));
	sat_enqueue( m, error, curuthread );
}

/*ARGSUSED*/
void
sat_bsdipc_missing( struct ifnet *ifp, struct ip * ip, int error ) 
{
	char ifname[SAT_IFNAME_SIZE];
	PROBEHEAD(SAT_BSDIPC_RX_MISSING);

	sat_chain_token(m, sat_new_status_token(SAT_FAILURE));
	sat_bsdipc_copy_ifname(ifname, ifp);
	sat_chain_token(m, sat_new_ifname_token(ifname));
	sat_enqueue( m, error, curuthread );
}

/*
 * for events:	SAT_BSDIPC_TX_RANGE, SAT_BSDIPC_TX_OK, SAT_BSDIPC_TX_TOOBIG
 *		SAT_BSDIPC_RX_RANGE, SAT_BSDIPC_RX_OK
 */
/*ARGSUSED*/
void
sat_bsdipc_range( 
	struct ifnet * ifp, 
	struct ip * ip, 
	uid_t duid,
	mac_label * dlbl, 
	int event,
	int error)
{
	char ifname[SAT_IFNAME_SIZE];
	PROBEHEAD(event);

	switch (event) {
	case SAT_BSDIPC_TX_TOOBIG:
	case SAT_BSDIPC_TX_RANGE:
	case SAT_BSDIPC_RX_RANGE:
		sat_chain_token(m, sat_new_status_token(SAT_FAILURE));
		break;
	case SAT_BSDIPC_RX_OK:
	case SAT_BSDIPC_TX_OK:
		break;

#ifdef DEBUG
	default:
		panic("sat_bsdipc_range: bad type (%d)", event);
#endif
	}

	sat_bsdipc_copy_ifname(ifname, ifp);
	sat_chain_token(m, sat_new_ifname_token(ifname));
	sat_chain_token(m,
	    sat_new_ip_header_token((char *)ip, sizeof(struct ip)));
	sat_chain_token(m, sat_new_mac_label_token(dlbl));
	sat_chain_token(m, sat_new_uid_list_token(&duid, 1));
	sat_enqueue( m, error, curuthread );
}

void
sat_clock( time_t newtime, int error )
{
	PROBEHEAD(SAT_CLOCK_SET);

	sat_chain_token(m, sat_new_time_token(newtime, 0));
	sat_enqueue( m, error, curuthread );
}

void
sat_hostname_set( char *new_hostname, int error )
{
	PROBEHEAD(SAT_HOSTNAME_SET);

	if (new_hostname)
		sat_chain_token(m, sat_new_text_token(new_hostname));
	sat_enqueue( m, error, curuthread );
}

void
sat_domainname_set( char *new_domainname, int error )
{
	PROBEHEAD(SAT_DOMAINNAME_SET);

	if (new_domainname)
		sat_chain_token(m, sat_new_text_token(new_domainname));
	sat_enqueue( m, error, curuthread );
}

void
sat_hostid_set( long newid, int error )
{
	PROBEHEAD(SAT_HOSTID_SET);

	sat_chain_token(m, sat_new_hostid_token(newid));
	sat_enqueue( m, error, curuthread );
}

void
sat_tty_setlabel( mac_label *label, int error )
{
	PROBEHEAD(SAT_TTY_SETLABEL);

	/*
	 * XXX:casey - not identifying the object here.
	 */
	sat_chain_token(m, sat_new_mac_label_token(label));
	sat_enqueue( m, error, curuthread );
}

void
sat_check_priv( int priv_state, int error )
{
	PROBEHEAD(SAT_CHECK_PRIV);

	sat_chain_token(m, sat_new_status_token(priv_state));
	sat_enqueue( m, error, curuthread );
}

void
sat_control( int command, int arg, int pid, int error )
{
	sat_sysarg_t argv[2];
	PROBEHEAD(SAT_CONTROL);

	argv[0] = command;
	argv[1] = arg;
	sat_chain_token(m, sat_new_sysarg_list_token(argv, 2));
	sat_chain_token(m, sat_new_pid_token(pid));
	sat_enqueue( m, error, curuthread );
}

/*
 *  SVR4 Networking Stuff.
 */

void
sat_svr4net_create( int dscr, void * socket, short domn, short prot,
		   int error )
{
	sat_descriptor_t fd = dscr;
	PROBEHEAD(SAT_SVR4NET_CREATE);

	sat_chain_token(m, sat_new_socket_token((sat_socket_id_t)socket));
	sat_chain_token(m, sat_new_protocol_token(domn, prot));
	sat_chain_token(m, sat_new_descriptor_list_token(&fd, 1));
	sat_enqueue( m, error, curuthread );
}

void
sat_svr4net_addr( int dscr, void * socket, struct mbuf *nam, int error )
{
	sat_descriptor_t fd = dscr;
	PROBEHEAD(SAT_SVR4NET_ADDRESS);

	sat_chain_token(m, sat_new_socket_token((sat_socket_id_t)socket));
	sat_chain_token(m, sat_new_descriptor_list_token(&fd, 1));

	if (nam != NULL) {
		sat_token_size_t i;
		sa_family_t family;
		char *bp;

		i = m_datacopy(nam, 0, sizeof(sa_family_t), (caddr_t)&family);
		i = (family == AF_UNIX) ?
		    sizeof(struct sockaddr_un) : sizeof(struct sockaddr);
		bp = kern_malloc(i);
		i = m_datacopy(nam, 0, i, bp);
		sat_chain_token(m, sat_new_sockadder_token(bp, i));
		kern_free(bp);
	}
	sat_enqueue( m, error, curuthread );
}

void
sat_svr4net_shutdown( int dscr, void * socket, short how, int error )
{
	sat_descriptor_t fd = dscr;
	sat_sysarg_t hw = how;
	PROBEHEAD(SAT_SVR4NET_SHUTDOWN);

	sat_chain_token(m, sat_new_socket_token((sat_socket_id_t)socket));
	sat_chain_token(m, sat_new_descriptor_list_token(&fd, 1));
	sat_chain_token(m, sat_new_sysarg_list_token(&hw, 1));
	sat_enqueue( m, error, curuthread );
}

/* Debug message that can be called from interrupt stack.
 */
/*ARGSUSED*/
void
sat_sys_note(char * msg, int error)
{ 
	sat_token_t m;

	m = sat_alloc(SAT_SYS_NOTE, NULL);
	if (! m)
		return;

	sat_chain_token(m, sat_new_text_token(msg));
	sat_enqueue( m, 0, curuthread );
}

void
sat_proc_acct( struct proc *pp, struct acct_timers *timers,
	       struct acct_counts *counts, int error )
{
	struct sat_proc_acct rec;
	PROBEHEAD(SAT_PROC_ACCT);

	rec.sat_version = PROC_ACCT_VERSION;
	rec.sat_flag = 0;
	if (pp->p_acflag & AFORK)
		rec.sat_flag |= SPASF_FORK;
	if (pp->p_acflag & ASU)
		rec.sat_flag |= SPASF_SU;
	if (pp->p_acflag & ACKPT)
		rec.sat_flag |= SPASF_CKPT;
	if (pp->p_shacct != NULL)
		rec.sat_flag |= SPASF_SECONDARY;
	rec.sat_nice  = pp->p_proxy.prxy_sched.prs_nice;
	rec.sat_sched = (IS_SPROC(&pp->p_proxy)) ? pp->p_shaddr->s_sched : 0;
	rec.sat_btime = pp->p_start;
	rec.sat_etime = lbolt - pp->p_ticks;

	/* Pull out session-oriented info if available from the array session */
	if (pp->p_vpagg) {
		arsess_t *arsess_info;
		/*
		 * arsess_t structure is very large.
		 * malloc it and return when done to prevent stack overflow.
		 */
		arsess_info = kern_malloc(sizeof(arsess_t));

		VPAG_EXTRACT_ARSESS_INFO(pp->p_vpagg, arsess_info);

		if (arsess_info->as_refcnt == 1)
			rec.sat_flag |= SPASF_SESSEND;
		rec.sat_prid = arsess_info->as_prid;
		rec.sat_ash  = arsess_info->as_handle;
		kern_free(arsess_info);
	} else {
		rec.sat_prid = PRID_NONE;
		rec.sat_ash  = ASH_NONE;
	}

	sat_chain_token(m, sat_new_acct_proc_token(&rec));
	sat_chain_token(m, sat_new_acct_timers_token(timers));
	sat_chain_token(m, sat_new_acct_counts_token(counts));
	sat_enqueue(m, error, curuthread);
}

void
sat_session_acct( struct arsess *pas, uthread_t *owner, int error )
{
	struct sat_session_acct rec;
	PROBEHEAD(SAT_SESSION_ACCT);

	rec.sat_version = eff_sessaf;
	rec.sat_flag  = 0;
	if (pas->as_flag & AS_GOT_CHGD_INFO) {
		rec.sat_flag |= SSASF_SECONDARY;
	}
	if (pas->as_flag & AS_FLUSHED) {
		rec.sat_flag |= SSASF_FLUSHED;
	}
	rec.sat_nice  = pas->as_nice;
	rec.sat_prid  = pas->as_prid;
	rec.sat_ash   = pas->as_handle;
	rec.sat_btime = pas->as_start;
	rec.sat_etime = lbolt - pas->as_ticks;

	if (eff_sessaf > 1) {
		rec.sat_spilen = pas->as_spilen;
	}

	/* Append accounting data to record */
	sat_chain_token(m, sat_new_acct_session_token(&rec));

	/* If this is flushed accounting data, include the UID/GID */
	/* from the specified uthread				   */
	if ((pas->as_flag & AS_FLUSHED)  &&  (owner != NULL)) {
		sat_chain_token(m,
				sat_new_ugid_token(owner->ut_cred->cr_ruid,
						   owner->ut_cred->cr_rgid));
	}

	/* Append Service Provider Info */
	switch (eff_sessaf) {
	case 1:
		sat_chain_token(m,
		    sat_new_acct_spi_token((char *)&pas->as_spi));
		break;
	case 2:
		sat_chain_token(m,
		    sat_new_acct_spi2_token(pas->as_spilen,
			(char *)&pas->as_spi));
		break;
	default:
		ASSERT(0);
	}

	/* Append timers and counts */
	sat_chain_token(m, sat_new_acct_timers_token(&pas->as_timers));
	sat_chain_token(m, sat_new_acct_counts_token(&pas->as_counts));
	sat_enqueue(m, error, curuthread);
}

/*
 * This is for the "rare" case where both the value of an attribute
 * is required both before and after an operation. A classic example
 * is the effective uid on exec, where it might very well change.
 *
 * It's probably a bit bogus to use a token type to indicate which
 * attribute gets saved. While SAT_CAP_SET_TOKEN isn't ambiguose,
 * SAT_UID_LIST_TOKEN could be.
 */
void
sat_save_attr(sat_token_id_t tid, uthread_t *ut)
{
	uid_t uids[3];
	gid_t gids[2];
	sat_proc_t *sp;
	cred_t *cr;
	sat_token_t tokens;

	if (ut == NULL)
		return;

	sp = ut->ut_sat_proc;
	cr = ut->ut_cred;

	switch (tid) {
	case SAT_CAP_SET_TOKEN:
		tokens = sat_new_cap_set_token(&cr->cr_cap);
		break;
	case SAT_MAC_LABEL_TOKEN:
		tokens = sat_new_mac_label_token(cr->cr_mac);
		break;
	case SAT_UID_LIST_TOKEN:
		uids[0] = cr->cr_uid;
		uids[1] = cr->cr_ruid;
		uids[2] = cr->cr_suid;
		tokens = sat_new_uid_list_token(uids, 3);
		break;
	case SAT_GID_LIST_TOKEN:
		gids[0] = cr->cr_gid;
		gids[1] = cr->cr_rgid;
		tokens = sat_new_gid_list_token(gids, 2);
		break;
	case SAT_UGID_TOKEN:
		tokens = sat_new_ugid_token(cr->cr_uid, cr->cr_gid);
		sat_chain_token(tokens,
		    sat_new_ugid_token(cr->cr_ruid, cr->cr_rgid));
		break;
	default:
		cmn_err(CE_PANIC, "Audit: Invalid sat_save_attr (%x)", tid);
		break;
	}
	if (sp->sat_tokens)
		sat_chain_token(sp->sat_tokens, tokens);
	else
		sp->sat_tokens = tokens;
}
