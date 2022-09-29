#ifndef __RHOST_CONF_H__
#define __RHOST_CONF_H__

#ident "$Revision: 1.2 $"

/* define DEFINE as either "extern" or "" */
#ifdef DEFINE
#define EXT
#define INIT(x) = x
#else
#define EXT extern
#define INIT(x)
#endif

#include <limits.h>
#include <netinet/in.h>
#include <sys/t6rhdb.h>

#define T6RHDB_ENTRY_DEFAULT	0x001
#define T6RHDB_ENTRY_ERROR	0x002

#define T6RHDB_MAX_HOST		16

typedef struct rhost {
	struct rhost  *rh_next_profile;
	struct htbl   *rh_host_list;
	struct rhost  *rh_default_profile;
	int	      rh_entry_type;
	int 	      rh_vendor;
	t6rhdb_host_buf_t rh_buf;
} rhost_t;

EXT rhost_t * rhdb_head INIT(NULL);
EXT rhost_t * rhdb_tail INIT(NULL);
EXT rhost_t * rhdb_curr INIT(NULL);

#define H_ADDR_IP	0x002
#define H_ADDR_NAME	0x004
#define H_ADDR_UNKNOWN	0x008
#define H_DFLT_PROFILE	0x010

typedef struct htbl {
	char *		hostname;
	rhost_t *	profile;
	struct htbl * 	next;		/* list of all hosts */
	struct htbl * 	related;	/* all host with same profile */
	int		flags;
	int		addr_type;
	struct in_addr  addr;
} htbl_t;


EXT htbl_t * first_host INIT(NULL);
EXT htbl_t * curr_host  INIT(NULL);
EXT htbl_t * last_host  INIT(NULL);

EXT int debug;

EXT char *remote;

extern int parse_conf(const char *);

#endif /* __RHOST_CONF_H__ */
