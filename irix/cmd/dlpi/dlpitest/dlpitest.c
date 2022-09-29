/*
 *    Spider Alpha Test Suite
 *	
 *    Copyright (c) 1993  Spider Systems Limited
 *
 */

/*
 *	 /net/redknee/projects/common/PBRAIN/SCCS/pbrainF/dev/tests/dlpitests/0/s.test.c
 *	@(#)test.c	1.1
 *
 *	Last delta created      16:33:51 5/28/93
 *	This file extracted     16:40:30 5/28/93
 *
 *	Modifications:
 *
 */

#define LLC2

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stropts.h>
#include <sys/dlpi.h>
#include <sys/stream.h>
#include <sys/poll.h>
#ifdef LLC2
#include <sys/snet/uint.h>
#include <sys/snet/timer.h>
#include <sys/snet/ll_control.h>
#include <sys/snet/ll_proto.h>
#endif

#undef DL_MAXPRIM
#define DL_MAXPRIM DL_GET_STATISTICS_ACK

#define NULL_PRIM	0xFF
#define NULL_ERROR	0xFF
#define U32FAIL		0xFFFFFFFF	/* Unsigned 32 bit */

#define MAXCMDLEN	20
#define MAXMSGLEN	75
#define MAXADDRLEN	80
#define DEFHWLEN	6

#define DEVICE		"/dev/llc2"

struct dlpi_addr {
	int length;
	unsigned char addr[MAXADDRLEN];
};

struct command {
	char		name[MAXCMDLEN];
#ifdef __STDC__
	void		(*parse_func)(struct command *, int, char **);
#else
	void		(*parse_func)();
#endif
	void		(*work_func)();
	char		usage[MAXMSGLEN];
};

/*
 * Maximum length of variable name.
 */
#define MAXVARLEN	8

/*
 * Maximum # of active variables.
 */
#define NUMVARS		30

/*
 * Variable, to hold correlation or token value from DL_TOKEN_ACK,
 * DL_CONNECT_IND, etc.  A variable consists of a name and a value
 * (all variables are of type long), and a 'last_use' count, so that
 * the least recently used variable can be reused if all the NUMVARS
 * variable slots are in use.
 */
struct variable {
	char		name[MAXVARLEN+1];
	int		last_use;
	dlu32_t		value;
};

struct variable variables[NUMVARS];


#ifdef __STDC__
void		scan_and_do(FILE *);
void		work(int, char **);
void		do_cmd_no_args(struct command *, int, char **);
void		do_cmd_open_args(struct command *, int, char **);
void		do_cmd_one_int(struct command *, int, char **);
void		do_cmd_two_ints(struct command *, int, char **);
void		do_cmd_three_ints(struct command *, int, char **);
void		do_cmd_six_ints(struct command *, int, char **);
void		do_cmd_int_varname(struct command *, int, char **);
void		do_cmd_connect_ind(struct command *, int, char **);
void		do_cmd_connect_res(struct command *, int, char **);
void		do_cmd_discon_req(struct command *, int, char **);
void		do_cmd_int_dlsap(struct command *, int, char **);
void		do_cmd_int_dlsap_int(struct command *, int, char **);
void		do_cmd_xid_test_req(struct command *, int, char **);
void		do_cmd_xid_test_res(struct command *, int, char **);
void		do_cmd_int_hexstr(struct command *, int, char **);
void		do_cmd_int_opt_hexstr(struct command *, int, char **);
void		do_cmd_int_dlsap_hexstr(struct command *, int, char **);
void		do_cmd_int_and_state(struct command *, int, char **);
void		usage(struct command *);
void		do_help();
void		do_quit();
int		read_line(FILE *, int *, char **, int);
int		is_space_str(char *);
void		uppercase_str(char *);
void		lowercase_str(char *);
int		parse_number(char *);
int		parse_hex(char *);
int		parse_hexstr(char *, u_char *);
int		parse_dlsap(char *, struct dlpi_addr *);
int		parse_varname(char *);
dlu32_t		variable_value(char *);
dlu32_t		parse_num_or_var(char *);
dlu32_t		parse_hex_or_var(char *);
void		parse_prim_and_error(int, char **);
dlu32_t		parse_prim(char *);
dlu32_t		parse_dlerror(char *);
int		parse_dlstate(char *);
void		do_open(int);
void		do_close(int);
void		do_sleep(int);
void		do_print_vars();
void		do_poll(int, int, int);
void		do_pollerr(int, int, int);
void		do_poll_common(int, int, int, int);
void		dl_info(int);
void		dl_check_state(int, int);
void		dl_attach(int, dlu32_t);
void		dl_detach(int);
void		dl_bind(int, dlu32_t, int, int, int, int);
void		dl_unbind(int);
void		dl_subs_bind(int, struct dlpi_addr *, int);
void		dl_subs_unbind(int, struct dlpi_addr *);
void		dl_connect_req(int, struct dlpi_addr *, int);
void		dl_connect_ind(int, int, int);
void		dl_connect_res(int, dlu32_t, dlu32_t);
void		dl_connect_con(int);
void		dl_token(int, int);
void		dl_disconnect_req(int, int);
void		dl_disconnect_ind(int, int);
void		dl_reset_req(int, int);
void		dl_reset_res(int);
void		dl_send_data(int, char *, int);
void		dl_recv_data(int, char *, int);
void		dl_send_udata(int, struct dlpi_addr *, char *, int);
void		dl_recv_udata(int, char *, int);
void		dl_test_req(int, struct dlpi_addr *, int, int, char *, int);
void		dl_test_res(int, struct dlpi_addr *, int, char *, int);
void		dl_xid_req(int, struct dlpi_addr *, int, int, char *, int);
void		dl_xid_res(int, struct dlpi_addr *, int, char *, int);
void		dl_test_xid_req(int, int, int, struct dlpi_addr *, int, int,
				char *, int);
void		dl_test_ind(int, int);
void		dl_test_con(int, int);
void		dl_xid_ind(int, int);
void		dl_xid_con(int, int);
void		dl_test_xid_ind(int, int, int);
void		l_async_notif(int);
void		l_disab_notif(int);
void		dl_enabmulti_req(int, struct dlpi_addr *);
void		dl_disabmulti_req(int, struct dlpi_addr *);
void		dl_enab_disab_multi(int, int, struct dlpi_addr *);
void		dl_get_statistics(int);
void		dl_phys_addr(int, int);
void		dl_set_phys_addr(int, struct dlpi_addr *);
void		compare_data(char *, int, char *, int);
int		expect(int, dlu32_t);
void		print_prim(union DL_primitives *);
void		print_dlsap(unsigned char *, dlu32_t);
void		print_hex(u_char *, int);
#else /*__STDC__*/
void		scan_and_do();
void		work();
void		do_cmd_no_args();
void		do_cmd_open_args();
void		do_cmd_one_int();
void		do_cmd_two_ints();
void		do_cmd_three_ints();
void		do_cmd_six_ints();
void		do_cmd_int_varname();
void		do_cmd_connect_ind();
void		do_cmd_connect_res();
void		do_cmd_discon_req();
void		do_cmd_int_dlsap();
void		do_cmd_int_dlsap_int();
void		do_cmd_xid_test_req();
void		do_cmd_xid_test_res();
void		do_cmd_int_hexstr();
void		do_cmd_int_opt_hexstr();
void		do_cmd_int_dlsap_hexstr();
void		do_cmd_int_and_state();
void		usage();
void		do_help();
void		do_quit();
int		read_line();
int		is_space_str();
void		uppercase_str();
void		lowercase_str();
int		parse_number();
int		parse_hex();
int		parse_hexstr();
int		parse_dlsap();
int		parse_varname();
dlu32_t		variable_value();
dlu32_t		parse_num_or_var();
dlu32_t		parse_hex_or_var();
void		parse_prim_and_error();
dlu32_t		parse_prim();
dlu32_t		parse_dlerror();
int		parse_dlstate();
void		do_open();
void		do_close();
void		do_sleep();
void		do_print_vars();
void		do_poll();
void		do_pollerr();
void		do_poll_common();
void		dl_info();
void		dl_check_state();
void		dl_attach();
void		dl_detach();
void		dl_bind();
void		dl_unbind();
void		dl_subs_bind();
void		dl_subs_unbind();
void		dl_connect_req();
void		dl_connect_ind();
void		dl_connect_res();
void		dl_connect_con();
void		dl_token();
void		dl_disconnect_req();
void		dl_disconnect_ind();
void		dl_reset_req();
void		dl_reset_res();
void		dl_send_data();
void		dl_recv_data();
void		dl_send_udata();
void		dl_recv_udata();
void		dl_test_req();
void		dl_test_res();
void		dl_xid_req();
void		dl_xid_res();
void		dl_test_xid_req();
void		dl_test_ind();
void		dl_test_con();
void		dl_xid_ind();
void		dl_xid_con();
void		dl_test_xid_ind();
void		l_async_notif();
void		l_disab_notif();
void		dl_enabmulti_req();
void		dl_disabmulti_req();
void		dl_enab_disab_multi();
void		dl_get_statistics();
void		dl_phys_addr();
void		dl_set_phys_addr();
void		compare_data();
int		expect();
void		print_prim();
void		print_dlsap();
void		print_hex();
#endif


