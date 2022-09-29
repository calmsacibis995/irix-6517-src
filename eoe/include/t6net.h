#ifndef __T6NET_H__
#define __T6NET_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.2 $"

#include <sys/types.h>

struct mac_label;
struct in_addr;

/*
 * CAP_MAC_UPGRADE, CAP_MAC_DOWNGRADE, CAP_MAC_RELABEL_OPEN, CAP_MAC_MLD
 * CAP_NETWORK_MGT
 */
int tsix_set_mac (int, struct mac_label *);
int tsix_set_mac_byrhost (int, struct in_addr *, struct mac_label **);

/*
 * CAP_SETUID
 * CAP_NETWORK_MGT
 */
int tsix_set_uid (int, uid_t);

/*
 * CAP_MAC_UPGRADE, CAP_MAC_DOWNGRADE
 * CAP_MAC_RELABEL_OPEN, CAP_MAC_MLD
 */
int tsix_sendto_mac (int, const void *, int, int, const void *, int,
		     struct mac_label *);

/*
 * CAP_NETWORK_MGT
 */
int tsix_on (int);
int tsix_off (int);
int tsix_set_solabel (int, struct mac_label *);

/*
 * NO CAPABILITY REQUIRED
 */
int tsix_get_mac (int, struct mac_label **);
int tsix_get_uid (int, uid_t *);
int tsix_get_solabel (int, struct mac_label **);
int tsix_recvfrom_mac (int, void *, int, int, void *, int *,
		       struct mac_label **);


#ifdef __cplusplus
}
#endif

#endif /* __T6NET_H__ */
