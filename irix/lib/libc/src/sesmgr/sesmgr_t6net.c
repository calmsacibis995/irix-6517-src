#ifdef __STDC__
	#pragma weak tsix_get_mac		= _tsix_get_mac
	#pragma weak tsix_get_solabel		= _tsix_get_solabel
	#pragma weak tsix_get_uid		= _tsix_get_uid
	#pragma weak tsix_off			= _tsix_off
	#pragma weak tsix_on			= _tsix_on
	#pragma weak tsix_recvfrom_mac		= _tsix_recvfrom_mac
	#pragma weak tsix_sendto_mac		= _tsix_sendto_mac
	#pragma weak tsix_set_mac		= _tsix_set_mac
	#pragma weak tsix_set_mac_byrhost	= _tsix_set_mac_byrhost
	#pragma weak tsix_set_solabel		= _tsix_set_solabel
	#pragma weak tsix_set_uid		= _tsix_set_uid
#endif

#include "synonyms.h"
#include <sys/types.h>
#include <sys/capability.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mac.h>
#include <sys/mac_label.h>
#include <sys/t6attrs.h>
#include <sys/t6rhdb.h>
#include <sys/ioctl.h>
#include <t6net.h>

#ident "$Revision: 1.6 $"

static int cipso_enabled = -1;
static const cap_value_t cap_label_mgt[] = {CAP_MAC_UPGRADE,
					    CAP_MAC_DOWNGRADE,
					    CAP_MAC_RELABEL_OPEN,
					    CAP_MAC_MLD};
static const cap_value_t cap_network_mgt = CAP_NETWORK_MGT;
static const cap_value_t cap_setuid = CAP_SETUID;

int
tsix_on (int fd)
{
	int r = 0;

	if (cipso_enabled == -1)
		cipso_enabled = (sysconf (_SC_IP_SECOPTS) > 0);

	if (cipso_enabled)
	{
		cap_t ocap = cap_acquire (1, &cap_network_mgt);
		r = t6mls_socket (fd, T6_ON);
		cap_surrender (ocap);
	}
	return (r);
}

int
tsix_off (int fd)
{
	int r = 0;

	if (cipso_enabled == -1)
		cipso_enabled = (sysconf (_SC_IP_SECOPTS) > 0);

	if (cipso_enabled)
	{
		cap_t ocap = cap_acquire (1, &cap_network_mgt);
		r = t6mls_socket (fd, T6_OFF);
		cap_surrender (ocap);
	}
	return (r);
}

/*
 * Do a recvfrom, obtaining the label of the incoming packet.
 */
int
tsix_recvfrom_mac (int fd, void *buf, int len, int flags, void *from,
		   int *fromlen, mac_t *labelp)
{
	const t6mask_t T6MASK = T6M_SL | T6M_INTEG_LABEL;
	t6attr_t t6attr;
	t6mask_t t6mask;
	msen_t sl;
	mint_t il;
	int rlen;

	if (labelp != NULL)
		*labelp = NULL;

	if (cipso_enabled == -1)
		cipso_enabled = (sysconf (_SC_IP_SECOPTS) > 0);

	if (!cipso_enabled)
		return (recvfrom (fd, buf, len, flags, from, fromlen));

	if (labelp == NULL)
		return (-1);

	/*
	 * create handle for the following attributes:
	 *	Sensitivity Label
	 *	Integrity Label
	 */
	t6attr = t6alloc_blk (T6MASK);
	if (t6attr == (t6attr_t) NULL)
		return (-1);

	/* Receive data with attributes */
	rlen = t6recvfrom (fd, buf, len, flags, from, fromlen,
			   t6attr, &t6mask);

	/*
	 * Ignore the rest of this stuff on error.
	 * Check that all asked for attributes were received.
	 */
	if (rlen == -1 || (t6mask & T6MASK) != T6MASK)
	{
		t6free_blk (t6attr);
		return (-1);
	}

	/* extract attributes from control structure */
	sl = (msen_t) t6get_attr (T6_SL, t6attr);
	il = (mint_t) t6get_attr (T6_INTEG_LABEL, t6attr);

	/* construct full mac label */
	*labelp = mac_from_msen_mint (sl, il);
	t6free_blk (t6attr);

	/* compute return value */
	return (*labelp == (mac_t) NULL ? -1 : rlen);
}

/*
 * Do a sendto, using the label of the incoming packet.
 */