struct command	commands[] = {
	{"?",		do_cmd_no_args,		do_help, ""},
	{"help",	do_cmd_no_args,		do_help, ""},
	{"quit",	do_cmd_no_args,		do_quit, ""},
	{"q",		do_cmd_no_args,		do_quit, ""},
	{"open",	do_cmd_open_args,	do_open, "[ <varname> ]"},
	{"close",	do_cmd_one_int,		do_close,
		"<stream>"},
	{"sleep",	do_cmd_one_int,		do_sleep,
		"<# seconds>"},
	{"print_vars",	do_cmd_no_args,		do_print_vars, ""},
	{"poll",	do_cmd_three_ints,	do_poll,
		"<low fd> <high fd> <expected fd>"},
	{"pollerr",	do_cmd_three_ints,	do_pollerr,
		"<low fd> <high fd> <expected fd>"},
	{"info",	do_cmd_one_int,		dl_info,
		"<stream>"},
	{"check_state",	do_cmd_int_and_state,	dl_check_state,
		"<stream> <expected state>"},
	{"attach",	do_cmd_two_ints,	dl_attach,
		"<stream> <ppa>",},
	{"detach",	do_cmd_one_int,		dl_detach,
		"<stream>"},
	{"bind",	do_cmd_six_ints,	dl_bind,
		"<stream> <sap> <srv_mode> <max_conind> <conn_mgmt> <xidtest_flg>",},
	{"unbind",	do_cmd_one_int,		dl_unbind,
		"<stream>"},
	{"subs_bind",	do_cmd_int_dlsap_int,	dl_subs_bind,
		"<stream> <dlsap addr> <peer (1) or hierarchical (2) class>"},
	{"subs_unbind",	do_cmd_int_dlsap,	dl_subs_unbind,
		"<stream> <dlsap addr>"},
	{"connect",	do_cmd_int_dlsap_int,	dl_connect_req,
		"<stream> <dlsap addr> <wait for response>"},
	{"connect_ind",	do_cmd_connect_ind,	dl_connect_ind,
		"<stream> <reply (1 = send DL_CONNECT_RES)> <varname>"},
	{"connect_res",	do_cmd_connect_res,	dl_connect_res,
		"<stream> <correlation> <token>"},
	{"connect_con",	do_cmd_one_int,		dl_connect_con,
		"<stream>"},
	{"token",	do_cmd_int_varname,	dl_token,
		"<stream> <var_name>"},
	{"disconnect",	do_cmd_discon_req,	dl_disconnect_req,
		"<stream> <correlation>"},
	{"disconnect_ind", do_cmd_discon_req,	dl_disconnect_ind,
		"<stream> <expected_correlation>"},
	{"reset",	do_cmd_two_ints,	dl_reset_req,
		"<stream> <wait for response>"},
	{"reset_res",	do_cmd_one_int,		dl_reset_res,
		"<stream>"},
	{"send_data",	do_cmd_int_hexstr,	dl_send_data,
		"<stream> <quoted sequence of hex digits>"},
	{"recv_data",	do_cmd_int_opt_hexstr,	dl_recv_data,
		"<stream> [<quoted sequence of hex digits>]"},
	{"send_udata",	do_cmd_int_dlsap_hexstr, dl_send_udata,
		"<stream> <dlsap addr> <quoted sequence of hex digits>"},
	{"recv_udata",	do_cmd_int_opt_hexstr,	dl_recv_udata,
		"<stream> [<quoted sequence of hex digits>]"},
	{"test_req",	do_cmd_xid_test_req,	dl_test_req,
	   "<stream> <dlsap addr> <pf_bit> <wait> <quoted sequence of hex digits>"},
	{"test_ind",	do_cmd_two_ints,	dl_test_ind,
		"<stream> <expected pf_bit>"},
	{"test_res",	do_cmd_xid_test_res,	dl_test_res,
	   "<stream> <dlsap addr> <pf_bit> <quoted sequence of hex digits>"},
	{"test_con",	do_cmd_two_ints,	dl_test_con,
		"<stream> <expected pf_bit>"},
	{"xid_req",	do_cmd_xid_test_req,	dl_xid_req,
	   "<stream> <dlsap addr> <pf_bit> <wait> <quoted sequence of hex digits>"},
	{"xid_ind",	do_cmd_two_ints,	dl_xid_ind,
		"<stream> <expected pf_bit>"},
	{"xid_res",	do_cmd_xid_test_res,	dl_xid_res,
	   "<stream> <dlsap addr> <pf_bit> <quoted sequence of hex digits>"},
	{"xid_con",	do_cmd_two_ints,	dl_xid_con,
		"<stream> <expected pf_bit>"},
	{"enab_multi",	do_cmd_int_dlsap,	dl_enabmulti_req,
		"<stream> <dlsap addr>"},
	{"disab_multi",	do_cmd_int_dlsap,	dl_disabmulti_req,
		"<stream> <dlsap addr>"},
	{"get_stats",	do_cmd_one_int,		dl_get_statistics,
		"<stream>"},
	{"phys_addr",	do_cmd_two_ints,	dl_phys_addr,
		"<stream> <factory (1) or current (2) type>"},
	{"set_phys_addr", do_cmd_int_dlsap,	dl_set_phys_addr,
		"<stream> <dlsap addr>"},
};


int		file_input;
dlu32_t		expected_prim;
dlu32_t		expected_error;
int		use_count;
#define DATABUF_LEN	300
u_char		databuf[DATABUF_LEN];
#define MAXPRIMSZ	360  /* DL_GET_STATISTICS_ACK is 336 long in 6.0 */
char		resultbuf[MAXPRIMSZ];

extern char	*sys_errlist[];

static char *dl_modes[] = {
	"Invalid",
	"DL_CODLS: connection-oriented service",
	"DL_CLDLS: connectionless data link service",
	"DL_CODLS or DL_CLDLS",
	"DL_ACLDLS: acknowledged connectionless service"
};

static char *dl_states[] = { 
	"DL_UNBOUND: PPA attached",
	"DL_BIND_PENDING: Waiting ack of DL_BIND_REQ",
	"DL_UNBIND_PENDING: Waiting ack of DL_UNBIND_REQ",
	"DL_IDLE: dlsap bound, awaiting use",
	"DL_UNATTACHED: PPA not attached",
	"DL_ATTACH_PENDING: Waiting ack of DL_ATTACH_REQ",
	"DL_DETACH_PENDING: Waiting ack of DL_DETACH_REQ",
	"DL_UDQOS_PENDING: Waiting ack of DL_UDQOS_REQ",
	"DL_OUTCON_PENDING: outgoing connection, awaiting DL_CONN_CON",
	"DL_INCON_PENDING: incoming connection, awaiting DL_CONN_RES",
	"DL_CONN_RES_PENDING: Waiting ack of DL_CONNECT_RES",
	"DL_DATAXFER: connection-oriented data transfer",
	"DL_USER_RESET_PENDING: user initiated reset, awaiting DL_RESET_CON",
	"DL_PROV_RESET_PENDING: provider initiated reset, awaiting DL_RESET_RES",
	"DL_RESET_RES_PENDING: Waiting ack of DL_RESET_RES",
	"DL_DISCON8_PENDING: Waiting ack of DL_DISC_REQ when in DL_OUTCON_PENDING",
	"DL_DISCON9_PENDING: Waiting ack of DL_DISC_REQ when in DL_INCON_PENDING",
	"DL_DISCON11_PENDING: Waiting ack of DL_DISC_REQ when in DL_DATAXFER",
	"DL_DISCON12_PENDING: Waiting ack of DL_DISC_REQ when in DL_USER_RESET_PENDING",
	"DL_DISCON13_PENDING: Waiting ack of DL_DISC_REQ when in DL_DL_PROV_RESET_PENDING",
	"DL_SUBS_BIND_PND: Waiting ack of DL_SUBS_BIND_REQ",
	"DL_SUBS_UNBIND_PND: Waiting ack of DL_SUBS_UNBIND_REQ"
};

static char *dl_prim[] = {
	"DL_INFO_REQ",
	"DL_BIND_REQ",
	"DL_UNBIND_REQ",
	"DL_INFO_ACK",
	"DL_BIND_ACK",
	"DL_ERROR_ACK",
	"DL_OK_ACK",
	"DL_UNITDATA_REQ",
	"DL_UNITDATA_IND",
	"DL_UDERROR_IND",
	"DL_UDQOS_REQ",
	"DL_ATTACH_REQ",
	"DL_DETACH_REQ",
	"DL_CONNECT_REQ",
	"DL_CONNECT_IND",
	"DL_CONNECT_RES",
	"DL_CONNECT_CON",
	"DL_TOKEN_REQ",
	"DL_TOKEN_ACK",
	"DL_DISCONNECT_REQ",
	"DL_DISCONNECT_IND",
	"DL_SUBS_UNBIND_REQ",
	"not used",
	"DL_RESET_REQ",
	"DL_RESET_IND",
	"DL_RESET_RES",
	"DL_RESET_CON",
	"DL_SUBS_BIND_REQ",
	"DL_SUBS_BIND_ACK",
	"DL_ENABMULTI_REQ",
	"DL_DISABMULTI_REQ",
	"DL_PROMISCON_REQ",
	"DL_PROMISCOFF_REQ",
	"DL_DATA_ACK_REQ",
	"DL_DATA_ACK_IND",
	"DL_DATA_ACK_STATUS_IND",
	"DL_REPLY_REQ",
	"DL_REPLY_IND",
	"DL_REPLY_STATUS_IND",
	"DL_REPLY_UPDATE_REQ",
	"DL_REPLY_UPDATE_STATUS_IND",
	"DL_XID_REQ",
	"DL_XID_IND",
	"DL_XID_RES",
	"DL_XID_CON",
	"DL_TEST_REQ",
	"DL_TEST_IND",
	"DL_TEST_RES",
	"DL_TEST_CON",
	"DL_PHYS_ADDR_REQ",
	"DL_PHYS_ADDR_ACK",
	"DL_SET_PHYS_ADDR_REQ",
	"DL_GET_STATISTICS_REQ",
	"DL_GET_STATISTICS_ACK",
};

static char *dl_media[] = {
        "DL_CSMACD",
        "DL_TPB",
        "DL_TPR",
        "DL_METRO",
        "DL_ETHER",
        "DL_HDLC",
        "DL_CHAR",
        "DL_CTCA",
        "DL_FDDI",
        "DL_OTHER",
};


static char *dl_errors[] = {
	"DL_BADSAP: Bad LSAP selector",
	"DL_BADADDR: DLSAP address in improper format or invalid",
	"DL_ACCESS:  Improper permissions for request",
	"DL_OUTSTATE: Primitive issued in improper state",
	"DL_SYSERR: UNIX system error occurred",
	"DL_BADCORR: Sequence number not from outstanding DL_CONN_IND",
	"DL_BADDATA: User data exceeded provider limit",
	"DL_UNSUPPORTED: Requested service not supplied by provider",
	"DL_BADPPA: Specified PPA was invalid",
	"DL_BADPRIM: Primitive received is not known by DLS provider",
	"DL_BADQOSPARAM: QOS parameters contained invalid values",
	"DL_BADQOSTYPE: QOS structure type is unknown or unsupported",
	"DL_BADTOKEN: Token used not associated with an active stream",
	"DL_BOUND: Attempted second bind with dl_max_conind or dl_conn_mgmt > 0 on same DLSAP or PPA",
	"DL_INITFAILED: Physical Link initialization failed",
	"DL_NOADDR: Provider couldn't allocate alternate address",
	"DL_NOTINIT: Physical Link not initialized",
	"DL_UNDELIVERABLE: Previous data unit could not be delivered",
	"DL_NOTSUPPORTED: Primitive is known but not supported by DLS provider",
	"DL_TOOMANY: limit exceeded",
	"DL_NOTENAB: Promiscuous mode not enabled",
	"DL_BUSY: Other streams for a particular PPA in the post-attached state",
	"DL_NOAUTO: Automatic handling of XID & TEST responses not supported",
	"DL_NOXIDAUTO: Automatic handling of XID not supported",
	"DL_NOTESTAUTO: Automatic handling of TEST not supported",
	"DL_XIDAUTO: Automatic handling of XID response",
	"DL_TESTAUTO: Automatic handling of TEST response",
	"DL_PENDING: pending outstanding connect indications"
};

static char *disc_reasons[] = {
	"DL_CONREJ_DEST_UNKNOWN",
	"DL_CONREJ_DEST_UNREACH_PERMANENT",
	"DL_CONREJ_DEST_UNREACH_TRANSIENT",
	"DL_CONREJ_QOS_UNAVAIL_PERMANENT",
	"DL_CONREJ_QOS_UNAVAIL_TRANSIENT",
	"DL_CONREJ_PERMANENT_COND",
	"DL_CONREJ_TRANSIENT_COND",
	"DL_DISC_ABNORMAL_CONDITION",
	"DL_DISC_NORMAL_CONDITION",
	"DL_DISC_PERMANENT_CONDITION",
	"DL_DISC_TRANSIENT_CONDITION",
	"DL_DISC_UNSPECIFIED"
};

static char *reset_reasons[] = {
	"DL_RESET_FLOW_CONTROL",
	"DL_RESET_LINK_ERROR",
	"DL_RESET_RESYNCH"
};



void DispHex (char *src, int len)

{
    char    *tmp;
    int    i,j;

    i = (j = 0);
    tmp = src;
    while (len > 0)
        {
        if (i == 8)
            {
            j++;
            if ((j == 1) || (j == 3))
                printf ("  %02x", *tmp);
            else if (j == 2)
                printf ("    %02x", *tmp);
            else
                {
                printf ("\n  %02x", *tmp);
                j = 0;
                }
            i = 1;
            tmp++;
            }
        else
            {
            printf ("%02x", *tmp);
            i++;
            tmp++;
            }
        len--;
        }
    printf("\n");
} /* end of DispHex */

