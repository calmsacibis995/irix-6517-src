/*
 * remote.h
 *	Prototypes and declarations for functions to handle connections
 *	to and from remote FFSCs
 */

#ifndef _REMOTE_H_
#define _REMOTE_H_

struct dest;
struct user;
struct userinfo;


/* Function prototypes */
int	remAcceptConnection(int, rackid_t *, struct userinfo *);
STATUS	remInit(void);
int	remOpenConnection(rackid_t, const struct userinfo *);
STATUS  remReceiveData(int, char *, int);
STATUS  remSendData(struct user *, char *, int);
int	remStartConnectCommand(const struct user *, const char *,
			       rackid_t, struct dest *);
int	remStartDataRequest(const struct user *, const char *,
			    rackid_t, struct dest *, int *);
void	remStartRemoteCommand(const struct user *, const char *,
			      rackid_t, struct dest *);


#endif  /* _REMOTE_H_ */
