#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/mac.h>
#include <sys/mac_label.h>
#include <sys/capability.h>
#include <sys/t6attrs.h>
#include <sys/t6rhdb.h>

#include <grp.h>
#include <pwd.h>
#include <ctype.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "conf.h"
#include "keyword.h"

#ident "$Revision: 1.4 $"

/* Input file */
int cur_line = 0;
int llen = 0;
char lbuf[BUFSIZ];

/* Pointers used for parsing keywords */
char * lptr;
char * k;
char * v;
char * h;

/* Hash Lookup */
int hi,lo,i;
int tgt;

#define iseol(c)	(c == '\0' || c == '\n' || c == '#')
#define issep(c)	(strchr(sep, c) != NULL)
#define isignore(c)	(strchr(ignore, c) != NULL)

/* Table of Session Manager  Protocols*/
static struct {
	char *smm_name;
	int  smm_id;
} smm_tbl[] = {
	{"msix",	T6RHDB_SMM_MSIX_1_0},
	{"msix_1.0",	T6RHDB_SMM_MSIX_1_0},
	{"msix_2.0",	T6RHDB_SMM_MSIX_2_0},
	{"tsix",	T6RHDB_SMM_TSIX_1_0},
	{"tsix_1.0",	T6RHDB_SMM_TSIX_1_0},
	{"tsix_1.1",	T6RHDB_SMM_TSIX_1_1},
	{"none",	T6RHDB_SMM_SINGLE_LEVEL},
	{"single_level",T6RHDB_SMM_SINGLE_LEVEL},
	{"",		0},
};

/* Table of IP Security Label Protocols */
static struct {
	char *nlm_name;
	int  nlm_id;
} nlm_tbl[] = {
	{"cipso",	T6RHDB_NLM_CIPSO},
	{"cipso_tt1",	T6RHDB_NLM_CIPSO_TT1},
	{"cipso_tt2",	T6RHDB_NLM_CIPSO_TT2},
	{"ripso_bso",	T6RHDB_NLM_RIPSO_BSO},
	{"ripso_bso_tx",T6RHDB_NLM_RIPSO_BSOT},
	{"ripso_bso_rx",T6RHDB_NLM_RIPSO_BSOR},
	{"ripso_eso",	T6RHDB_NLM_RIPSO_ESO},
	{"sgipso",	T6RHDB_NLM_SGIPSO},
	{"sgipso_nouid",T6RHDB_NLM_SGIPSONOUID},
	{"sgipso_spcl",	T6RHDB_NLM_SGIPSO_SPCL},
	{"sgipso_loop",	T6RHDB_NLM_SGIPSO_LOOP},
	{"none",	T6RHDB_NLM_INVALID},
	{"unlabeled",	T6RHDB_NLM_UNLABELED},
	{"",		0},
};

/* Table of Security Atrribute Flags */
static struct {
	char *flag_name;
	int flag_id;
} flag_tbl[] = {
	{"import",	T6RHDB_FLG_IMPORT},
	{"export",	T6RHDB_FLG_EXPORT},
	{"deny_access",	T6RHDB_FLG_DENY_ACCESS},
	{"mand_sl",	T6RHDB_FLG_MAND_SL},
	{"mand_integ",	T6RHDB_FLG_MAND_INTEG},
	{"mand_ilb",	T6RHDB_FLG_MAND_ILB},
	{"mand_privs",	T6RHDB_FLG_MAND_PRIVS},
	{"mand_luid",	T6RHDB_FLG_MAND_LUID},
	{"mand_ids",	T6RHDB_FLG_MAND_IDS},
	{"mand_sid",	T6RHDB_FLG_MAND_SID},
	{"mand_pid",	T6RHDB_FLG_MAND_PID},
	{"mand_clearance", T6RHDB_FLG_MAND_CLEARANCE},
	{"trace_rcv_pkt", T6RHDB_FLG_TRC_RCVPKT},
	{"trace_xmt_pkt", T6RHDB_FLG_TRC_XMTPKT},
	{"trace_rcv_att", T6RHDB_FLG_TRC_RCVATT},
	{"trace_xmt_att", T6RHDB_FLG_TRC_XMTATT},
	{"",		0},
};