main(argc, argv)
	int		argc;
	char		**argv;
{
	int		s;

	file_input = !isatty(0);
	if (argc != 1) {
		fprintf(stderr, "usage: %s\n", argv[0]);
		exit(1);
	}
	signal(SIGHUP, exit);
	signal(SIGINT, exit);
	signal(SIGTERM, exit);
	scan_and_do(stdin);
	exit(0);
}


/* ------------------------------------------------------------------------- */

#define MAX_ARGC 20
#define LINELEN 512

char		line[LINELEN];
int		lineno = 0;

void
scan_and_do(fp)
	FILE		*fp;
{
	int		argc;
	char		*args[MAX_ARGC];
	char		**argv;

	if (!file_input) {
		(void) printf("Type '?' for a list of commands\n");
	}
	while (1) {
		argc = 0;
		while (argc == 0) {
			if (!file_input) {
				(void) printf("> ");
			}
			if (read_line(fp, &argc, args, MAX_ARGC) == 0)
				/* EOF */
				return;
		}
		argv = args;
		work(argc, argv);
	}
}


void
work(argc, argv) 
	int		argc;
	char		**argv;
{
	int		num_cmds;
	int		i;

	expected_prim = NULL_PRIM;
	expected_error = NULL_ERROR;
	num_cmds = sizeof(commands) / sizeof(struct command);
	for (i = 0; i < num_cmds; i++) {
		if (strcmp(argv[0], commands[i].name) == 0) {
			argc--; argv++;
			commands[i].parse_func(&commands[i], argc, argv);
			return;
		}
	}
	(void) fprintf(stderr, "\t**Unknown command: %s\n", argv[0]);
	if (!file_input) {
		(void) printf("\tType '?' for a list of commands\n");
	}
}


/*
 * Handle command that takes no args.
 */
void
do_cmd_no_args(cmd, argc, argv)
	struct command	*cmd;
	int		argc;
	char		**argv;
{
	if (argc > 0) {
		usage(cmd);
		return;
	}
	cmd->work_func();
}


/*
 * Handle args for open command -- either no args or a variable name.
 */
void
do_cmd_open_args(cmd, argc, argv)
	struct command	*cmd;
	int		argc;
	char		**argv;
{
	int		var_index;

	if (argc > 1) {
		usage(cmd);
		return;
	}
	if (argc == 1) {
		var_index = parse_varname(argv[0]);
		if (var_index == -1) {
			return;
		}
	} else {
		var_index = -1;
	}
	do_open(var_index);
}


/*
 * Handle command that takes one integer arg.
 */
void
do_cmd_one_int(cmd, argc, argv)
	struct command	*cmd;
	int		argc;
	char		**argv;
{
	dlu32_t		s;

	if (argc < 1 || argc > 3 ||
	    (s = parse_num_or_var(argv[0])) == U32FAIL) {
		usage(cmd);
		return;
	}
	argc--; argv++;
	parse_prim_and_error(argc, argv);
	cmd->work_func((int)s);
}


/*
 * Handle command that takes two integer args.
 */
void
do_cmd_two_ints(cmd, argc, argv)
	struct command	*cmd;
	int		argc;
	char		**argv;
{
	dlu32_t		s;
	int		arg1;

	if (argc < 2 || argc > 4 ||
	    (s = parse_num_or_var(argv[0])) == U32FAIL ||
	    (arg1 = parse_hex(argv[1])) < 0) {
		usage(cmd);
		return;
	}
	argc -= 2;
	argv += 2;
	parse_prim_and_error(argc, argv);
	cmd->work_func((int)s, arg1);
}


/*
 * Handle command that takes three integer args.
 */
void
do_cmd_three_ints(cmd, argc, argv)
	struct command	*cmd;
	int		argc;
	char		**argv;
{
	dlu32_t		s;
	int		arg1;
	int		arg2;

	if (argc < 3 || argc > 5 ||
	    (s = parse_num_or_var(argv[0])) == U32FAIL ||
	    (arg1 = parse_hex_or_var(argv[1])) < 0 ||
	    (arg2 = parse_hex_or_var(argv[2])) < 0) {
		usage(cmd);
		return;
	}
	argc -= 3;
	argv += 3;
	parse_prim_and_error(argc, argv);
	cmd->work_func((int)s, arg1, arg2);
}


/*
 * Handle command that takes six integer args.
 */
void
do_cmd_six_ints(cmd, argc, argv)
	struct command	*cmd;
	int		argc;
	char		**argv;
{
	dlu32_t		s;
	int		arg1;
	int		arg2;
	int		arg3;
	int		arg4;
	int		arg5;

	if (argc < 6 || argc > 8 ||
	    (s = parse_num_or_var(argv[0])) == U32FAIL ||
	    (arg1 = parse_hex(argv[1])) < 0 ||
	    (arg2 = parse_hex(argv[2])) < 0 ||
	    (arg3 = parse_hex(argv[3])) < 0 ||
	    (arg4 = parse_hex(argv[4])) < 0 ||
	    (arg5 = parse_hex(argv[5])) < 0) {
		usage(cmd);
		return;
	}
	argc -= 6;
	argv += 6;
	parse_prim_and_error(argc, argv);
	cmd->work_func((int)s, arg1, arg2, arg3, arg4, arg5);
}


/*
 * Handle command that takes one integer plus a variable name.
 */
void
do_cmd_int_varname(cmd, argc, argv)
	struct command	*cmd;
	int		argc;
	char		**argv;
{
	int		s;
	int		var_index;

	if (argc < 2 || argc > 4 ||
	    (s = parse_num_or_var(argv[0])) < 0 ||
	    (var_index = parse_varname(argv[1])) < 0) {
		usage(cmd);
		return;
	}
	argc -= 2;
	argv += 2;
	parse_prim_and_error(argc, argv);
	cmd->work_func(s, var_index);
}


/*
 * Handle args for the dl_connect_ind command.  This command takes two
 * integers, <stream> and <reply>, and an optional variable name to hold the
 * correlation value received in the DL_CONNECT_IND message.
 * If <reply> = 1, the variable name is not parsed, and is ignored.
 */
void
do_cmd_connect_ind(cmd, argc, argv)
	struct command	*cmd;
	int		argc;
	char		**argv;
{
	int		s;
	int		reply;
	int		var_index;

	if (argc < 3 || argc > 5 ||
	    (s = parse_num_or_var(argv[0])) < 0 ||
	    (reply = parse_hex(argv[1])) < 0) {
		usage(cmd);
		return;
	}
	if (reply != 1) {
		if ((var_index = parse_varname(argv[2])) < 0) {
			usage(cmd);
			return;
		}
	}
	argc -= 3;
	argv += 3;
	parse_prim_and_error(argc, argv);
	cmd->work_func(s, reply, var_index);
}


/*
 * Handle args for the dl_connect_res command.  This command takes three
 * integers, <stream>, <correlation>, and <token>.  The latter two can
 * be variable names, in which case the values of the variables are
 * substituted for the correlation and/or token.
 */
void
do_cmd_connect_res(cmd, argc, argv)
	struct command	*cmd;
	int		argc;
	char		**argv;
{
	int		s;
	dlu32_t		corr;
	dlu32_t		token;

	if (argc < 3 || argc > 5 ||
	    (s = parse_num_or_var(argv[0])) < 0 ||
	    (corr = parse_hex_or_var(argv[1])) == U32FAIL ||
	    (token = parse_hex_or_var(argv[2])) == U32FAIL) {
		usage(cmd);
		return;
	}
	argc -= 3;
	argv += 3;
	parse_prim_and_error(argc, argv);
/*	cmd->work_func(s, corr, token); */
	dl_connect_res(s, corr, token);
}


/*
 * Handle args for the disconnect and disconnect_ind commands.  These
 * commands take two integers, <stream> and <correlation>, where correlation
 * can be a variable.
 */
void
do_cmd_discon_req(cmd, argc, argv)
	struct command	*cmd;
	int		argc;
	char		**argv;
{
	int		s;
	int		corr;

	if (argc < 2 || argc > 4 ||
	    (s = parse_num_or_var(argv[0])) < 0 ||
	    (corr = parse_hex_or_var(argv[1])) < 0) {
		usage(cmd);
		return;
	}
	argc -= 2;
	argv += 2;
	parse_prim_and_error(argc, argv);
	cmd->work_func(s, corr);
}


/*
 * Handle command that takes a DLSAP address.
 */
void
do_cmd_int_dlsap(cmd, argc, argv)
	struct command	*cmd;
	int		argc;
	char		**argv;
{
	int		s;
	struct dlpi_addr dlsap;

	if (argc < 2 || argc > 4 ||
	    (s = parse_num_or_var(argv[0])) < 0 ||
	    parse_dlsap(argv[1], &dlsap) < 0) {
		usage(cmd);
		return;
	}
	argc -= 2;
	argv += 2;
	parse_prim_and_error(argc, argv);
	cmd->work_func(s, &dlsap);
}


/*
 * Handle command that takes one int plus a DLSAP address.
 */
void
do_cmd_int_dlsap_int(cmd, argc, argv)
	struct command	*cmd;
	int		argc;
	char		**argv;
{
	int		s;
	struct dlpi_addr dlsap;
	int		arg1;

	if (argc < 3 || argc > 5 ||
	    (s = parse_num_or_var(argv[0])) < 0 ||
	    parse_dlsap(argv[1], &dlsap) < 0 ||
	    (arg1 = parse_number(argv[2])) < 0) {
		usage(cmd);
		return;
	}
	argc -= 3;
	argv += 3;
	parse_prim_and_error(argc, argv);
	cmd->work_func(s, &dlsap, arg1);
}


/*
 * Handle args for the dl_test_req and dl_xid_req commands.  These commands
 * take a DLSAP address, two ints, and a quoted sequence of hex digits.
 */
void
do_cmd_xid_test_req(cmd, argc, argv)
	struct command	*cmd;
	int		argc;
	char		**argv;
{
	int		s;
	struct dlpi_addr dlsap;
	int		arg1;
	int		arg2;
	int		len;

	if (argc < 5 || argc > 7 ||
	    (s = parse_num_or_var(argv[0])) < 0 ||
	    parse_dlsap(argv[1], &dlsap) < 0 ||
	    (arg1 = parse_number(argv[2])) < 0 ||
	    (arg2 = parse_number(argv[3])) < 0 ||
	    (len = parse_hexstr(argv[4], databuf)) < 0) {
		usage(cmd);
		return;
	}
	argc -= 5;
	argv += 5;
	parse_prim_and_error(argc, argv);
	cmd->work_func(s, &dlsap, arg1, arg2, databuf, len);
}


/*
 * Handle args for the dl_test_res and dl_xid_res commands.  These commands
 * take a DLSAP address, two ints, and a quoted sequence of hex digits.
 */
void
do_cmd_xid_test_res(cmd, argc, argv)
	struct command	*cmd;
	int		argc;
	char		**argv;
{
	int		s;
	struct dlpi_addr dlsap;
	int		arg1;
	int		len;

	if (argc < 4 || argc > 6 ||
	    (s = parse_num_or_var(argv[0])) < 0 ||
	    parse_dlsap(argv[1], &dlsap) < 0 ||
	    (arg1 = parse_number(argv[2])) < 0 ||
	    (len = parse_hexstr(argv[3], databuf)) < 0) {
		usage(cmd);
		return;
	}
	argc -= 4;
	argv += 4;
	parse_prim_and_error(argc, argv);
	cmd->work_func(s, &dlsap, arg1, databuf, len);
}


