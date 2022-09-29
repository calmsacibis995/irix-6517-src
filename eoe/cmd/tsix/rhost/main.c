/* Silicon Graphics
 */

#ident "$Revision: 1.4 $"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <syslog.h>
#include <assert.h>
#include <pwd.h>
#include <grp.h>

#include <sys/types.h>
#include <sys/sema.h>		/* required by t6rhdb.h */
#include <sys/mac_label.h>
#include <sys/mac.h>
#include <sys/capability.h>
#include <sys/t6attrs.h>
#include <sys/t6rhdb.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define DEFINE
#include "conf.h"
#include "keyword.h"
#undef DEFINE

#define RHDB_SIZE	8192
#define ROUNDUP(X)      ((X + 0x03)&~0x03)

static int verbose = 0;

static int
down_load (void)
{
	char *bp;		/* Base pointer */
	char *dp;		/* Data Pointer - next byte to be written */

	htbl_t *hp;
	t6rhdb_host_buf_t *pb;
	int size;
	int * checksum;

	rhost_t * pp;
	rhost_t * parms;

	/* Allocate buffer */
	bp = malloc(RHDB_SIZE);
	if (bp == NULL) {
		printf("Failed to allocate buffer\n");
		exit(1);
	}

	if (verbose)
		printf("Downloading Remote Host Database\n\n");

	/* Download entry */
	pb = (t6rhdb_host_buf_t *)bp;
	for (pp = rhdb_head; pp != NULL; pp = pp->rh_next_profile) {
		cap_t ocap;
		const cap_value_t cap_network_mgt = CAP_NETWORK_MGT;

		dp = bp;

		if (pp->rh_entry_type & T6RHDB_ENTRY_DEFAULT)
			continue;

		if (pp->rh_entry_type & T6RHDB_ENTRY_ERROR) {
			printf("Skipping host due to error: %s\n",
			    pp->rh_host_list->hostname);
			fflush(stdout);
		}

		/* Init entry */
		memset((void *) pb, '\0', RHDB_SIZE);
		if (pp->rh_default_profile == NULL)
			parms = pp;
		else
			parms = pp->rh_default_profile;
		
		/*
		 * Copy the whole buffer out.  We'll overwrite the
		 * the host count later.
		 */

		memcpy((void *) pb, (void *) &parms->rh_buf, sizeof(*pb));
		dp += sizeof(*pb);

		/* Add Host Addresses */
		for (hp = pp->rh_host_list; hp != NULL; hp = hp->related) {
			/* Valid entry ? */
			if (hp->flags & H_ADDR_UNKNOWN) {
				printf("Skipping %s: unknown address\n",
				    hp->hostname);
				fflush(stdout);
				continue;
			}

			/* Load Hosts */
			pb->hp_host_cnt++;
			((struct in_addr *)dp)->s_addr = hp->addr.s_addr;
			dp += sizeof(struct in_addr);
		}

		assert(((uintptr_t)dp % 4) == 0);
		size = (int)((uintptr_t)dp - (uintptr_t)bp) + sizeof(int);

		checksum  = (int *)dp;
		*checksum = size;

		ocap = cap_acquire (1, &cap_network_mgt);
		if (t6rhdb_put_host(size, bp) != 0) {
			perror("t6rhdb_put_host");
			printf("Download failed\n");
		}
		cap_surrender(ocap);
	}

	return 0;
}

static char *
smm_type (int smm)
{
	switch (smm) {
		case T6RHDB_SMM_INVALID:
			return "SMM_INVALID";
		case T6RHDB_SMM_SINGLE_LEVEL:
			return "SMM_SINGLE_LEVEL";
		case T6RHDB_SMM_MSIX_1_0:
			return "SMM_MSIX_1_0";
		case T6RHDB_SMM_MSIX_2_0:
			return "SMM_MSIX_2_0";
		case T6RHDB_SMM_TSIX_1_0:
			return "SMM_TSIX_1_0";
		case T6RHDB_SMM_TSIX_1_1:
			return "SMM_TSIX_1_1";
	}
	return "Unknown SMM type";
}

static char *
nlm_type (int nlm)
{
	switch (nlm) {
		case T6RHDB_NLM_INVALID:
			return "NLM_INVALID";
		case T6RHDB_NLM_UNLABELED:
			return "NLM_UNLABELED";
		case T6RHDB_NLM_CIPSO:
			return "NLM_CIPSO";
		case T6RHDB_NLM_CIPSO_TT1:
			return "NLM_CIPSO_TT1";
		case T6RHDB_NLM_CIPSO_TT2:
			return "NLM_CIPSO_TT2";
		case T6RHDB_NLM_RIPSO_BSO:
			return "NLM_RIPSO_BSO";
		case T6RHDB_NLM_RIPSO_BSOT:
			return "NLM_RIPSO_BSO_TX";
		case T6RHDB_NLM_RIPSO_BSOR:
			return "NLM_RIPSO_BSO_RX";
		case T6RHDB_NLM_RIPSO_ESO:
			return "NLM_RIPSO_ESO";
		case T6RHDB_NLM_SGIPSO:
			return "NLM_SGIPSO";
		case T6RHDB_NLM_SGIPSONOUID:
			return "NLM_SGIPSO_NOUID";
		case T6RHDB_NLM_SGIPSO_SPCL:
			return "NLM_SGIPSO_SPCL";
		case T6RHDB_NLM_SGIPSO_LOOP:
			return "NLM_SGIPSO_LOOP";
	}
	return "Unknown NLM type";
}