/* Table of known CMW vendors so we can be bug compatible if needed */
static struct {
	char *vendor_name;
	int vendor_id;
} vendor_tbl[] = {
	{"unknown",		T6RHDB_VENDOR_UNKNOWN},
	{"sun",			T6RHDB_VENDOR_SUN},
	{"hewlett-packard",	T6RHDB_VENDOR_HP},
	{"hp",			T6RHDB_VENDOR_HP},
	{"ibm",			T6RHDB_VENDOR_IBM},
	{"cray",		T6RHDB_VENDOR_CRAY},
	{"dg",			T6RHDB_VENDOR_DG},
	{"harris",		T6RHDB_VENDOR_HARRIS},
	{"",		0},
};

/*
 * Convert session manager name to constant by searching
 * serially though table until name is matched.
 */
static int
find_smm(const char *name)
{
	register int i;
	for (i = 0;smm_tbl[i].smm_name[0] != '\0'; i++)
		if (strcasecmp(name,smm_tbl[i].smm_name) == 0)
			return smm_tbl[i].smm_id;
	return T6RHDB_SMM_INVALID;
}

/*
 * Convert IPSO name to constant by searching
 * serially though table until name is matched.
 */
static int
find_nlm(const char *name)
{
	register int i;
	for (i = 0;nlm_tbl[i].nlm_name[0] != '\0'; i++)
		if (strcasecmp(name,nlm_tbl[i].nlm_name) == 0)
			return nlm_tbl[i].nlm_id;
	return T6RHDB_NLM_INVALID;
}

/*
 * Convert attribute name flag to constant by searching
 * serially though table until name is matched.
 */
static int
find_flag(const char *name)
{
	register int i;
	for (i = 0;flag_tbl[i].flag_name[0] != '\0'; i++)
		if (strcasecmp(name,flag_tbl[i].flag_name) == 0)
			return flag_tbl[i].flag_id;

	return T6RHDB_FLG_INVALID;
}

/*
 * Convert vendor name to constant by searching
 * serially though table until name is matched.
 */
static int
find_vendor(const char *name)
{
	register int i;
	for (i = 0;vendor_tbl[i].vendor_name[0] != '\0'; i++)
		if (strcasecmp(name,vendor_tbl[i].vendor_name) == 0)
			return vendor_tbl[i].vendor_id;

	return T6RHDB_VENDOR_INVALID;
}

/*
 * Convert user name to user id.
 */
static uid_t
get_uid(const char *str)
{
	struct passwd *p;

	/* Uid string is numeric */
	if (strspn(str,"0123456789") == strlen(str))
		return atoi(str);

	/* Lookup user name */
	p = getpwnam(str);
	endpwent();
	if (p != NULL)
		return p->pw_uid;

	return -1;
}

/*
 * Convert group name to group id.
 */
static gid_t
get_gid(const char *str)
{
	struct group *g;

	assert(str != NULL);
	
	/* Uid string is numeric */
	if (strspn(str,"0123456789") == strlen(str))
		return atoi(str);

	/* Lookup group name */
	g = getgrnam(str);
	endgrent();
	if (g != NULL)
		return g->gr_gid;

	return -1;
}

/*
 * Return a pointer to a new rhost_entry;
 */
static rhost_t *
new_host_entry(void)
{
	rhost_t * entry;

	if ((entry = (rhost_t *) malloc(sizeof(*entry))) == NULL) {
		perror("new_host_entry");
		return NULL;
	}
	memset((void *) entry, '\0', sizeof(*entry));

	if (rhdb_head == NULL) {
		assert(rhdb_tail == NULL);
		rhdb_head = entry;
		rhdb_tail = entry;
	} else {
		assert(rhdb_tail != NULL);
		rhdb_tail->rh_next_profile = entry;
		rhdb_tail = entry;	
	}

	if (debug) {
		printf("new_host_entry 0x%08x\n",entry);
		fflush(stdout);
	}

	return entry;
}

#ifdef DEBUG
static int
check_one_host(htbl_t * entry)
{
	int error = 0;

	if (entry->hostname == NULL || entry->profile == NULL ||
	    (entry->next == NULL && last_host != entry) ) {
		printf("host entry 0x%08x has Null Pointer\n",entry);
		printf("\thostname:   0x%08x\n",entry->hostname);
		printf("\tprofile:    0x%08x\n",entry->profile);
		printf("\tnext:       0x%08x\n",entry->next);
		printf("\trelated:    0x%08x\n",entry->related);
		printf("\tflags:      0x%08x\n",entry->flags);
		printf("\taddr_type:  0x%08x\n",entry->addr_type);
		printf("\tIP addr:    0x%08x\n",entry->addr.s_addr);
		error++;
	}
	return error;
}