/*
 * Handle command that takes one int plus a quoted string of hex bytes.
 */
void
do_cmd_int_hexstr(cmd, argc, argv)
	struct command	*cmd;
	int		argc;
	char		**argv;
{
	int		s;
	struct dlpi_addr dlsap;
	int		len;

	if (argc < 2 || argc > 4 ||
	    (s = parse_num_or_var(argv[0])) < 0 ||
	    (len = parse_hexstr(argv[1], databuf)) < 0) {
		usage(cmd);
		return;
	}
	argc -= 2;
	argv += 2;
	parse_prim_and_error(argc, argv);
	cmd->work_func(s, databuf, len);
}


/*
 * Handle command that takes one int plus (optionally) a quoted string of
 * hex bytes.
 */
void
do_cmd_int_opt_hexstr(cmd, argc, argv)
	struct command	*cmd;
	int		argc;
	char		**argv;
{
	int		s;
	struct dlpi_addr dlsap;
	u_char		buf[DATABUF_LEN];
	int		len = 0;
	char		c;

	if (argc < 1 || argc > 4 ||
	    (s = parse_num_or_var(argv[0])) < 0) {
		usage(cmd);
		return;
	}
	argc--;
	argv++;
	if (argc > 0) {
		c = argv[0][0];
		if (c != 'd' && c != 'D') {
			len = parse_hexstr(argv[0], buf);
			if (len < 0) {
				usage(cmd);
				return;
			}
			argc--;
			argv++;
		}
	}
	parse_prim_and_error(argc, argv);
	cmd->work_func(s, buf, len);
}


/*
 * Handle command that takes one int plus a DLSAP address plus a quoted
 * string of hex bytes.
 */
void
do_cmd_int_dlsap_hexstr(cmd, argc, argv)
	struct command	*cmd;
	int		argc;
	char		**argv;
{
	int		s;
	struct dlpi_addr dlsap;
	int		len;

	if (argc < 3 || argc > 5 ||
	    (s = parse_num_or_var(argv[0])) < 0 ||
	    parse_dlsap(argv[1], &dlsap) < 0 ||
	    (len = parse_hexstr(argv[2], databuf)) < 0) {
		usage(cmd);
		return;
	}
	argc -= 3;
	argv += 3;
	parse_prim_and_error(argc, argv);
	cmd->work_func(s, &dlsap, databuf, len);
}


/*
 * Handle command that takes one int plus a quoted string of hex bytes.
 */
void
do_cmd_int_and_state(cmd, argc, argv)
	struct command	*cmd;
	int		argc;
	char		**argv;
{
	int		s;
	int		state;

	if (argc < 2 || argc > 4 ||
	    (s = parse_num_or_var(argv[0])) < 0 ||
	    (state = parse_dlstate(argv[1])) < 0) {
		usage(cmd);
		return;
	}
	argc -= 2;
	argv += 2;
	parse_prim_and_error(argc, argv);
	cmd->work_func(s, state);
}


void
usage(cmd)
	struct command	*cmd;
{
	fprintf(stderr, "	**Usage: %s %s\n", cmd->name, cmd->usage);
}


void
do_help()
{
	printf("	help, ?		Prints this information\n");
	printf("	quit, q		Quit\n");
	printf("	open		Open a stream to the driver\n");
	printf("	close		Close a stream\n");
	printf("	poll		Poll a stream for incoming data\n");
	printf("	pollerr		Poll a stream for error condition\n");
	printf("	info		Send DL_INFO_REQ\n");
	printf("	check_state	Verify that current DLPI state is correct\n");
	printf("	attach		Send DL_ATTACH_REQ\n");
	printf("	detach		Send DL_DETACH_REQ\n");
	printf("	bind		Send DL_BIND_REQ\n");
	printf("	unbind		Send DL_UNBIND_REQ\n");
	printf("	subs_bind	Send DL_SUBS_BIND_REQ\n");
	printf("	subs_unbind	Send DL_SUBS_UNBIND_REQ\n");
	printf("	unbind		Send DL_UNBIND_REQ\n");
	printf("	connect		Send DL_CONNECT_REQ\n");
	printf("	connect_ind	Wait for DL_CONNECT_IND\n");
	printf("	connect_res	Send DL_CONNECT_RES\n");
	printf("	connect_con	Wait for DL_CONNECT_CON\n");
	printf("	token		Send DL_TOKEN_REQ\n");
	printf("	disconnect	Send DL_DISCONNECT_REQ\n");
	printf("	disconnect_ind	Receive DL_DISCONNECT_IND\n");
	printf("	reset		Send DL_RESET_REQ\n");
	printf("	reset_res	Send DL_RESET_RES\n");
	printf("	send_data	Send LLC2 data in M_DATA\n");
	printf("	recv_data	Receive LLC2 data\n");
	printf("	send_udata	Send LLC1 data in DL_UNITDATA_REQ\n");
	printf("	recv_udata	Recv LLC1 data in DL_UNITDATA_IND\n");
	printf("	test_req	Send DL_TEST_REQ\n");
	printf("	test_ind	Receive DL_TEST_IND\n");
	printf("	test_res	Send DL_TEST_RES\n");
	printf("	test_con	Receive DL_TEST_CON\n");
	printf("	xid_req		Send DL_XID_REQ\n");
	printf("	xid_ind		Receive DL_XID_IND\n");
	printf("	xid_res		Send DL_XID_RES\n");
	printf("	xid_con		Receive DL_XID_CON\n");
	printf("	enab_multi	Send DL_ENABMULTI_REQ\n");
	printf("	disab_multi	Send DL_DISABMULTI_REQ\n");
	printf("	get_stats	Send DL_GET_STATISTICS_REQ\n");
	printf("	phys_addr	Send DL_PHYS_ADDR_REQ\n");
	printf("	set_phys_addr	Send DL_SET_PHYS_ADDR_REQ\n");
}


void
do_quit()
{
	exit(0);
}


/* ------------------------------------------------------------------------- */

char		line2[LINELEN];	/* argv will point in here */

/*
 * Returns 0 on EOF.
 */
int
read_line(fp, argcp, argv, max_argc)
	FILE		*fp;
	int		*argcp;
	char		**argv;
	int		max_argc;
{
	char		*lineptr;
	char		*cp;
	int		argc;
	int		length;

	*argcp = 0;
	lineptr = fgets(line, sizeof(line), fp);
	lineno++;
	if (lineptr == NULL) 
		/* EOF */
		return (0);
	if (file_input) {
		(void) printf("%2d: %s", lineno, line);
	}
	/* Remove any trailing comments */
	cp = strchr(line, '#');
	if (cp != NULL)
		*cp = '\0';	/* Terminate string */
	/* Remove trailing LF */
	cp = strchr(line, '\n');
	if (cp != NULL) {
		*cp = '\0';
	}
	if (is_space_str(line))
		return (1);

	length = strlen(line);
	/* Handle '\'s. */
	while (line[length - 1] == '\\') {
		cp = fgets(&line[length - 1], sizeof(line) - length, fp);
		if (cp == NULL) {
			(void) fprintf(stderr, "parse error: truncated file?\n");
			(void) fprintf(stderr, "Line %d: %s\n", lineno, line);
			exit(1);
		}
		lineno++;
		if (file_input) {
			(void) printf("%2d: %s", lineno, line);
		}
		/* Remove any trailing comments */
		cp = strchr(line, '#');
		if (cp != NULL)
			*cp = '\0';	/* Terminate string */
		/* Remove trailing LF */
		cp = strchr(line, '\n');
		if (cp != NULL) {
			*cp = '\0';
		}
		length = strlen(line);
	}
	

	(void) strcpy(line2, line);
	lowercase_str(line2);

	argc = 0;
	lineptr = line2;
	while (*lineptr != NULL && argc < max_argc) {
		/* Skip leading white space */
		while (isspace((u_char)*lineptr) && *lineptr != NULL)
			lineptr++;
		/* Catch trailing spaces */
		if (*lineptr == NULL)
			break;
		if (*lineptr == '\'') {
			/* Find the next quote */
			lineptr++;
			cp = strchr(lineptr, '\'');
			if (cp == NULL) {
				(void) fprintf(stderr, "Mismatched quotes (')\n");
				(void) fprintf(stderr, "Line %d: %s\n", lineno, line);
				exit(1);
			}
			*cp = NULL;
			argv[argc++] = lineptr;
			lineptr = cp + 1;
			continue;
		}
		/* find next space */
		cp = lineptr;
		while (!isspace((u_char)*cp) && *cp != NULL)
			cp++;
		argv[argc++] = lineptr;
		if (*cp == NULL) {
			lineptr = cp;
		} else {
			*cp = NULL;
			lineptr = cp + 1;
		}
	}
	if (argc >= max_argc) {
		(void) fprintf(stderr, "Bad syntax (too many items)\n");
		(void) fprintf(stderr, "Line %d: %s\n", lineno, line);
		exit(1);
	}
	*argcp = argc;
	return (1);
}


/* ------------------------------------------------------------------------- */

is_space_str(str)
	char *str;
{
	while (*str) {
		if (!isspace((u_char)*str))
			return (0);
		str++;
	}
	return (1);
}


void
uppercase_str(str)
	char *str;
{
	while (*str) {
		if (islower((u_char)*str))
			*str = toupper(*str);
		str++;
	}
}


void
lowercase_str(str)
	char *str;
{
	while (*str) {
		if (isupper((u_char)*str))
			*str = tolower(*str);
		str++;
	}
}


int
parse_number(str)
	char		*str;
{
	char		*ptr;
	int		ret;

	ret = strtol(str, &ptr, 0);
	if (str == ptr || *ptr != NULL) {
		(void) fprintf(stderr, "	**Bad number: %s\n", str);
		return (-1);
	}
	return (ret);
}


int
parse_hex(str)
	char		*str;
{
	char		*ptr;
	int		ret;

	ret = strtol(str, &ptr, 16);
	if (str == ptr || *ptr != NULL) {
		(void) fprintf(stderr, "	**Bad hex number: %s\n", str);
		return (-1);
	}
	return (ret);
}


int
parse_hexstr(str, buf)
	char		*str;
	u_char		*buf;
{
	int		len;	/* in bytes */
	int		val;
	char		*next;

	len = 0;
	while (isspace((u_char)*str) && *str != NULL) {
		str++;
	}
	while (*str != NULL) {
		val = strtol(str, &next, 16);
		if (str == next) {
			(void) fprintf(stderr,
					"	**Bad hex number: %s\n", str);
			return (-1);
		}
		if (val == -1)
			return (-1);
		if (val > 255) {
			(void) fprintf(stderr, "**Byte value too large: %s\n",
				       str);
			return (-1);
		}
			
		*buf++ = val;
		len++;
		str = next;
		while(isspace((u_char)*str) && *str != NULL)
			str++;
	}
	return (len);
}


int
parse_dlsap(str, dlsap)
	char		*str;
	struct dlpi_addr *dlsap;
{
	unsigned int v;
	char *cp = str;
	char c;
	int i;

	for (i = 0; i < MAXADDRLEN; i++)
	{
		char *cpold;
		cpold = cp;
		while (*cp != ':' && *cp != ',' && *cp != 0)
			cp++;
		c = *cp;	/* save original terminator */
		*cp = '\0';
		cp++;
		if (sscanf(cpold, "%x", &v) != 1 || v > 255)
		{
			(void) fprintf(stderr, "	*** Bad DLSAP address: %s\n",
				str);
			return (-1);
		}

		dlsap->addr[i] = v;
		if (c == 0)
			break;
	}

