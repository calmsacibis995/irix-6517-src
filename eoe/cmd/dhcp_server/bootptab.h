#include <sys/param.h>
#include <netinet/in.h>

#ifndef uint64
# ifdef _LONGLONG
#  define uint64        unsigned long long
# else
#  define uint64        unsigned __long_long
# endif
#endif

/* 
 * bootptab.h 
 */

#ifndef _bootptab_h
#define _bootptab_h

struct hosts {
	char	host[MAXHOSTNAMELEN+1];	/* host name (and suffix) */
	u_char	htype;		/* hardware type */
	u_char	haddr[6];	/* hardware address */
	struct in_addr	iaddr;		/* internet address */
	char	bootfile[128];	/* default boot file name */
};

extern int nhosts;

extern int insert_btab(struct hosts *hp);
extern struct hosts *lookup_btabm(u_char htype, u_char* haddr);
extern void cleanup_btabm(void);
extern struct hosts *lookup_btabi(struct in_addr iaddr);
extern void cleanup_btabi(void);
#endif /* _bootptab_h */