int
tsix_sendto_mac (int fd, const void *msg, int len, int flags, const void *to,
		 int tolen, mac_t lbl)
{
	const t6mask_t T6MASK = T6M_SL | T6M_INTEG_LABEL;
	t6attr_t t6attr;
	msen_t sl;
	mint_t il;
	cap_t ocap;
	int r;

	if (cipso_enabled == -1)
		cipso_enabled = (sysconf (_SC_IP_SECOPTS) > 0);

	if (!cipso_enabled)
		return (sendto (fd, msg, len, flags, to, tolen));

	if (lbl == NULL)
		return (-1);

	/*
	 * create handle for the following attributes:
	 *	Sensitivity Label
	 *	Integrity Label
	 */
	t6attr = t6alloc_blk (T6MASK);
	if (t6attr == (t6attr_t) NULL)
		return (-1);

	/* set sensitivity portion of attribute block */
	sl = msen_from_mac (lbl);
	if (sl == NULL || t6set_attr (T6_SL, (void *) sl, t6attr) == -1)
	{
		msen_free (sl);
		t6free_blk (t6attr);
		return (-1);
	}
	msen_free (sl);

	/* set integrity portion of attribute block */
	il = mint_from_mac (lbl);
	if (il == NULL ||
	    t6set_attr (T6_INTEG_LABEL, (void *) il, t6attr) == -1)
	{
		mint_free (il);
		t6free_blk (t6attr);
		return (-1);
	}
	mint_free (il);

	/* send message + attributes */
	ocap = cap_acquire (4, cap_label_mgt);
	r = t6sendto (fd, msg, len, flags, to, tolen, t6attr);
	cap_surrender (ocap);
	t6free_blk (t6attr);

	return (r);
}

int
tsix_set_mac (int fd, mac_t label)
{
	const t6mask_t T6MASK = T6M_SL | T6M_INTEG_LABEL;
	t6attr_t t6attr;
	t6mask_t t6mask;
	msen_t sl = (msen_t) NULL;
	mint_t il = (mint_t) NULL;
	cap_t ocap;
	int r;

	if (cipso_enabled == -1)
		cipso_enabled = (sysconf (_SC_IP_SECOPTS) > 0);

	if (!cipso_enabled)
		return (0);

	if (label == NULL)
		return (-1);

	/*
	 * create handle for the following attributes:
	 *	Sensitivity Label
	 *	Integrity Label
	 */
	t6attr = t6alloc_blk (T6MASK);
	if (t6attr == (t6attr_t) NULL)
	    return (-1);

	/*
	 * unpack MAC label into sensitivity and integrity components
	 */
	sl = msen_from_mac (label);
	if (sl == NULL || t6set_attr (T6_SL, (void *) sl, t6attr) == -1)
	{
		msen_free (sl);
		t6free_blk (t6attr);
		return (-1);
	}
	msen_free (sl);

	il = mint_from_mac (label);
	if (il == NULL ||
	    t6set_attr (T6_INTEG_LABEL, (void *) il, t6attr) == -1)
	{
		mint_free (il);
		t6free_blk (t6attr);
		return (-1);
	}
	mint_free (il);
	
	ocap = cap_acquire (4, cap_label_mgt);
	r = t6set_endpt_default (fd, T6MASK, t6attr);
	cap_surrender (ocap);
	t6free_blk (t6attr);

	if (r == -1)
		return (r);

	/* set the connection mask to enable these attributes */
	if ((r = t6get_endpt_mask (fd, &t6mask)) == 0)
	{
		ocap = cap_acquire (1, &cap_network_mgt);
		r = t6set_endpt_mask (fd, t6mask | T6MASK);
		cap_surrender (ocap);
	}

	return (r);
}

/*
 * Get label of socket using TSIX interfaces.
 */
int
tsix_get_mac (int fd, mac_t *lbl)
{
	const t6mask_t T6MASK = T6M_SL | T6M_INTEG_LABEL;
	t6attr_t t6attr;
	t6mask_t t6mask;
	msen_t sl;
	mint_t il;

	if (lbl != NULL)
		*lbl = NULL;

	if (cipso_enabled == -1)
		cipso_enabled = (sysconf (_SC_IP_SECOPTS) > 0);

	if (!cipso_enabled)
		return (0);

	if (lbl == NULL)
		return (-1);

	/*
	 * create handle for the following attributes:
	 *	Sensitivity Label
	 *	Integrity Label
	 */
	t6attr = t6alloc_blk (T6MASK);
	if (t6attr == (t6attr_t) NULL)
		return (-1);

	/* retrieve attributes from connection */
	if (t6last_attr (fd, t6attr, &t6mask) == -1 ||
	    (t6mask & T6MASK) != T6MASK)
	{
		t6free_blk (t6attr);
		return (-1);
	}

	/* extract attributes from control structure */
	sl = (msen_t) t6get_attr (T6_SL, t6attr);
	il = (mint_t) t6get_attr (T6_INTEG_LABEL, t6attr);

	/* create complete mac label */
	*lbl = mac_from_msen_mint (sl, il);
	t6free_blk (t6attr);

	/* compute return value */
	return (*lbl == (mac_t) NULL ? -1 : 0);
}