static void
print_flags (int flags)
{
	int i;

	printf ("Flags: ");
	for (i = T6RHDB_FLG_INVALID; i < T6RHDB_FLG_MAND_CLEARANCE; i++) {
		if ((T6RHDB_MASK(i) & flags) == 0)
			continue;

		switch (i) {
			case T6RHDB_FLG_INVALID:
				printf ("INVALID ");
				break;
			case T6RHDB_FLG_IMPORT:
				printf ("IMPORT ");
				break;
			case T6RHDB_FLG_EXPORT:
				printf ("EXPORT ");
				break;
			case T6RHDB_FLG_DENY_ACCESS:
				printf ("DENY_ACCESS ");
				break;
			case T6RHDB_FLG_MAND_SL:
				printf ("MAND_SL ");
				break;
			case T6RHDB_FLG_MAND_INTEG:
				printf ("MAND_INTEG ");
				break;
			case T6RHDB_FLG_MAND_ILB:
				printf ("MAND_ILB ");
				break;
			case T6RHDB_FLG_MAND_PRIVS:
				printf ("MAND_PRIVS ");
				break;
			case T6RHDB_FLG_MAND_LUID:
				printf ("MAND_LUID ");
				break;
			case T6RHDB_FLG_MAND_IDS:
				printf ("MAND_IDS ");
				break;
			case T6RHDB_FLG_MAND_SID:
				printf ("MAND_SID ");
				break;
			case T6RHDB_FLG_MAND_PID:
				printf ("MAND_PID ");
				break;
			case T6RHDB_FLG_MAND_CLEARANCE:
				printf ("MAND_CLEARANCE ");
				break;
			case T6RHDB_FLG_TRC_RCVPKT:
				printf ("TRACE_RECEIVE_PACKET ");
				break;
			case T6RHDB_FLG_TRC_XMTPKT:
				printf ("TRACE_TRANSMIT_PACKET ");
				break;
			case T6RHDB_FLG_TRC_RCVATT:
				printf ("TRACE_RECEIVE_ATTRIB ");
				break;
			case T6RHDB_FLG_TRC_XMTATT:
				printf ("TRACE_TRANSMIT_ATTRIB ");
				break;
		}
	}
	printf ("\n");
}

static void
lookup_host (char *host_name)
{

	struct in_addr addr;
	struct hostent *hostent = 0;
	t6rhdb_host_buf_t hbuf;

	/* Lookup IP Address */
	if ((addr.s_addr = inet_addr(host_name)) == (unsigned long) -1) {
		hostent = gethostbyname(host_name);
		if (hostent) {
			memcpy((void *) &addr.s_addr, (void *) hostent->h_addr,
				hostent->h_length);
		} else {
			printf("Host address unknown: %s\n",host_name);
			return;
		}
	}

	memset((void *) &hbuf, '\0', sizeof(hbuf));
	if (t6rhdb_get_host(&addr, sizeof(hbuf), (caddr_t) &hbuf) == -1) {
		printf("Host %s not in RHDB\n", host_name);
	} else {
		char *s;
		char *NA = "Not Available";
		int i;
		struct passwd *pwd;
		struct group *grp;

		printf("Host %s found in RHDB\n",host_name);
		printf ("SMM Type: %s\n", smm_type(hbuf.hp_smm_type));
		printf ("NLM Type: %s\n", nlm_type(hbuf.hp_nlm_type));
		printf ("AUTH Type: %d\n", hbuf.hp_auth_type);
		printf ("ENCRYPT Type: %d\n", hbuf.hp_encrypt_type);
		printf ("Attributes: %d\n", hbuf.hp_attributes);
		print_flags(hbuf.hp_flags);
		s = cap_to_text(&hbuf.hp_max_priv, (size_t *) NULL);
		printf ("Maximum Privilege: %s\n", s ? s : NA);
		cap_free(s);
		s = msen_to_text (&hbuf.hp_def_sl, (size_t *) NULL);
		printf ("Default Sensitivity: %s\n", s ? s : NA);
		msen_free(s);
		s = msen_to_text (&hbuf.hp_min_sl, (size_t *) NULL);
		printf ("Minimum Sensitivity: %s\n", s ? s : NA);
		msen_free(s);
		s = msen_to_text (&hbuf.hp_max_sl, (size_t *) NULL);
		printf ("Maximum Sensitivity: %s\n", s ? s : NA);
		msen_free(s);
		s = mint_to_text (&hbuf.hp_def_integ, (size_t *) NULL);
		printf ("Default Integrity: %s\n", s ? s : NA);
		mint_free(s);
		s = mint_to_text (&hbuf.hp_min_integ, (size_t *) NULL);
		printf ("Minimum Integrity: %s\n", s ? s : NA);
		mint_free(s);
		s = mint_to_text (&hbuf.hp_max_integ, (size_t *) NULL);
		printf ("Maximum Integrity: %s\n", s ? s : NA);
		mint_free(s);
		s = msen_to_text(&hbuf.hp_def_clearance, (size_t *) NULL);
		printf ("Default Clearance: %s\n", s ? s : NA);
		msen_free(s);
		printf ("Default Session ID: %lu\n",
			(unsigned long) hbuf.hp_def_sid);
		pwd = getpwuid(hbuf.hp_def_uid);
		printf ("Default User ID: %s\n", pwd ? pwd->pw_name : NA);
		pwd = getpwuid(hbuf.hp_def_luid);
		printf ("Default Audit ID: %s\n", pwd ? pwd->pw_name : NA);
		grp = getgrgid(hbuf.hp_def_gid);
		printf ("Default Group ID: %s\n", grp ? grp->gr_name : NA);
		printf ("Default Group Cnt: %d\n", hbuf.hp_def_grp_cnt);
		printf ("Default Group List: ");
		for (i = 0; i < hbuf.hp_def_grp_cnt; i++) {
			grp = getgrgid(hbuf.hp_def_groups[i]);
			if (grp)
				printf ("%s ", grp->gr_name);
			else
				printf ("%ld ", (unsigned long) hbuf.hp_def_groups[i]);
		}
		printf ("\n");
		s = cap_to_text(&hbuf.hp_def_priv, (size_t *) NULL);
		printf ("Default Privilege: %s\n", s ? s : NA);
		cap_free(s);
	}
}