static int
check_hosts(void)
{
	htbl_t * entry;
	int error = 0;

	entry = first_host;
	while (entry != NULL) {
		if (check_one_host(entry))
			error++;
		entry = entry->next;
	}
	return error;
}
#endif

/*
 *  Search the remote host list for a matching
 *  name.  Return a pointer to the entry, or
 *  NULL if not found.
 */
static htbl_t *
find_host_entry(const char *hostname)
{
	htbl_t * entry;

	if (hostname == NULL) {
		printf("Fatal internal error - "
			"find_host_entry called with null pointer\n");
		exit(1);
	}
	assert(hostname != NULL);
	entry = first_host;
	while (entry != NULL) {
		if (debug) {
			if (check_one_host(entry)) 
				exit(1);
		}
		if (strcasecmp(entry->hostname,hostname) == 0)
			return entry;
		entry = entry->next;
	}
	return NULL;
}

/*
 *  Add a new remote host entry.
 */
static htbl_t *
add_host(const char *host_name)
{
	htbl_t * host;
	unsigned long temp;
	struct hostent *hostent = 0;

	assert(rhdb_head != NULL);
	assert(rhdb_curr != NULL);

	host = (htbl_t *)malloc(sizeof(htbl_t));
	host->hostname = strdup(host_name);
	host->profile = rhdb_curr;
	host->next = NULL;
	host->related = NULL;
	host->flags = 0;

	if (first_host == NULL) {
		assert(last_host == NULL);
		first_host = host;
		last_host = host;
	} else {
		assert(last_host != NULL);
		last_host->next = host;
		last_host = host;	
	}

	/* Lookup IP Address */
	temp = inet_addr(host_name);
	if (temp != (unsigned long) -1) {
		host->addr_type = AF_INET;
		host->addr.s_addr = temp;
		host->flags |= H_ADDR_IP;
	} else {
		host->flags |= H_ADDR_NAME;
		hostent = gethostbyname(host_name);
		if (hostent) {
			host->addr_type = hostent->h_addrtype;
			if (hostent->h_length <= sizeof(host->addr))
				memcpy((void *) &host->addr.s_addr,
				       (void *) hostent->h_addr,
				       hostent->h_length);
		} else {
			host->flags |= H_ADDR_UNKNOWN;
		}
	}

	if (debug) {
		printf("Add host %s at 0x%08x  0x%08x\n",
			host_name, host, host->hostname);
	}
	return host;
}


/*
 * getline() Read a line from a file, converting backslash-newline to space.
 * Returns it argument, or NULL on end-of-file.
 */
static char *
getline(char * line, FILE * f)
{
	char * p;

	/* Skip blank lines and comments */
	do {
		if (!fgets(line, BUFSIZ, f)) {
			return (NULL);
		}
		cur_line++;
	} while (iseol(line[0]));

	p = line;
	for (;;) {
		while (!iseol(*p)) {
			p++;
		}
		if (*p == '\n' && *(p - 1) == '\\') {
			*(p - 1) = ' ';
			if (!fgets(p, BUFSIZ, f)) {
				break;
			}
			cur_line++;
		} else {
			*p = 0;
			break;
		}
	}
	return (line);
}

/*
 *  Parse the remote host config file.
 *  The file consists if one line (continuation allowed) per
 *  entry.  The fields are seperated by ':'.  A sample may
 *  be found in rhdb.conf.
 */
