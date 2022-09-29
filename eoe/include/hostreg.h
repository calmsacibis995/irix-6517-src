/*
 *	hostreg.h	1.1	7/27/88	host registrar initial release.
 */
#include <sys/bsd_types.h>

#define REG_NEWNAME	1	/* allocate new IP address */
#define REG_RENAME	2	/* rename host */
#define REG_DELETE	3	/* delete host and free IP address */

#define MAX_ALIASES	60	/* allows maximum characters */

struct reg_info {
	u_int	action;
	union {
		struct {
			u_long	network;
			u_long	netmask;
		} alloc_info;
		struct {
			char	passwd[9];
			char	rename_info[32];
		} change_info;
	} info;
	char	aliases[MAX_ALIASES+1];
};

#define info_network	info.alloc_info.network
#define info_netmask	info.alloc_info.netmask
#define info_rename	info.change_info.rename_info
#define info_passwd	info.change_info.passwd

typedef struct reg_info reg_info;