static void
flush_host (char *host)
{
	struct in_addr addr;
	struct hostent *hostent;
	cap_t ocap;
	const cap_value_t cap_network_mgt = CAP_NETWORK_MGT;

	if (verbose)
		printf("flushing %s\n", host);

	/* Lookup IP Address */
	addr.s_addr = inet_addr(host);
	if (addr.s_addr == -1) {
		hostent = gethostbyname(host);
		if (hostent) {
			memcpy((void *) &addr.s_addr, (void *) hostent->h_addr,
			       hostent->h_length);
		} else {
			printf("Host address unknown: %s\n", host);
			return;
		}
	}

	ocap = cap_acquire (1, &cap_network_mgt);
	if (t6rhdb_flush(&addr) == -1)
		perror(host);
	cap_surrender(ocap);
}

static void
usage (void)
{
	(void)fputs("usage: rhost [-d] [-f cfile] [-r remote]\n"
		    "       rhost flush host1 ... hostn\n"
		    "       rhost -l hostname\n", stderr);
	exit(1);
}

void
main (int argc, char **argv)
{
	int i, lookup_flag = 0, check_only = 0;
	char *cfilename = "/etc/rhost.conf";

	/* check for appropriate environment */
	if (sysconf(_SC_MAC) <= 0) {
		fprintf(stderr, "%s MAC not enabled.\n", argv[0]);
		exit(1);
	}

	/* check for appropriate privilege */
	if (cap_envl(0, CAP_NETWORK_MGT, CAP_NOT_A_CID) == -1) {
		fprintf(stderr, "%s: insufficient privilege\n", argv[0]);
		exit(1);
	}

	openlog(argv[0], LOG_PID | LOG_ODELAY | LOG_NOWAIT, LOG_DAEMON);

	opterr = 0;
	while (i = getopt(argc,argv,"df:klnr:v"), i != EOF)
		switch (i) {
		case 'd':
			debug++;
			break;
		case 'k':
			(void)printf("keywords:\n");
			for (i = 0; i < KEYTBL_LEN-1; i++)
				(void)printf("\t%s\n",keytbl[i].str);
			exit(0);
			break;
		case 'l':
			lookup_flag++;
			break;
		case 'f':
			cfilename = optarg;
			break;
		case 'n':
			check_only++;
			break;
		case 'r':
			remote = optarg;
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage();
	}

	if (argc > optind) {
		/* Lookup command */
		if (lookup_flag) {
			for ( ; optind < argc; optind++)
				lookup_host(argv[optind]);
			exit(0);
		}

		/* Flush command */
		if (strcasecmp(argv[optind++], "flush") == 0) {
			if (optind == argc)
				usage();
			else {
				for (; optind < argc; optind++)
					flush_host(argv[optind]);
			}
			exit(0);
		}

		/* Unknown parameters */
		usage();
	}


	if (verbose || debug)
		printf("Parsing RHDB\n");
	if (parse_conf(cfilename)) {
		printf("Error occurred parsing config file\n");
		exit(1);
	} 

	if (check_only)
		printf("Config file checked only\n");
	else {
		if (verbose || debug)
			printf("Downloading RHDB\n");
		if (down_load()) {
			printf("Error occurred downloading RHDB\n");
			exit(1);
		}
	}

	exit(0);
}
