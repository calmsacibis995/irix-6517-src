#include <sys/types.h>
#include <sys/systm.h>
#include <sys/errno.h>

/*
 * Session Manager Stubs.
 */

#ifdef DEBUG
#define DOPANIC(s) panic(s)
#else /* DEBUG */
#define DOPANIC(s)
#endif /* DEBUG */

struct ipsec;
struct mbuf;

void sesmgr_init()		{/* Do not put a DOPANIC in this stub! */}
int  sgi_sesmgr()		{ return(ENOPKG); }
void sesmgr_confignote()	{ DOPANIC("sesmgr_confignote stub"); }

void sesmgr_lreceive()		{ DOPANIC("sesmgr_lreceive stub");
				  /* NOTREACHED */ }
void sesmgr_get_label()		{ DOPANIC("sesmgr_get_label stub"); }
void sesmgr_set_label()		{ DOPANIC("sesmgr_set_label stub"); }
void sesmgr_get_sendlabel()	{ DOPANIC("sesmgr_get_sendlabel stub"); }
void sesmgr_set_sendlabel()	{ DOPANIC("sesmgr_set_sendlabel stub"); }
void sesmgr_get_soacl()		{ DOPANIC("sesmgr_get_soacl stub"); }
void sesmgr_set_soacl()		{ DOPANIC("sesmgr_set_soacl stub"); }
void sesmgr_get_uid()		{ DOPANIC("sesmgr_get_uid stub"); }
void sesmgr_set_uid()		{ DOPANIC("sesmgr_set_uid stub"); }
void sesmgr_get_rcvuid()	{ DOPANIC("sesmgr_get_rcvuid stub"); }
void sesmgr_set_rcvuid()	{ DOPANIC("sesmgr_set_rcvuid stub"); }
void sesmgr_soattr_free()	{DOPANIC("sesmgr_soattr_free stub");
				  /* NOTREACHED */ }
void sesmgr_add_options()	{DOPANIC("sesmgr_add_options stub");
				  /* NOTREACHED */ }
void sesmgr_socket_copy_attrs()	{DOPANIC("sesmgr_socket_copy_attrs stub");
				  /* NOTREACHED */ }
void sesmgr_socket_init_attrs()	{DOPANIC("sesmgr_socket_init_attrs stub");
				  /* NOTREACHED */ }
void sesmgr_socket_free_attrs()	{DOPANIC("sesmgr_socket_free_attrs stub");
				  /* NOTREACHED */ }
void sesmgr_satmp_soclear()	{DOPANIC("sesmgr_satmp_soclear stub");
				  /* NOTREACHED */ }
void sesmgr_ip_options()	{DOPANIC("sesmgr_ip_options stub");
				  /* NOTREACHED */ }
int ip_sec_dooptions()		{DOPANIC("ip_sec_dooptions stub");
				  /* NOTREACHED */ }
struct mac_label * ip_sec_get_label() {DOPANIC("ip_sec_get_label stub");
				  /* NOTREACHED */ }
struct mac_b_label *ip_sec_get_msen() {DOPANIC("ip_sec_get_msen stub");
				  /* NOTREACHED */ }
struct mac_b_label *ip_sec_get_mint() {DOPANIC("ip_sec_get_mint stub");
				  /* NOTREACHED */ }
ushort ip_sec_get_uid()		{DOPANIC("ip_sec_get_uid stub");
				  /* NOTREACHED */ }
int ip_sec_add_cache()		{DOPANIC(" stub");
				  /* NOTREACHED */ }
struct mbuf * ip_sec_get_xmt()	{DOPANIC("ip_sec_get_xmt stub");
				  /* NOTREACHED */ }
int ip_sec_get_rcv()		{DOPANIC("ip_sec_get_rcv stub");
				  /* NOTREACHED */ }
struct mbuf * ip_sec_insertopts() {DOPANIC("ip_sec_insertopts stub"); 
				  /* NOTREACHED */ }
int  sesmgr_samp_accept()	{DOPANIC("sesmgr_samp_accept stub");
				  /* NOTREACHED */ }
int  sesmgr_put_samp()		{DOPANIC("sesmgr_put_samp stub");
				  /* NOTREACHED */ }
int  sesmgr_samp_init_data()	{DOPANIC("sesmgr_samp_init_data stub");
				  /* NOTREACHED */ }
int  sesmgr_samp_send_data()	{DOPANIC("sesmgr_samp_send_data stub");
				  /* NOTREACHED */ }
int sesmgr_samp_kudp()		{DOPANIC("sesmgr_samp_kudp stub");
				  /* NOTREACHED */ }
void sesmgr_samp_sbcopy()	{DOPANIC("sesmgr_samp_sbcopy stub");
				  /* NOTREACHED */ }
struct ipsec *sesmgr_nfs_soattr_copy()	{DOPANIC("sesmgr_nfs_soattr_copy stub");
				  /* NOTREACHED */ }
int sesmgr_nfs_vsetlabel()	{DOPANIC("sesmgr_nfs_vsetlabel stub");
				  /* NOTREACHED */ }
struct mbuf *sesmgr_nfs_set_ipsec()	{DOPANIC("sesmgr_nfs_set_ipsec stub");
						/* NOTREACHED */ }
int sesmgr_soreceive()		{DOPANIC("sesmgr_nfs_vsetlabel stub");
				  /* NOTREACHED */ }

void set_lo_secattr()		{DOPANIC("set_lo_secattr stub");
				  /* NOTREACHED */ }
int sesmgr_ip_output()		{DOPANIC("sesmgr_ip_output stub");
				  /* NOTREACHED */ }
int siocgetlabel()		{DOPANIC("siocgetlabel stub");
				  /* NOTREACHED */ }
int siocsetlabel()		{DOPANIC("siocsetlabel stub");
				  /* NOTREACHED */ }