int
tsix_set_solabel (int fd, mac_t label)
{
	cap_t ocap;
	int r;

	if (cipso_enabled == -1)
		cipso_enabled = (sysconf (_SC_IP_SECOPTS) > 0);

	if (!cipso_enabled)
		return (0);

	ocap = cap_acquire (1, &cap_network_mgt);
	r = ioctl (fd, SIOCSETLABEL, label);
	cap_surrender (ocap);

	return (r);
}

int
tsix_get_solabel (int fd, mac_t *labelp)
{
	if (labelp != NULL)
		*labelp = NULL;

	if (cipso_enabled == -1)
		cipso_enabled = (sysconf (_SC_IP_SECOPTS) > 0);

	if (!cipso_enabled)
		return (0);

	if (labelp == NULL)
		return (-1);

	*labelp = (mac_t) malloc (sizeof (struct mac_label));
	if (*labelp == NULL)
		return (-1);

	if (ioctl (fd, SIOCGETLABEL, *labelp) == -1)
	{
		free ((void *) *labelp);
		*labelp = (mac_t) NULL;
		return (-1);
	}
	return (0);
}

int
tsix_get_uid (int fd, uid_t *uidp)
{
	const t6mask_t T6MASK = T6M_UID;
	t6attr_t t6attr;
	t6mask_t t6mask;
	uid_t *p;

	if (uidp != NULL)
		*uidp = 0;

	if (cipso_enabled == -1)
		cipso_enabled = (sysconf (_SC_IP_SECOPTS) > 0);

	if (!cipso_enabled)
		return (0);

	if (uidp == NULL)
		return (-1);

	/*
	 * create handle for the following attributes:
	 *	User ID
	 */
	t6attr = t6alloc_blk (T6MASK);
	if (t6attr == (t6attr_t) NULL)
		return (-1);

	/* retrieve attributes from connection */
	if (t6last_attr (fd, t6attr, &t6mask) == -1 ||
	    (t6mask & T6MASK) != T6MASK)
	{
		t6free_blk (t6attr);
		return (-1);
	}

	/* extract attributes from control structure */
	p = (uid_t *) t6get_attr (T6_UID, t6attr);
	if (p == NULL)
	{
		t6free_blk (t6attr);
		return (-1);
	}
	*uidp = *p;
	t6free_blk (t6attr);

	return (0);
}

int
tsix_set_uid (int fd, uid_t id)
{
	const t6mask_t T6MASK = T6M_UID;
	t6attr_t t6attr;
	t6mask_t t6mask;
	cap_t ocap;
	int r;

	if (cipso_enabled == -1)
		cipso_enabled = (sysconf (_SC_IP_SECOPTS) > 0);

	if (!cipso_enabled)
		return (0);

	/*
	 * create handle for the following attributes:
	 *	User ID
	 */
	t6attr = t6alloc_blk (T6MASK);
	if (t6attr == (t6attr_t) NULL)
	    return (-1);

	if (t6set_attr (T6_UID, (void *) &id, t6attr) == -1)
	{
		t6free_blk (t6attr);
		return (-1);
	}

	ocap = cap_acquire (1, &cap_setuid);
	r = t6set_endpt_default (fd, T6MASK, t6attr);
	cap_surrender (ocap);
	t6free_blk (t6attr);

	if (r == -1)
		return (r);

	/* set the connection mask to enable these attributes */
	if ((r = t6get_endpt_mask (fd, &t6mask)) == 0)
	{
	    ocap = cap_acquire (1, &cap_network_mgt);
	    r = t6set_endpt_mask (fd, t6mask | T6MASK);
	    cap_surrender (ocap);
	}

	return (r);
}

int
tsix_set_mac_byrhost (int fd, struct in_addr *addr, mac_t *olbl)
{
	int failed;
	t6rhdb_host_buf_t hbuf;
	mac_t lbl;

	if (olbl != NULL)
		*olbl = NULL;

	if (cipso_enabled == -1)
		cipso_enabled = (sysconf (_SC_IP_SECOPTS) > 0);

	if (!cipso_enabled)
		return (0);

	memset((void *) &hbuf, '\0', sizeof(hbuf));
	failed = (t6rhdb_get_host(addr, sizeof(hbuf), (caddr_t) &hbuf) == -1);
	if (!failed) {
		lbl = mac_from_msen_mint(&hbuf.hp_def_sl, &hbuf.hp_def_integ);
		if (lbl == NULL || tsix_set_mac(fd, lbl) == -1)
			failed = 1;
		if (!failed && olbl != NULL)
			*olbl = lbl;
		else
			mac_free(lbl);
	}
	return (failed);
}
