/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Configure all known protocol modules.
 */
#include "exception.h"
#include "protocol.h"

/*
 * Protocols below are grouped and indented to show nesting layers.
 */
extern Protocol
	snoop_proto,
	loop_proto, ether_proto, fddi_proto, crayio_proto,
	tokenring_proto, hippi_proto, atm_proto,
	    llc_proto, mac_proto, smt_proto, tokenmac_proto,
	    arp_proto, rarp_proto,
		arpip_proto,
		osi_proto, sna_proto,
	    ip_proto,
		hello_proto, icmp_proto, igmp_proto, tcp_proto, udp_proto,
		    dns_proto, bootp_proto, rip_proto, snmp_proto,
		    sunrpc_proto, tftp_proto, ftp_proto, telnet_proto,
		    smtp_proto, tsp_proto, x11_proto, rlogin_proto,
		    rcp_proto,
			nfs_proto, pmap_proto, nlm_proto,
			netbios_proto,
	    rp_proto,
		nsp_proto,
	    xtp_proto,
	    vines_proto,
	    lat_proto,
	    ipx_proto,
		spx_proto, novellrip_proto,
	    idp_proto,
		xnsrip_proto, echo_proto, error_proto, pep_proto, spp_proto,
	    elap_proto,
		aarp_proto, ddp_proto,
		    rtmp_proto, aep_proto, atp_proto,
		    nbp_proto, adsp_proto, zip_proto,
			asp_proto, pap_proto,
			    afp_proto;

Protocol *_protocols[] = {
	&snoop_proto,
	&loop_proto, &ether_proto, &fddi_proto, &crayio_proto,
	&tokenring_proto, &hippi_proto, &atm_proto,
	    &llc_proto, &mac_proto, &smt_proto, &tokenmac_proto,
	    &arp_proto, &rarp_proto,
		&arpip_proto,
		&osi_proto, &sna_proto,
	    &ip_proto,
		&hello_proto, &icmp_proto, &igmp_proto, &tcp_proto, &udp_proto,
		    &dns_proto, &bootp_proto, &rip_proto, &snmp_proto,
		    &sunrpc_proto, &tftp_proto, &ftp_proto, &telnet_proto,
		    &smtp_proto, &tsp_proto, &x11_proto, &rlogin_proto,
		    &rcp_proto,
			&nfs_proto, &pmap_proto, &nlm_proto,
			&netbios_proto,
	    &rp_proto,
		&nsp_proto,
	    &xtp_proto,
	    &vines_proto,
	    &lat_proto,
	    &ipx_proto,
		&spx_proto, &novellrip_proto,
	    &idp_proto,
		&xnsrip_proto,&echo_proto,&error_proto,&pep_proto,&spp_proto,
	    &elap_proto,
		&aarp_proto, &ddp_proto,
		    &rtmp_proto, &aep_proto, &atp_proto,
		    &nbp_proto, &adsp_proto, &zip_proto,
			&asp_proto, &pap_proto,
			    &afp_proto,
	0
};

void
initprotocols()
{
	Protocol **prp;

	for (prp = _protocols; *prp; prp++) {
		if (!pr_init(*prp))
			exc_raise(0, "cannot initialize %s protocol",
				  (*prp)->pr_name);
	}
}

ProtOptDesc *
pr_findoptdesc(Protocol *pr, const char *name)
{
	ProtOptDesc *pod;
	int npod;

	pod = pr->pr_optdesc;
	if (pod == 0)
		return 0;
	for (npod = pr->pr_numoptions; --npod >= 0; pod++)
		if (!strcmp(pod->pod_name, name))
			return pod;
	return 0;
}