	if (i == MAXADDRLEN)
	{
		(void) fprintf(stderr, "        *** Bad DLSAP address: %s\n",
			str);
		return (-1);
	}
	dlsap->length = i + 1;

	return (0);
}


#define INFINITY	100000000

/*
 * Assign a slot in the 'variables' array to hold the variable 'name'.
 * The only parsing that is done here is to ensure that the length of
 * the variable name is not greater than the limit.
 */
int
parse_varname(name)
	char		*name;
{
	int		i;
	int		oldest_use = INFINITY;
	int		oldest_index;

	if ((int) strlen(name) > MAXVARLEN) {
		fprintf(stderr, "	*** Variable name '%s' is too long\n",
			name);
		return -1;
	}
	for (i = 0; i < NUMVARS; i++) {
		/*
		 * If the variable name is already in use, use the same
		 * slot.
		 */
		if (variables[i].name[0] &&
		    strcmp(name, variables[i].name) == 0) {
			variables[i].value = 0;
			variables[i].last_use = use_count++;
			return i;
		}
	}
	for (i = 0; i < NUMVARS; i++) {
		if (variables[i].name[0] == '\0') {
			/*
			 * Found a free slot, put the variable into it.
			 */
			strcpy(variables[i].name, name);
			variables[i].value = 0;
			variables[i].last_use = use_count++;
			return i;
		}
		if (variables[i].last_use < oldest_use) {
			oldest_use = variables[i].last_use;
			oldest_index = i;
		}
	}
	/*
	 * No free slots, so put the variable in the LRU slot.
	 */
	i = oldest_index;
	strcpy(variables[i].name, name);
	variables[i].value = 0;
	variables[i].last_use = use_count++;
	return i;
}


/*
 * Return the value of the variable 'name', or -1 if 'name' is an
 * undefined variable.
 */
dlu32_t
variable_value(name)
	char		*name;
{
	int		i;

	for (i = 0; i < NUMVARS; i++) {
		if (variables[i].name[0] &&
		    strcmp(name, variables[i].name) == 0) {
			variables[i].last_use = use_count++;
			return variables[i].value;
		}
	}
	fprintf(stderr, "	*** Undefined variable '%s'\n", name);
	return U32FAIL;
}


dlu32_t
parse_num_or_var(str)
	char		*str;
{
	if (str[0] == '$') {
		/*
		 * Is a variable.
		 */
		return variable_value(++str);
	}
	return parse_number(str);
}


dlu32_t
parse_hex_or_var(str)
	char		*str;
{
	if (str[0] == '$') {
		/*
		 * Is a variable.
		 */
		return variable_value(++str);
	}
	return parse_hex(str);
}


void
parse_prim_and_error(argc, argv)
	int		argc;
	char		**argv;
{
	if (argc > 0) {
		expected_prim = (dlu32_t)parse_prim(argv[0]);
	}
	if (argc > 1 && expected_prim != NULL_PRIM) {
		expected_error = parse_dlerror(argv[1]);
	}
}


dlu32_t
parse_prim(str)
	char		*str;
{
	int		i;

	uppercase_str(str);
	for (i = 0; i <= DL_MAXPRIM; i++) {
		if (strcmp(dl_prim[i], str) == 0) {
			printf("Primitive %s=%d\n", str, i);
			return (dlu32_t)i;
		}
	}
	fprintf(stderr, "	*** Bad DLPI primitive '%s' (ignored)\n", str);
	return U32FAIL;
}


dlu32_t
parse_dlerror(str)
	char		*str;
{
	int		errnum;
	int		len = strlen(str);

	uppercase_str(str);
	for (errnum = 0; errnum <= DL_PENDING; errnum++) {
		if (strncmp(dl_errors[errnum], str, len) == 0 &&
		    dl_errors[errnum][len] == ':') {
			return errnum;
		}
	}
	fprintf(stderr, "	*** Bad DLPI error '%s' (ignored)\n", str);
	return -1;
}


int
parse_dlstate(str)
	char		*str;
{
	int		state;
	int		len = strlen(str);

	uppercase_str(str);
	for (state = 0; state <= DL_SUBS_UNBIND_PND; state++) {
		if (strncmp(dl_states[state], str, len) == 0 &&
		    dl_states[state][len] == ':') {
			return state;
		}
	}
	fprintf(stderr, "	*** Bad DLPI error '%s' (ignored)\n", str);
	return -1;
}


void
do_open(var_index)
	int		var_index;
{
	int		s;

	s = open(DEVICE, O_RDWR);
	if (s < 0) {
		perror("	**open");
	} else {
		printf("	=> %d\n", s);
		if (var_index != -1) {
			variables[var_index].value = s;
		}
	}
}


void
do_close(s)
	int		s;
{
	if (close(s) < 0) {
		perror("	**close");
	}
}


void
do_sleep(s)
	int		s;
{
	sleep(s);
}


void
do_print_vars()
{
	int		i;

	for (i = 0; i < NUMVARS; i++) {
		if (variables[i].name[0]) {
			printf(" %s	= 0x%x\n", variables[i].name,
				variables[i].value);
		}
	}
}


void
do_poll(low, high, expected)
	int		low;
	int		high;
	int		expected;
{
	do_poll_common(low, high, expected, POLLIN);
}


void
do_pollerr(low, high, expected)
	int		low;
	int		high;
	int		expected;
{
	do_poll_common(low, high, expected, POLLERR);
}


/*
 * Poll the fds numbered 'low' ... 'high' to see if the event 'event' has
 * occurred on any of them.  If the desired event has occurred on the
 * 'expected' fd, print an OK message, otherwise print an error message.
 * If 'expected' is 0, then we are not expecting to see an event on any
 * of the fds.
 */
void
do_poll_common(low, high, expected, event)
	int		low;
	int		high;
	int		expected;
	int		event;
{
	struct pollfd	*poll_fds;
	int		size;
	int		i;
	int		saw_event = 0;
	int		saw_wrong_event = 0;

	if (low > high) {
		fprintf(stderr,
			"	*** do_poll: <high_fd> must be >= <low_fd>\n");
		return;
	}
	size = high - low + 1;
	poll_fds = (struct pollfd *) malloc( size * sizeof(*poll_fds));
	if (poll_fds == NULL) {
		fprintf(stderr, "	*** do_poll: malloc failed\n");
		return;
	}
	for (i = 0; i < size; i++) {
		poll_fds[i].fd = low + i;
		poll_fds[i].events = POLLIN;
	}
	if (poll(poll_fds, size, 0) < 0) {
		perror("	**poll");
	}
	for (i = 0; i < size; i++) {
		if (poll_fds[i].revents == event) {
			if (poll_fds[i].fd == expected) {
				saw_event = 1;
			} else {
				saw_wrong_event = 1;
				if (expected > 0) {
					printf(
		"***ERROR: expected an event on fd %d, saw event on fd %d\n",
						expected, poll_fds[i].fd);
				} else {
					printf(
		"***ERROR: wasn't expecting an event, saw event on fd %d\n",
						poll_fds[i].fd);
				}
			}
		}
		printf("fd %d, revents 0x%x\n", poll_fds[i].fd,
			poll_fds[i].revents);
	}
	if (saw_event) {
		printf("Saw expected event on fd %d\n", expected);
	} else if (expected > 0) {
		printf("***ERROR: expected event not seen on fd %d\n",
			expected);
	} else if (!saw_wrong_event) {
		printf("No event seen, as expected\n");
	}
	free(poll_fds);
}


void
dl_info(s)
	int		s;
{
	struct strbuf	ctl;
	dl_info_req_t	info;
	int		flags = RS_HIPRI;

	ctl.len = DL_INFO_REQ_SIZE;
	ctl.buf = (char *) &info;
	info.dl_primitive = DL_INFO_REQ;
	if (putmsg(s, &ctl, NULL, flags) < 0) {
		perror("	**putmsg DL_INFO_REQ");
		return;
	}
	expect(s, DL_INFO_ACK);
}


void
dl_check_state(s, state)
	int		s;
	int		state;
{
	struct strbuf	ctl;
	dl_info_req_t	info;
	dl_info_ack_t	*info_ack;

	ctl.len = DL_INFO_REQ_SIZE;
	ctl.buf = (char *) &info;
	info.dl_primitive = DL_INFO_REQ;
	if (putmsg(s, &ctl, NULL, 0) < 0) {
		perror("	**putmsg DL_INFO_REQ");
		return;
	}
	if (expect(s, DL_INFO_ACK) == 0) {
		info_ack = (dl_info_ack_t *) resultbuf;
		if (info_ack->dl_current_state == state) {
			printf("Stream is in expected state\n");
		} else {
			printf("*** ERROR: Expected state %s\n",
				dl_states[state]);
			printf("                      saw %s\n",
				dl_states[info_ack->dl_current_state]);
		}
	}
}


void
dl_attach(s, ppa)
	int		s;
	dlu32_t		ppa;
{
	struct strbuf	ctl;
	dl_attach_req_t	attach;

	ctl.len = DL_ATTACH_REQ_SIZE;
	ctl.buf = (char *) &attach;
	attach.dl_primitive = DL_ATTACH_REQ;
	attach.dl_ppa = ppa;
	if (putmsg(s, &ctl, NULL, 0) < 0) {
		perror("	**putmsg DL_ATTACH_REQ");
		return;
	}
	expect(s, DL_OK_ACK);
}


void
dl_detach(s)
	int		s;
{
	struct strbuf	ctl;
	dl_detach_req_t	detach;

	ctl.len = DL_DETACH_REQ_SIZE;
	ctl.buf = (char *) &detach;
	detach.dl_primitive = DL_DETACH_REQ;
	if (putmsg(s, &ctl, NULL, 0) < 0) {
		perror("	**putmsg DL_DETACH_REQ");
		return;
	}
	expect(s, DL_OK_ACK);
}


void
dl_bind(s, sap, srv_mode, max_conind, conn_mgmt, xidtest_flg)
	int		s;
	dlu32_t		sap;
	int		srv_mode;
	int		max_conind;
	int		conn_mgmt;
	int		xidtest_flg;
{
	struct strbuf	ctl;
	dl_bind_req_t	bind;

	ctl.len = DL_BIND_REQ_SIZE;
	ctl.buf = (char *) &bind;
	bind.dl_primitive = DL_BIND_REQ;
	bind.dl_sap = sap;
	bind.dl_max_conind = max_conind;
	switch (srv_mode) {
	case 1:
		bind.dl_service_mode = DL_CLDLS;
		break;
	case 2:
		bind.dl_service_mode = DL_CODLS;
		break;
	default:
		/* To test unsupported service mode */
		bind.dl_service_mode = DL_ACLDLS;
		break;
	}
	bind.dl_conn_mgmt = conn_mgmt;
	bind.dl_xidtest_flg = xidtest_flg;

	if (putmsg(s, &ctl, NULL, 0) < 0) {
		perror("	**putmsg DL_BIND_REQ");
		return;
	}
	expect(s, DL_BIND_ACK);
}


void
dl_unbind(s)
	int		s;
{
	struct strbuf	ctl;
	dl_unbind_req_t	unbind;

	ctl.len = DL_UNBIND_REQ_SIZE;
	ctl.buf = (char *) &unbind;
	unbind.dl_primitive = DL_UNBIND_REQ;

	if (putmsg(s, &ctl, NULL, 0) < 0) {
		perror("	**putmsg DL_UNBIND_REQ");
		return;
	}
	expect(s, DL_OK_ACK);
}