int
parse_conf(const char *cfilename)
{
	FILE *cfile;
	char c;
	int host_cnt;
	htbl_t * hp, *hp_related;

	msen_t min_sl;
	mint_t min_il;

	cap_t lcaps;

	/* Open config file */
	cfile = fopen(cfilename, "r");
	if (!cfile) {
		printf("Unable to open config file: %s\n", cfilename);
		perror("fopen");
		lptr = 0;
		return 1;
        }
	if (debug) {
		printf("config file is: %s\n", cfilename);
		fflush(stdout);
	}

	for (;;) {
		lptr = getline(lbuf,cfile);
		if (lptr == NULL)
			break;

		/* Allocate space to hold profile */
		rhdb_curr = new_host_entry();
		if (rhdb_curr == NULL) {
			perror("new_host_entry");
			(void) fclose(cfile);
			exit(1);
		}

		/* Each line begins with one or more host names */
		host_cnt=0;
		hp_related = NULL;
		lptr += strspn(lptr, " \t");		/* Skip whitespace */
		c = *lptr;
		while (c != ':') {
			h = lptr;
			lptr = strpbrk(lptr, " \t:=");
			if ((c = *lptr) == '=') {
				printf("Near line %d, entry missing ':'\n",
				    cur_line);
				(void) fclose(cfile);
				return 1;
			}
			*lptr++ = '\0';
			hp = add_host(h);
			if (hp_related == NULL) {
				hp_related = hp;
				rhdb_curr->rh_host_list = hp;
			} else {
				hp_related->related = hp;
				hp_related = hp;
			}
			host_cnt++;
			if (debug) {
				printf("Line %03d: Processing host %s at"
					"0x%08x\n", cur_line, h, hp);
				fflush(stdout);
			}
		}

		/* Validate Hosts */
		if (host_cnt == 0) {
			printf("No host specified\n");
			continue;
		}

		/* Parse parameters */
		while (lptr != 0) {
			/* find the next keyword and following '=', if any */
			lptr += strspn(lptr, " \t");
			if (*lptr == '\0' || *lptr == '\n')
				break;
			k = lptr;
			v = 0;

			lptr = strpbrk(lptr, " \t=");
			if (lptr) {
				if (*lptr != '=') {
					*lptr++ = '\0';
					lptr += strspn(lptr, " \t");
				}
				if (*lptr == '=') {
					*lptr++ = '\0';
					v = lptr+strspn(lptr, " \t");
#ifdef LATER
					if (*v == '"') {
						/* simplistic quoted string */
						lptr = strchr(++v, '"');
					} else {
						lptr = strpbrk(v, " \t");
					}
#endif
					lptr = strchr(v,':');
					if (lptr)
						*lptr++ = '\0';
				}
			}

			/* binary search the list of keywords */
			lo = 0;
			hi = KEYTBL_LEN;
			for (;;) {
				tgt = lo + (hi-lo)/2;
				i = strcasecmp(keytbl[tgt].str, k);
				if (i < 0) {
					lo = tgt+1;
				} else if (i == 0) {
					/* found it */
					if (keytbl[tgt].flag && v != 0)
						/* syntax error */
						tgt = KEYTBL_LEN-1;
					break;
				} else {
					hi = tgt-1;
				}
				if (lo >= KEYTBL_LEN || hi < lo) {
					/* force syntax error */
					tgt = KEYTBL_LEN-1;
					break;
				}
			}

			switch (keytbl[tgt].key) {

			case KEYW_HOST_TYPE:
				printf("\tHost type is: %s\n",v);
				break;

			case KEYW_SMM_TYPE:
				/*printf("\tSMM type is: %s\n",v);*/
				rhdb_curr->rh_buf.hp_smm_type = find_smm(v);
				if (rhdb_curr->rh_buf.hp_smm_type ==
				    T6RHDB_SMM_INVALID)
					printf("%3d Invalid session"
					    " manager: %s\n",cur_line, v);
				break;

			case KEYW_NLM_TYPE:
				/*printf("\tNLM type is: %s\n",v); */
				rhdb_curr->rh_buf.hp_nlm_type = find_nlm(v);
				if (rhdb_curr->rh_buf.hp_nlm_type ==
					T6RHDB_NLM_INVALID)
					printf("%3d Invalid network"
					    " module: %s\n", cur_line, v);
				break;

			case KEYW_IPSEC:
				printf("\tIPSEC keyword identified\n");
				break;

			case KEYW_DEFAULT_SPEC:
			{
				int x;
				htbl_t *host;
				/* Default Spec */
				if (v[0] == '.') {
					rhdb_curr->rh_entry_type |=
					    T6RHDB_ENTRY_DEFAULT;
					for (x = 1; v[x] != '\0'; x++)
					if (!isspace(v[x]))
						printf(
						    "Extranous characters\n");
					break;
				}

				/* Uses default spec */
				if (debug) {
					printf("Searching for default profile"
						"for %s\n",v);
					fflush(stdout);
				}
				host = find_host_entry(v);
				if (host == NULL) {
					printf("Default profile not found\n");
					break;
				}
				if (host->profile == NULL) {
					printf("No profile for this host\n");
					break;
				}
				rhdb_curr->rh_default_profile = host->profile;
				break;
			}

			case KEYW_CACHE_SIZE:
				rhdb_curr->rh_buf.hp_cache_size = atoi(v);
				break;

			case KEYW_MIN_SL:
				min_sl = msen_from_text(v);
				if (min_sl == NULL)
					printf("%3d Invalid min sl: %s\n",
					    cur_line, v);
				else
					memcpy(&rhdb_curr->rh_buf.hp_min_sl, min_sl, sizeof(mac_b_label));
				msen_free(min_sl);
				if (check_hosts())
					printf("Host list corrupted 1/%d\n",
						cur_line);
				break;

			case KEYW_MAX_SL:
				min_sl = msen_from_text(v);
				if (min_sl == NULL)
					printf("%3d Invalid max sl: %s\n",
					    cur_line, v);
				else
					memcpy(&rhdb_curr->rh_buf.hp_max_sl, min_sl, sizeof(mac_b_label));
				msen_free(min_sl);
				if (check_hosts())
					printf("Host list corrupted 2/%d\n",
						cur_line);
				break;

			case KEYW_MIN_INTEG:
				min_il = mint_from_text(v);
				if (min_il == NULL)
					printf("%3d Invalid min integ: %s\n",
					    cur_line, v);
				else
					memcpy(&rhdb_curr->rh_buf.hp_min_integ,
					       min_il, sizeof(mac_b_label));
				mint_free(min_il);
				if (check_hosts())
					printf("Host list corrupted 3/%d\n",
						cur_line);
				break;

			case KEYW_MAX_INTEG:
				min_il = mint_from_text(v);
				if (min_il == NULL)
					printf("%3d Invalid max integ: %s\n",
					    cur_line, v);
				else
					memcpy(&rhdb_curr->rh_buf.hp_max_integ,
					       min_il, sizeof(mac_b_label));
				mint_free(min_il);
				if (check_hosts())
					printf("Host list corrupted 4/%d\n",
						cur_line);
				break;

			case KEYW_DEF_SL:
				min_sl = msen_from_text(v);
				if (min_sl == NULL)
					printf("%3d Invalid default sl: %s\n",
					    cur_line, v);
				else
					memcpy(&rhdb_curr->rh_buf.hp_def_sl,
					       min_sl, sizeof(mac_b_label));
				msen_free(min_sl);
				if (check_hosts())
					printf("Host list corrupted 5/%d\n",
						cur_line);
				break;

			case KEYW_DEF_INTEG:
				min_il = mint_from_text(v);
				if (min_il == NULL)
					printf("%3d Invalid default integ: %s\n", cur_line, v);
				else
					memcpy(&rhdb_curr->rh_buf.hp_def_integ,
					       min_il, sizeof(mac_b_label));
				mint_free(min_il);
				if (check_hosts())
					printf("Host list corrupted 6/%d\n",
						cur_line);
				break;

			case KEYW_DEF_ILB:
				min_sl = msen_from_text(v);
				if (min_sl == NULL)
					printf("%3d Invalid information"
					    " label: %s\n",cur_line, v);
				else
					memcpy(&rhdb_curr->rh_buf.hp_def_ilb,
					       min_sl,
					       sizeof(mac_b_label));
				msen_free(min_sl);
				if (check_hosts())
					printf("Host list corrupted 7/%d\n",
						cur_line);
				break;

			case KEYW_DEF_CLEARANCE:
				min_sl = msen_from_text(v);
				if (min_sl == NULL)
					printf("%3d Invalid clearance: %s\n",
					    cur_line, v);
				else
					memcpy(&rhdb_curr->rh_buf.hp_def_clearance, min_sl, sizeof(mac_b_label));
				msen_free(min_sl);
				if (check_hosts())
					printf("Host list corrupted 8/%d\n",
						cur_line);
				break;

			case KEYW_DEF_UID:
				if ((rhdb_curr->rh_buf.hp_def_uid = get_uid(v)) == -1)
					printf("%3d User Id invalid: %s\n",
					    cur_line, v);
				break;

			case KEYW_DEF_LUID:
				if ((rhdb_curr->rh_buf.hp_def_luid = get_uid(v)) == -1)
					printf("%3d User Id invalid: %s\n",
					    cur_line, v);
				break;

			case KEYW_DEF_SID:
				if ((rhdb_curr->rh_buf.hp_def_sid = get_uid(v)) == -1)
					printf("%3d Session Id invalid: %s\n",
					   cur_line, v);
				break;

			case KEYW_DEF_GID:
				if ((rhdb_curr->rh_buf.hp_def_gid = get_gid(v)) == -1)
					printf("%3d Group Id invalid: %s\n",
					    cur_line,v);
				break;

			case KEYW_DEF_NGRPS:
				if (!isdigit(v[0]))
					printf("%3d Invalid number of groups\n",
					    cur_line);
				rhdb_curr->rh_buf.hp_def_grp_cnt = atoi(v);
				if (rhdb_curr->rh_buf.hp_def_grp_cnt > NGROUPS_MAX)
					printf("%3d Max number of groups"
					       " exceeded\n",cur_line);
				if (debug) {
					printf("%3d number of groups is %d\n", cur_line,rhdb_curr->rh_buf.hp_def_grp_cnt);
				}
				break;

			case KEYW_DEF_GIDS:
			{
				gid_t gid;
				int gcnt;
				char *g, *temp;

				gcnt = 0;
				if (v == NULL || *v == '\0' || *v == '\n') {
					printf("No groups specified\n");
					break;
				}

				g = v;
				while (g != NULL) {
					v = strchr(g,',');
					if (v != NULL) {
						*v++ = '\0';
						v += strspn(v, " \t");
					}
					if ((temp = strpbrk(g," \t")) != NULL)
						*temp = '\0';
					gid = get_gid(g);
					if (gid == -1)
						printf("Group invalid\n");
					else {
						rhdb_curr->rh_buf.hp_def_groups[gcnt++]
						    = gid;
					}
					fflush(stdout);
					g = v;
					if (gcnt > NGROUPS_MAX) {
						printf("Max groups exceeded\n");
						break;
					}
					if (gcnt > rhdb_curr->rh_buf.hp_def_grp_cnt) {
						printf("Count exceeded\n");
						break;
					}
				}
				break;
			}

			case KEYW_DEF_AUDIT_ID:
				if ((rhdb_curr->rh_buf.hp_def_luid = get_uid(v)) == -1)
					printf("%3d Group Id invalid\n",
					       cur_line);
				break;

			case KEYW_DEF_PRIVS:
				if (strcasecmp(v,"none") == 0)
					break;

				if (strcasecmp(v,"all_privs") == 0)
					break;

				if ((lcaps = cap_from_text(v)) == NULL) {
					printf("%3d Unable to parse def caps: %s\n", cur_line, v);
				} else
					memcpy(&rhdb_curr->rh_buf.hp_def_priv,
					       lcaps, sizeof(cap_set_t));
				cap_free(lcaps);
				if (check_hosts())
					printf("Host list corrupted 9/%d\n",
						cur_line);
				break;

			case KEYW_MAX_PRIVS:
				if (strcasecmp(v,"none") == 0)
					break;

				if (strcasecmp(v,"all_privs") == 0)
					break;

				if ((lcaps = cap_from_text(v)) == NULL) {
					printf("%3d Unable to parse max caps: %s\n",
					    cur_line, v);
				} else
					memcpy(&rhdb_curr->rh_buf.hp_max_priv,
					       lcaps, sizeof(cap_set_t));
				cap_free(lcaps);
				if (check_hosts())
					printf("Host list corrupted 10/%d\n",
						cur_line);
				break;

			case KEYW_VENDOR:
				rhdb_curr->rh_vendor = find_vendor(v);
				if (rhdb_curr->rh_vendor ==
				    T6RHDB_VENDOR_INVALID)
					printf("%3d Invalid vendor"
					    " id: %s\n",cur_line, v);
				break;

			case KEYW_DOI:
				printf("\tDOI = %s\n",v);
				break;

			case KEYW_FLAGS:
			{
				int id;
				char *f, *temp;

				if (v == NULL || *v == '\0' || *v == '\n') {
					printf("No flags specified\n");
					break;
				}

				f = v;
				while (f != NULL) {
					v = strchr(f,',');
					if (v != NULL) {
						*v++ = '\0';
						v += strspn(v, " \t");
					}
					if ((temp = strpbrk(f," \t")) != NULL)
						*temp = '\0';
					id = find_flag(f);
					if (id == T6RHDB_FLG_INVALID)
						printf("Flag invalid\n");
					else
						rhdb_curr->rh_buf.hp_flags |=
						   T6RHDB_MASK(id);
					fflush(stdout);
					f = v;

				}
				break;
			}

			case KEYW_ZZZZ:
				printf("\tZZZZ found\n");
				break;

			default:
				printf("%3d Unknown keyword: %s - %s\n",
				   cur_line, k, v);
				break;
			}

		} /* end keyword scan */
	}

	/* Close */
	(void)fclose(cfile);
	return 0;
}
