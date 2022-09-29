#ifndef __FILE_LAMED_H__
#define __FILE_LAMED_H__

#include <sys/param.h>

#define ALIASES			0
#define ALIASES_BYNAME		"mail.aliases"
#define ALIASES_BYADDR		"mail.byaddr"

#define BOOTPARAMS		1
#define BOOTPARAMS_BYNAME	"bootparams"

#define ETHERS			2
#define ETHERS_BYNAME		"ethers.byname"
#define ETHERS_BYADDR		"ethers.byaddr"

#define GROUP			3
#define GROUP_BYNAME		"group.byname"
#define GROUP_BYGID		"group.bygid"
#define GROUP_BYMEMBER		"group.bymember"

#define	HOSTS			4
#define	HOSTS_BYNAME		"hosts.byname"
#define HOSTS_BYADDR		"hosts.byaddr"

#define NETWORKS		5
#define NETWORKS_BYNAME		"networks.byname"
#define NETWORKS_BYADDR		"networks.byaddr"

#define	PASSWD			6
#define PASSWD_BYNAME		"passwd.byname"
#define PASSWD_BYUID		"passwd.byuid"

#define	PROTOCOLS		7
#define PROTOCOLS_BYNAME	"protocols.byname"
#define PROTOCOLS_BYNUMBER	"protocols.bynumber"

#define	RPC			8
#define RPC_BYNAME		"rpc.byname"
#define RPC_BYNUMBER		"rpc.bynumber"

#define SHADOW			9
#define SHADOW_BYNAME		"shadow.byname"

#define SERVICES		10
#define SERVICES_BYNAME		"services"
#define SERVICES_BYPORT		"services.byname"

#define MAC			11
#define MAC_BYNAME		"mac.byname"
#define MAC_BYVALUE		"mac.byvalue"

#define CAPABILITY		12
#define CAPABILITY_BYNAME	"capability.byname"

#define CLEARANCE		13
#define CLEARANCE_BYNAME	"clearance.byname"

#define NETGROUP		14
#define NETGROUP_BYNAME		"netgroup"
#define NETGROUP_BYHOST		"netgroup.byhost"
#define NETGROUP_BYUSER		"netgroup.byuser"

#define GENERIC			15

#define ERROR_INDEX		16

#define MAXRESULT		4096
#define FILE_MAX_DOMAIN		255
#define RETRIES 3

#define TOSPACE(p) for (; *(p) && !isspace(*(p)); (p)++)
#define SKIPWSPACE(p) for (; *(p) && isspace(*(p)); (p)++)
#define SKIPSPACE(p) for (; *(p) && (*(p) == ' ' || *(p) == '\t'); (p)++)

typedef struct file_domain {
	struct file_domain	*next;
	struct timeval		version;
	time_t			touched;
	uint32_t                flags;
	char			*map;
	long long		size;
	char			path[1];
} file_domain_t;

/* Values for file_domain_t.flags */
#define FUTUREDATE (1<<0)	/* Is the date on the file in the future? */

typedef struct file_name {
	char			*value;
	struct file_name	*next;
} file_name_t;

#define TYPE_PASSWD	0
#define TYPE_NETGROUP	1
#define TYPE_LOGIN	2
#define TYPE_UID	3
#define TYPE_ALL	4
#define TYPE_ERROR	-1

#define FILEID(h)	(uint32_t)(h)[0]

int file_open(file_domain_t *);
file_domain_t *file_domain_lookup(nsd_file_t *);
void file_domain_clear(void);
int file_callback(nsd_file_t **, int);

#endif /* __FILE_LAMED_H__ */