void
dl_subs_bind(s, dlsap, class)
	int		s;
	struct dlpi_addr *dlsap;
	int		class;
{
	struct strbuf	ctl;
	char		buf[DL_SUBS_BIND_REQ_SIZE + MAXADDRLEN];
	dl_subs_bind_req_t *subs_bind;

	ctl.len = DL_SUBS_BIND_REQ_SIZE + dlsap->length;
	ctl.buf = buf;
	subs_bind = (dl_subs_bind_req_t *) buf;
	subs_bind->dl_primitive = DL_SUBS_BIND_REQ;
	subs_bind->dl_subs_sap_length = dlsap->length;
	subs_bind->dl_subs_sap_offset = DL_SUBS_BIND_REQ_SIZE;
	subs_bind->dl_subs_bind_class = class;
	memcpy((char *) buf + DL_SUBS_BIND_REQ_SIZE, dlsap->addr,
		dlsap->length);

	if (putmsg(s, &ctl, NULL, 0) < 0) {
		perror("	**putmsg DL_SUBS_BIND_REQ");
		return;
	}

	expect(s, DL_SUBS_BIND_ACK);
}

void
dl_subs_unbind(s, dlsap)
	int		s;
	struct dlpi_addr *dlsap;
{
	struct strbuf	ctl;
	char		buf[DL_SUBS_UNBIND_REQ_SIZE + MAXADDRLEN];
	dl_subs_unbind_req_t *subs_unbind;

	ctl.len = DL_SUBS_UNBIND_REQ_SIZE + dlsap->length;
	ctl.buf = buf;
	subs_unbind = (dl_subs_unbind_req_t *) buf;
	subs_unbind->dl_primitive = DL_SUBS_UNBIND_REQ;
	subs_unbind->dl_subs_sap_length = dlsap->length;
	subs_unbind->dl_subs_sap_offset = DL_SUBS_UNBIND_REQ_SIZE;
	memcpy((char *) buf + DL_SUBS_UNBIND_REQ_SIZE, dlsap->addr,
		dlsap->length);

	if (putmsg(s, &ctl, NULL, 0) < 0) {
		perror("	**putmsg DL_SUBS_UNBIND_REQ");
		return;
	}

	expect(s, DL_OK_ACK);
}

void
dl_connect_req(s, dlsap, wait)
	int		s;
	struct dlpi_addr *dlsap;
	int		wait;
{
	struct strbuf	ctl;
	char		buf[DL_CONNECT_REQ_SIZE + MAXADDRLEN];
	dl_connect_req_t *connect;

	ctl.len = DL_CONNECT_REQ_SIZE + dlsap->length;
	ctl.buf = buf;
	connect = (dl_connect_req_t *) buf;
	connect->dl_primitive = DL_CONNECT_REQ;
	connect->dl_dest_addr_length = dlsap->length;
	connect->dl_dest_addr_offset = DL_CONNECT_REQ_SIZE;
	connect->dl_qos_length = 0;
	connect->dl_qos_offset = 0;
	memcpy((char *) buf + DL_CONNECT_REQ_SIZE, dlsap->addr, dlsap->length);

	if (putmsg(s, &ctl, NULL, 0) < 0) {
		perror("	**putmsg DL_CONNECT_REQ");
		return;
	}
	if (wait) {
		expect(s, DL_CONNECT_CON);
	}
}


void
dl_connect_ind(s, reply, var_index)
	int		s;
	int		reply;
	int		var_index;
{
	dl_connect_ind_t *conn_ind;

	if (expect(s, DL_CONNECT_IND) < 0) {
		return;
	}
	conn_ind = (dl_connect_ind_t *) resultbuf;
	if (reply) {
		printf("Sending DL_CONNECT_RES...\n");
		dl_connect_res(s, conn_ind->dl_correlation, 0);
	} else {
		variables[var_index].value = conn_ind->dl_correlation;
	}
}


void
dl_connect_res(s, correlation, token)
	int		s;
	dlu32_t		correlation;
	dlu32_t		token;
{
	struct strbuf	ctl;
	dl_connect_res_t conn_res;

	ctl.len = DL_CONNECT_RES_SIZE;
	ctl.buf = (char *) &conn_res;
	conn_res.dl_primitive = DL_CONNECT_RES;
	conn_res.dl_correlation = correlation;
	conn_res.dl_resp_token = token;
	conn_res.dl_qos_length = 0;
	conn_res.dl_qos_offset = 0;
	conn_res.dl_growth = 0;

	if (putmsg(s, &ctl, NULL, 0) < 0) {
		perror("	**putmsg DL_CONNECT_RES");
		return;
	}
	expect(s, DL_OK_ACK);
}


void
dl_connect_con(s)
	int		s;
{
	expect(s, DL_CONNECT_CON);
}


void
dl_token(s, var_index)
	int		s;
	int		var_index;
{
	struct strbuf	ctl;
	dl_token_req_t	token;
	dl_token_ack_t	*token_ack;

	ctl.len = DL_TOKEN_REQ_SIZE;
	ctl.buf = (char *) &token;
	token.dl_primitive = DL_TOKEN_REQ;

	if (putmsg(s, &ctl, NULL, 0) < 0) {
		perror("	**putmsg DL_TOKEN_REQ");
		return;
	}
	if (expect(s, DL_TOKEN_ACK) == 0) {
		token_ack = (dl_token_ack_t *) resultbuf;
		variables[var_index].value = token_ack->dl_token;
		printf("DL_TOKEN_ACK: dl_token=%x, len=%d\n",
			token_ack->dl_token,sizeof(token_ack->dl_token));
	}
}


void
dl_disconnect_req(s, correlation)
	int		s;
	int		correlation;
{
	struct strbuf	ctl;
	dl_disconnect_req_t disconnect;

	ctl.len = DL_DISCONNECT_REQ_SIZE;
	ctl.buf = (char *) &disconnect;
	disconnect.dl_primitive = DL_DISCONNECT_REQ;
	disconnect.dl_reason = 0;
	disconnect.dl_correlation = correlation;

	if (putmsg(s, &ctl, NULL, 0) < 0) {
		perror("	**putmsg DL_DISCONNECT_REQ");
		return;
	}
	expect(s, DL_OK_ACK);
}


void
dl_disconnect_ind(s, correlation)
	int		s;
	int		correlation;
{
	struct strbuf	ctl;
	dl_disconnect_ind_t *discon_ind;

	if (expect(s, DL_DISCONNECT_IND) == 0) {
		discon_ind = (dl_disconnect_ind_t *) resultbuf;
		if (discon_ind->dl_correlation == correlation) {
			printf("Received the expected correlation\n");
		} else {
			printf(
			    "*** ERROR: expected correlation 0x%x, got 0x%x\n",
				correlation, discon_ind->dl_correlation);
		}
	}
}


void
dl_reset_req(s, wait)
	int		s;
	int		wait;
{
	struct strbuf	ctl;
	dl_reset_req_t	reset;

	ctl.len = DL_RESET_REQ_SIZE;
	ctl.buf = (char *) &reset;
	reset.dl_primitive = DL_RESET_REQ;

	if (putmsg(s, &ctl, NULL, 0) < 0) {
		perror("	**putmsg DL_RESET_REQ");
		return;
	}
	if (wait) {
		expect(s, DL_RESET_CON);
	}
}


void
dl_reset_res(s)
	int		s;
{
	struct strbuf	ctl;
	dl_reset_res_t	reset_res;

	ctl.len = DL_RESET_RES_SIZE;
	ctl.buf = (char *) &reset_res;
	reset_res.dl_primitive = DL_RESET_RES;

	if (putmsg(s, &ctl, NULL, 0) < 0) {
		perror("	**putmsg DL_RESET_RES");
		return;
	}
	expect(s, DL_OK_ACK);
}


void
dl_send_data(s, buf, len)
	int		s;
	char		*buf;
	int		len;
{
	struct strbuf	data;

	data.len = len;
	data.buf = buf;

	if (putmsg(s, NULL, &data, 0) < 0) {
		perror("	**putmsg M_DATA");
		return;
	}
}


void
dl_recv_data(s, buf, len)
	int		s;
	char		*buf;
	int		len;
{
	struct strbuf	data;
	int		flags = 0;

	data.maxlen = DATABUF_LEN;
	data.buf = (char *) databuf;

	if (getmsg(s, NULL, &data, &flags) < 0) {
		perror("	**getmsg");
		return;
	}
	print_hex(databuf, data.len);
	if (len) {
		compare_data(buf, len, (char *) databuf, data.len);
	}
}


void
dl_send_udata(s, dlsap, buf, len)
	int		s;
	struct dlpi_addr *dlsap;
	char		*buf;
	int		len;
{
	struct strbuf	ctl;
	struct strbuf	data;
	char		ctlbuf[DL_UNITDATA_REQ_SIZE + MAXADDRLEN];
	dl_unitdata_req_t *data_req;

	ctl.len = DL_UNITDATA_REQ_SIZE + dlsap->length;
	ctl.buf = ctlbuf;
	data_req = (dl_unitdata_req_t *) ctlbuf;
	data_req->dl_primitive = DL_UNITDATA_REQ;
	data_req->dl_dest_addr_length = dlsap->length;
	/* NETWARE HACK: if (dlsap->dl_sap == 0xFF)
		data_req->dl_dest_addr_length--; */
	data_req->dl_dest_addr_offset = DL_UNITDATA_REQ_SIZE;
	data_req->dl_priority.dl_min = 0;
	data_req->dl_priority.dl_max = 0;
	memcpy((char *) ctlbuf + DL_UNITDATA_REQ_SIZE, dlsap->addr,
		dlsap->length);

	data.len = len;
	data.buf = buf;

	if (putmsg(s, &ctl, &data, 0) < 0) {
		perror("	**putmsg DL_UNITDATA_REQ");
		return;
	}
}


void
dl_recv_udata(s, buf, len)
	int		s;
	char		*buf;
	int		len;
{
	struct strbuf	ctl;
	struct strbuf	data;
	int		flags = 0;
	union DL_primitives *dlp;

	ctl.maxlen = MAXPRIMSZ;
	ctl.buf = resultbuf;
	data.maxlen = DATABUF_LEN;
	data.buf = (char *) databuf;

	if (getmsg(s, &ctl, &data, &flags) < 0) {
		perror("	**getmsg");
		return;
	}
	if (expected_prim == NULL_PRIM) {
		expected_prim = (dlu32_t)DL_UNITDATA_IND;
	}
	dlp = (union DL_primitives *) resultbuf;
	if (dlp->dl_primitive == expected_prim) {
		printf("received expected %s\n", dl_prim[expected_prim]);
	} else {
		printf("*** ERROR: Expected %s, got %s\n",
			dl_prim[expected_prim], dl_prim[dlp->dl_primitive]);
	}
	print_prim(dlp);
	if (data.len > 0) {
		print_hex(databuf, data.len);
	}
	if (len) {
		compare_data(buf, len, (char *) databuf, data.len);
	}
}


void
dl_test_req(s, dlsap, pf_bit, wait, datbuf, len)
	int		s;
	struct dlpi_addr *dlsap;
	int		pf_bit;
	int		wait;
	char		*datbuf;
	int		len;
{
	dl_test_xid_req(DL_TEST_REQ, DL_TEST_CON, s, dlsap, pf_bit, wait,
			datbuf, len);
}


void
dl_test_res(s, dlsap, pf_bit, datbuf, len)
	int		s;
	struct dlpi_addr *dlsap;
	int		pf_bit;
	char		*datbuf;
	int		len;
{
	dl_test_xid_req(DL_TEST_RES, 0, s, dlsap, pf_bit, 0, datbuf, len);
}


