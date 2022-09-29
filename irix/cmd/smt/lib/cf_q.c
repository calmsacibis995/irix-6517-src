/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.9 $
 */

#include <sys/types.h>
#include <sm_types.h>
#include <sm_log.h>
#include <smtd.h>

void
dequeue_phy(SMT_PHY *phy)
{
	register SMT_PHY *ophy;

	if (!phy)
		return;

	if ((ophy = smtp->phy) == 0)
		goto free_phy;

	if (phy == ophy) {
		smtp->phy = phy->next;
		goto dq_phy;
	}
	while (ophy->next) {
		if (phy == ophy->next) {
			ophy->next = phy->next;
			goto dq_phy;
		}
		ophy = ophy->next;
	}

	/* Not queued yet, Just free it */
	goto free_phy;

dq_phy:
	smtp->phy_ct--;
	CFDEBUG((LOG_DBGCF, 0, "phy dequeued and %d phys left", smtp->phy_ct));

free_phy:
	free(phy);
}

void
enqueue_phy(SMT_PHY *phy)
{
	register SMT_PHY *ophy;

	if (!phy) {
		CFDEBUG((LOG_DBGCF, 0, "no phy to enqueue"));
		return;
	}

	ophy = smtp->phy;
	if (ophy == 0) {
		smtp->phy = phy;
	} else {
		while (ophy->next)
			ophy = ophy->next;
		ophy->next = phy;
	}

	phy->next = 0;	/* paranoid */
	smtp->phy_ct++;
	CFDEBUG((LOG_DBGCF, 0, "total phy enqueued = %d", smtp->phy_ct));
}

void
dequeue_mac(SMT_MAC *mac)
{
	register SMT_MAC *omac;

	if (!mac)
		return;

	if ((omac = smtp->mac) == 0)
		goto free_mac;

	if (mac == omac) {
		smtp->mac = mac->next;
		goto dq_mac;
	}
	while (omac->next) {
		if (mac == omac->next) {
			omac->next = mac->next;
			goto dq_mac;
		}
		omac = omac->next;
	}

	/* not queued yet. Just free it */
	goto free_mac;

dq_mac:
	smtp->mac_ct--;
	CFDEBUG((LOG_DBGCF, 0, "mac dequeued and %d mac left", smtp->mac_ct));
free_mac:
	dequeue_phy(mac->primary);
	dequeue_phy(mac->secondary);
	free(mac->st);
	free(mac->st1);
	free(mac);
}
void
enqueue_mac(SMT_MAC *mac)
{
	register SMT_MAC *omac;

	if (!mac) {
		CFDEBUG((LOG_DBGCF, 0, "no mac to enqueue"));
		return;
	}

	omac = smtp->mac;
	if (omac == 0) {
		smtp->mac = mac;
	} else {
		while (omac->next)
			omac = omac->next;
		omac->next = mac;
	}

	mac->next = 0;	/* paranoid */
	smtp->mac_ct++;
	CFDEBUG((LOG_DBGCF, 0, "total mac enqueued = %d", smtp->mac_ct));
}