void
dl_xid_req(s, dlsap, pf_bit, wait, datbuf, len)
	int		s;
	struct dlpi_addr *dlsap;
	int		pf_bit;
	int		wait;
	char		*datbuf;
	int		len;
{
	dl_test_xid_req(DL_XID_REQ, DL_XID_CON, s, dlsap, pf_bit, wait,
			datbuf, len);
}


void
dl_xid_res(s, dlsap, pf_bit, datbuf, len)
	int		s;
	struct dlpi_addr *dlsap;
	int		pf_bit;
	char		*datbuf;
	int		len;
{
	dl_test_xid_req(DL_XID_RES, 0, s, dlsap, pf_bit, 0, datbuf, len);
}


void
dl_test_xid_req(prim, return_prim, s, dlsap, pf_bit, wait, datbuf, len)
	int		prim;
	int		return_prim;
	int		s;
	struct dlpi_addr *dlsap;
	int		pf_bit;
	int		wait;
	char		*datbuf;
	int		len;
{
	struct strbuf	ctl;
	struct strbuf	data;
	struct strbuf	*dataptr;
	char		buf[DL_TEST_REQ_SIZE + MAXADDRLEN];
	dl_test_req_t	*test;

	ctl.len = DL_TEST_REQ_SIZE + dlsap->length;
	ctl.buf = buf;
	if (len > 0) {
		data.len = len;
		data.buf = datbuf;
		dataptr = &data;
	} else {
		dataptr = NULL;
	}
	test = (dl_test_req_t *) buf;
	test->dl_primitive = prim;
	test->dl_flag = pf_bit;
	test->dl_dest_addr_length = dlsap->length;
	test->dl_dest_addr_offset = DL_TEST_REQ_SIZE;
	memcpy((char *) buf + DL_TEST_REQ_SIZE, dlsap->addr, dlsap->length);

	if (putmsg(s, &ctl, dataptr, 0) < 0) {
		perror("	**putmsg DL_TEST_REQ");
		return;
	}
	if (wait) {
		dl_test_xid_ind(return_prim, s, pf_bit);
	}
}


void
dl_test_ind(s, expected_pf)
	int		s;
	int		expected_pf;
{
	dl_test_xid_ind(DL_TEST_IND, s, expected_pf);
}


void
dl_test_con(s, expected_pf)
	int		s;
	int		expected_pf;
{
	dl_test_xid_ind(DL_TEST_CON, s, expected_pf);
}


void
dl_xid_ind(s, expected_pf)
	int		s;
	int		expected_pf;
{
	dl_test_xid_ind(DL_XID_IND, s, expected_pf);
}


void
dl_xid_con(s, expected_pf)
	int		s;
	int		expected_pf;
{
	dl_test_xid_ind(DL_XID_CON, s, expected_pf);
}


void
dl_test_xid_ind(prim, s, expected_pf)
	int		s;
	int		expected_pf;
{
	struct strbuf	ctl;
	struct strbuf	data;
	int		flags = 0;
	union DL_primitives *dlp;
	dl_test_ind_t	*test_ind;

	ctl.maxlen = MAXPRIMSZ;
	ctl.buf = resultbuf;
	data.maxlen = DATABUF_LEN;
	data.buf = (char *) databuf;

	if (getmsg(s, &ctl, &data, &flags) < 0) {
		perror("	**getmsg");
		return;
	}
	if (expected_prim == NULL_PRIM) {
		expected_prim = prim;
	}
	dlp = (union DL_primitives *) resultbuf;
	if (dlp->dl_primitive == expected_prim) {
		printf("received expected %s\n", dl_prim[expected_prim]);
	} else {
		printf("*** ERROR: Expected %s, got %s\n",
			dl_prim[expected_prim], dl_prim[dlp->dl_primitive]);
	}
	print_prim(dlp);

	switch (dlp->dl_primitive) {
	case DL_TEST_IND:
	case DL_TEST_CON:
	case DL_XID_IND:
	case DL_XID_CON:
		test_ind = (dl_test_ind_t *) resultbuf;
		if (test_ind->dl_flag == expected_pf) {
			printf("Received the expected P/F bit\n");
		} else {
			printf("*** ERROR: expected P/F bit 0x%x, got 0x%x\n",
				expected_pf, test_ind->dl_flag);
		}
		if (data.len > 0) {
			print_hex(databuf, data.len);
		}
		break;
	default:
		break;
	}
}

void
dl_enabmulti_req(s, dlsap)
	int		s;
	struct dlpi_addr *dlsap;
{
	dl_enab_disab_multi(DL_ENABMULTI_REQ, s, dlsap);
}


void
dl_disabmulti_req(s, dlsap)
	int		s;
	struct dlpi_addr *dlsap;
{
	dl_enab_disab_multi(DL_DISABMULTI_REQ, s, dlsap);
}


void
dl_enab_disab_multi(prim, s, dlsap)
	int		prim;
	int		s;
	struct dlpi_addr *dlsap;
{
	struct strbuf	ctl;
	char		buf[DL_ENABMULTI_REQ_SIZE + MAXADDRLEN];
	dl_enabmulti_req_t *req;

	ctl.len = DL_ENABMULTI_REQ_SIZE + dlsap->length;
	ctl.buf = buf;
	req = (dl_enabmulti_req_t *) buf;
	req->dl_primitive = prim;
	req->dl_addr_length = dlsap->length;
	req->dl_addr_offset = DL_ENABMULTI_REQ_SIZE;
	memcpy((char *) buf + DL_ENABMULTI_REQ_SIZE, dlsap->addr,
		dlsap->length);

	if (putmsg(s, &ctl, NULL, 0) < 0) {
		perror("	**putmsg DL_ENABMULTI_REQ");
		return;
	}
	expect(s, DL_OK_ACK);
}

void
dl_get_statistics(s)
	int		s;
{
	struct strbuf	ctl;
	dl_get_statistics_req_t	get_stats;

	ctl.len = DL_GET_STATISTICS_REQ_SIZE;
	ctl.buf = (char *) &get_stats;
	get_stats.dl_primitive = DL_GET_STATISTICS_REQ;

	if (putmsg(s, &ctl, NULL, 0) < 0) {
		perror("	**putmsg DL_GET_STATISTICS_REQ");
		return;
	}
	expect(s, DL_GET_STATISTICS_ACK);
}

void
dl_phys_addr(s, type)
	int		s;
	int		type;
{
	struct strbuf	ctl;
	dl_phys_addr_req_t phys_addr;

	ctl.len = DL_PHYS_ADDR_REQ_SIZE;
	ctl.buf = (char *) &phys_addr;
	phys_addr.dl_primitive = DL_PHYS_ADDR_REQ;
	phys_addr.dl_addr_type = type;

	if (putmsg(s, &ctl, NULL, 0) < 0) {
		perror("	**putmsg DL_PHYS_ADDR_REQ");
		return;
	}
	expect(s, DL_PHYS_ADDR_ACK);
}

void
dl_set_phys_addr(s, dlsap)
	int		s;
	struct dlpi_addr *dlsap;
{
	struct strbuf	ctl;
	char		buf[DL_SET_PHYS_ADDR_REQ_SIZE + MAXADDRLEN];
	dl_set_phys_addr_req_t *req;

	ctl.len = DL_SET_PHYS_ADDR_REQ_SIZE + dlsap->length;
	ctl.buf = buf;
	req = (dl_set_phys_addr_req_t *) buf;
	req->dl_primitive = DL_SET_PHYS_ADDR_REQ;
	req->dl_addr_length = dlsap->length;
	req->dl_addr_offset = DL_SET_PHYS_ADDR_REQ_SIZE;
	memcpy((char *) buf + DL_SET_PHYS_ADDR_REQ_SIZE, dlsap->addr,
		dlsap->length);

	if (putmsg(s, &ctl, NULL, 0) < 0) {
		perror("	**putmsg DL_SET_PHYS_ADDR_REQ");
		return;
	}
	expect(s, DL_OK_ACK);
}

void
compare_data(expected_buf, expected_len, buf, len)
	char		*expected_buf;
	int		expected_len;
	char		*buf;
	int		len;
{
	int		i;

	if (len != expected_len) {
		printf("*** ERROR: Expected %d bytes, got %d\n",
			expected_len, len);
		return;
	}
	for (i = 0; i < len; i++) {
		if (buf[i] != expected_buf[i]) {
			printf("*** ERROR: byte #%d is incorrect: ", i+1);
			printf("expected 0x%x, got 0x%x\n",
				expected_buf[i], buf[i]);
			return;
		}
	}
	printf("Received expected data\n");
}


int
expect(str, prim)
	int		str;
	dlu32_t		prim;
{
	struct strbuf	ctl;
	union DL_primitives *dlp = (union DL_primitives *) resultbuf;
	struct strbuf	data;
	int		sz;
	int		flags = 0;
	int		retval = 0;
	dlu32_t		exp_prim;

	ctl.maxlen = MAXPRIMSZ;
	ctl.buf = resultbuf;
	data.maxlen = DATABUF_LEN;
	data.buf = (char *) databuf;

	if (getmsg(str, &ctl, &data, &flags) < 0) {
		perror("	**getmsg");
		return -1;
	}
	if (data.len > 0) {
		printf(
		 "*** ERROR: Not expecting M_DATA, received %d bytes of data\n",
			data.len);
		return -1;
	}
	if (expected_prim != NULL_PRIM)
		exp_prim = expected_prim;
	else
		exp_prim = prim;
	DispHex(resultbuf, ctl.len);

	if (dlp->dl_primitive != exp_prim) {
		if (dlp->dl_primitive > DL_MAXPRIM) {
			printf("*** ERROR: expect: bad incoming prim id 0x%x\n",
				dlp->dl_primitive);
			return -1;
		}
		retval = -1;
		printf("*** ERROR: Expected %s, got %s\n", dl_prim[prim],
			dl_prim[dlp->dl_primitive]);
	} else {
		if (expected_error != NULL_ERROR && exp_prim == DL_ERROR_ACK) {
			if (dlp->error_ack.dl_errno == expected_error) {
				printf("received expected %s\n", dl_prim[prim]);
			} else {
				retval = -1;
				printf("*** ERROR: Expected error %s\n",
					dl_errors[expected_error]);
				printf("                      got %s\n",
					dl_errors[dlp->error_ack.dl_errno]);
			}
		} else if (expected_error != NULL_ERROR && 
				exp_prim == DL_UDERROR_IND) {
			if (dlp->uderror_ind.dl_errno == expected_error) {
				printf("received expected %s\n", dl_prim[prim]);
			} else {
				retval = -1;
				printf("*** ERROR: Expected error %s\n",
					dl_errors[expected_error]);
				printf("                      got %s\n",
					dl_errors[dlp->uderror_ind.dl_errno]);
			}
		} else {
			printf("received expected %s\n", dl_prim[prim]);
		}
	}
	print_prim(dlp);
	return retval;
}


void
print_prim(dlp)
	union DL_primitives *dlp;
{
	switch (dlp->dl_primitive) {

	case DL_OK_ACK:
		printf("	correct_primitive %s\n",
			dl_prim[dlp->ok_ack.dl_correct_primitive]);
		break;

	case DL_ERROR_ACK:
		printf("	error_primitive %s\n",
			dl_prim[dlp->error_ack.dl_error_primitive]);
		printf("	errno is      %s\n",
			dl_errors[dlp->error_ack.dl_errno]);
		printf("	unix_errno is %s\n", 
			sys_errlist[dlp->error_ack.dl_unix_errno]);
		break;

	case DL_INFO_ACK:
		printf("	current_state is %s\n",
			dl_states[dlp->info_ack.dl_current_state]);
		printf("	dl_mac_type is %s\n",
			dl_media[dlp->info_ack.dl_mac_type]);
		printf("	dl_sap_length %d\n",
			dlp->info_ack.dl_sap_length);
		printf("	dl_service_mode is %s\n",
			dl_modes[dlp->info_ack.dl_service_mode]);
		printf("	dl_min_sdu %d\n", dlp->info_ack.dl_min_sdu);
		printf("	dl_max_sdu %d\n", dlp->info_ack.dl_max_sdu);
		if (dlp->info_ack.dl_addr_length) {
			printf("	dl_address ");
			print_dlsap((u_char *) dlp +
				dlp->info_ack.dl_addr_offset,
				dlp->info_ack.dl_addr_length);
			printf("\n");
		}
		if (dlp->info_ack.dl_brdcst_addr_length) {
			printf("	dl_brdcst_address ");
			print_dlsap((u_char *) dlp +
				dlp->info_ack.dl_brdcst_addr_offset,
				dlp->info_ack.dl_brdcst_addr_length);
			printf("\n");
		}
		break;

	case DL_BIND_ACK:

		printf("	dl_sap assigned is 0x%04x\n", 
					dlp->bind_ack.dl_sap);
		printf("	dl_addr_length is %d\n", 
					dlp->bind_ack.dl_addr_length);
		printf("	dl_addr_offset is %d\n", 
					dlp->bind_ack.dl_addr_offset);
		printf("	dl_max_conind is %d\n", 
					dlp->bind_ack.dl_max_conind);

		printf("	addr = ");
		print_dlsap((u_char *) dlp +
			dlp->bind_ack.dl_addr_offset,
			dlp->bind_ack.dl_addr_length);
		printf("\n");
		break;

	case DL_SUBS_BIND_ACK:

		printf("	dl_subs_sap ");
		print_dlsap((u_char *) dlp +
			 dlp->subs_bind_ack.dl_subs_sap_offset,
			 dlp->subs_bind_ack.dl_subs_sap_length);
		printf("\n");
		break;

	case DL_CONNECT_IND:
		printf("	dl_correlation 0x%x\n",
					dlp->connect_ind.dl_correlation);
		printf("	dl_called_addr ");
		print_dlsap((u_char *) dlp +
			 dlp->connect_ind.dl_called_addr_offset,
			 dlp->connect_ind.dl_called_addr_length);
		printf("\n");
		printf("	dl_calling_addr ");
		print_dlsap((u_char *) dlp +
			 dlp->connect_ind.dl_calling_addr_offset,
			 dlp->connect_ind.dl_calling_addr_length);
		printf("\n");
		break;

	case DL_CONNECT_CON:
		printf("	dl_resp_addr ");
		print_dlsap((u_char *) dlp +
			 dlp->connect_con.dl_resp_addr_offset,
			 dlp->connect_con.dl_resp_addr_length);
		printf("\n");
		break;

	case DL_TOKEN_ACK:
		printf("	dl_token 0x%x\n", (dlu32_t)(dlp->token_ack.dl_token));
		break;

	case DL_DISCONNECT_IND:

		switch (dlp->disconnect_ind.dl_originator) {

		case DL_USER:
			printf("	dl_originator is DL_USER\n");
			break;

		case DL_PROVIDER:
			printf("	dl_originator is DL_PROVIDER\n");
			break;
		default:
			printf("	dl_originator is invalid (0x%x)\n",
				dlp->disconnect_ind.dl_originator);
		}
		dlp->disconnect_ind.dl_reason &= 0xff;	/* yuck */
		printf("	dl_reason is %s\n", 
			disc_reasons[dlp->disconnect_ind.dl_reason]);
		printf("	dl_correlation 0x%x\n",
			dlp->disconnect_ind.dl_correlation);
		break;

	case DL_RESET_IND:
		switch (dlp->reset_ind.dl_originator) {

		case DL_USER:
			printf("	dl_originator is DL_USER\n");
			break;

		case DL_PROVIDER:
			printf("	dl_originator is DL_PROVIDER\n");
			break;
		default:
			printf("	dl_originator is invalid (0x%x)\n",
				dlp->reset_ind.dl_originator);
		}
		dlp->reset_ind.dl_reason &= 0xff;	/* yuck */
		printf("	dl_reason is %s\n", 
			reset_reasons[dlp->reset_ind.dl_reason]);
		break;

	case DL_UNITDATA_IND:
		printf("	dl_dest_addr ");
		print_dlsap((u_char *) dlp +
			 dlp->unitdata_ind.dl_dest_addr_offset,
			 dlp->unitdata_ind.dl_dest_addr_length);
		printf("\n");
		printf("	dl_src_addr ");
		print_dlsap((u_char *) dlp +
			 dlp->unitdata_ind.dl_src_addr_offset,
			 dlp->unitdata_ind.dl_src_addr_length);
		printf("\n");
		printf("	dl_group_address %d\n",
			dlp->unitdata_ind.dl_group_address);
		break;

	case DL_UDERROR_IND:
		printf("	dl_dest_addr ");
		print_dlsap((u_char *) dlp +
			 dlp->uderror_ind.dl_dest_addr_offset,
			 dlp->uderror_ind.dl_dest_addr_length);
		printf("\n");
		printf("	errno is      %s\n",
			dl_errors[dlp->uderror_ind.dl_errno]);
		printf("	unix_errno is %s\n",
			sys_errlist[dlp->uderror_ind.dl_unix_errno]);
		break;

	case DL_TEST_IND:
	case DL_TEST_CON:
	case DL_XID_IND:
	case DL_XID_CON:
		printf("	dl_flag %d\n", dlp->test_ind.dl_flag);
		printf("	dl_dest_addr ");
		print_dlsap((u_char *) dlp +
			 dlp->test_ind.dl_dest_addr_offset,
			 dlp->test_ind.dl_dest_addr_length);
		printf("\n");
		printf("	dl_src_addr ");
		print_dlsap((u_char *) dlp +
			 dlp->test_ind.dl_src_addr_offset,
			 dlp->test_ind.dl_src_addr_length);
		printf("\n");
		break;

	case DL_GET_STATISTICS_ACK:
#ifdef LLC2
	{
		struct llc2_stats stats;
		dlu32_t length = dlp->get_statistics_ack.dl_stat_length;
		
		if (length != sizeof (struct llc2_stats))
		{
			printf("*** ERROR: Expected stats size %d, got %d\n",
				sizeof (struct llc2_stats), length);
			break;
		}

		/* ensure the data is aligned */
		memcpy((char *) &stats,
			(char *) dlp + dlp->get_statistics_ack.dl_stat_offset,
			length);

		printf("   ----------------------------------------------------------------------\n");
		printf("  |          |        RECEIVE        |              TRANSMIT             |\n");
		printf("  |  TYPE    |-----------------------|-----------------------------------|\n");
		printf("  |          |  COMMAND  |  RESPONSE |  COMMAND  |  RESPONSE | CMD P-BIT |\n");
		printf("  |----------|-----------|-----------|-----------|-----------|-----------|\n");
		printf("  | RR       |%10d |%10d |%10d |%10d |%10d |\n",
			stats.llc2monarray[RR_rx_cmd],
			stats.llc2monarray[RR_rx_rsp],
			stats.llc2monarray[RR_tx_cmd],
			stats.llc2monarray[RR_tx_rsp],
			stats.llc2monarray[RR_tx_cmd_p]);

		printf("  | RNR      |%10d |%10d |%10d |%10d |%10d |\n",
			stats.llc2monarray[RNR_rx_cmd],
			stats.llc2monarray[RNR_rx_rsp],
			stats.llc2monarray[RNR_tx_cmd],
			stats.llc2monarray[RNR_tx_rsp],
			stats.llc2monarray[RNR_tx_cmd_p]);

		printf("  | REJ      |%10d |%10d |%10d |%10d |%10d |\n",
			stats.llc2monarray[REJ_rx_cmd],
			stats.llc2monarray[REJ_rx_rsp],
			stats.llc2monarray[REJ_tx_cmd],
			stats.llc2monarray[REJ_tx_rsp],
			stats.llc2monarray[REJ_tx_cmd_p]);

		printf("  | SABME    |%10d |           |%10d |           |           |\n",
			stats.llc2monarray[SABME_rx_cmd],
			stats.llc2monarray[SABME_tx_cmd]);

		printf("  | DISC     |%10d |           |%10d |           |           |\n",
			stats.llc2monarray[DISC_rx_cmd],
			stats.llc2monarray[DISC_tx_cmd]);

		printf("  | UA       |           |%10d |           |%10d |           |\n",
			stats.llc2monarray[UA_rx_rsp],
			stats.llc2monarray[UA_tx_rsp]);
		printf("  | DM       |           |%10d |           |%10d |           |\n",
			stats.llc2monarray[DM_rx_rsp],
			stats.llc2monarray[DM_tx_rsp]);
		printf("  | I        |%10d |%10d |%10d |%10d |           |\n",
			stats.llc2monarray[I_rx_cmd],
			stats.llc2monarray[I_rx_rsp],
			stats.llc2monarray[I_tx_cmd],
			stats.llc2monarray[I_tx_rsp]);
		printf("  | FRMR     |           |%10d |           |%10d |           |\n",
			stats.llc2monarray[FRMR_rx_rsp],
			stats.llc2monarray[FRMR_tx_rsp]);
		printf("  | UI       |%10d |           |%10d |           |           |\n",
			stats.llc2monarray[UI_rx_cmd],
			stats.llc2monarray[UI_tx_cmd]);
		printf("  | XID      |%10d |%10d |%10d |%10d |           |\n",
			stats.llc2monarray[XID_rx_cmd],
			stats.llc2monarray[XID_rx_rsp],
			stats.llc2monarray[XID_tx_cmd],
			stats.llc2monarray[XID_tx_rsp]);
		printf("  | TEST     |%10d |%10d |%10d |%10d |           |\n",
			stats.llc2monarray[TEST_rx_cmd],
			stats.llc2monarray[TEST_rx_rsp],
			stats.llc2monarray[TEST_tx_cmd],
			stats.llc2monarray[TEST_tx_rsp]);
		printf("   ----------------------------------------------------------------------\n");
	}
#else /* LLC2 */
		print_hex((char *) dlp + dlp->get_statistics_ack.dl_stat_offset,
			dlp->get_statistics_ack.dl_stat_length);
#endif /* LLC2 */
		break;

	case DL_PHYS_ADDR_ACK:
		printf("	dl_addr ");
		print_dlsap((u_char *) dlp +
			 dlp->physaddr_ack.dl_addr_offset,
			 dlp->physaddr_ack.dl_addr_length);
		printf("\n");
		break;

	}
}


void
print_dlsap(addr, length)
unsigned char *addr;
dlu32_t length;
{
	int		i;

	for (i = 0; i < length; i++) {
		printf("%x", addr[i]);
		if (i == length - 1)
			break;
		if (i < DEFHWLEN - 1)
			printf(":");
		else
			printf(",");
	}
}


void
print_hex(p, length)
	u_char		*p;
	int		length;
{
	int		i;
	int		j;

	j = length;
	i = 0;
	while (j-- > 0) {
		i++;
		(void) printf(" %02x", *p++);
		if (i == 8) {
			printf("   ");
		} else if (i == 16) {
			i = 0;
			(void) printf("\n");
		}
	}
	if (i != 0)
		(void) printf("\n\n");
	else
		(void) printf("\n");
}
