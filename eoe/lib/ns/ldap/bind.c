#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include "lber.h"
#ifdef _SSL
#include <bsafe.h>
#include <ssl.h>
#endif
#include "ldap.h"

extern int errno;

int
ldap_unbind(LDAP *ld)
{
	if (! ld) {
		perror("ldap_unbind");
		return -1;
	}

#ifdef _SSL
	if (ld->ld_sb.ssl) ssl_close(ld->ld_sb.ssl);
#endif
	close(ld->ld_sb.fd);
	free(ld->ld_sb.buf);
	free(ld);

	return 0;
}

LDAP *
ldap_make_ldap(void)
{
	LDAP	*ld;

	ld = (LDAP *) malloc(sizeof(LDAP));
	ld->ld_id = 0;
	ld->ld_req = (struct ldap_request *) 0;
	ld->ld_mes = (LDAPMessage *) 0;
	ld->ld_sb.buf = (char *) malloc(LBER_BUF_SIZE);
	ld->ld_sb.size = LBER_BUF_SIZE;
	ld->ld_sb.ptr = ld->ld_sb.end = ld->ld_sb.buf;
	ld->ld_sb.fd = -1;
	ld->ld_sb.opt = 0;
#ifdef _SSL
	ld->ld_sb.ssl = (SSL *) 0;
#endif

	return ld;
}

#ifdef _SSL
LDAP *
ldap_ssl_init(char *addr, short port, int cipher)
{
	struct sockaddr_in	so;
	LDAP			*ld;
	int			rc;
	SSL			*ssl;

	ld = ldap_make_ldap();
	ld->ld_sb.opt |= LBER_OPT_SSL;

	if ((ssl = ssl_init(addr, port, SSL_OPT_NOBLOCK, cipher)) == NULL) {
		perror("ssl_init");
		return NULL;
	}

	if (ssl->blocking == 1) {
		ld->ld_status = LDAP_STAT_CONNECTING;
	} else 	{
		ld->ld_status = LDAP_STAT_OK;
		if ((rc = ssl_open(ssl)) != 0) {
			perror("ssl_open");
			return NULL;
		}
	}

	ld->ld_sb.fd = ssl->fd;
	ld->ld_sb.ssl = (void *) ssl;

	return ld;
}	
#endif

LDAP *
ldap_init(char *addr, short port)
{
	struct sockaddr_in	so;
	LDAP			*ld;
	int			rc, fd, nb = 1;

	ld = ldap_make_ldap();

	if ((ld->ld_sb.fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		return NULL;
	}

	if (ioctl(ld->ld_sb.fd, FIONBIO, &nb) < 0) {
		perror("ioctl");
		return NULL;
	}

	bzero((char *) &so, sizeof(so));
	so.sin_family 		= AF_INET;
	so.sin_port		= htons(port);
	so.sin_addr.s_addr	= inet_addr(addr);

	if (rc = connect(ld->ld_sb.fd, (struct sockaddr *) &so, 
		sizeof(so)) < 0) {
		if (errno == EINPROGRESS) {
			ld->ld_status = LDAP_STAT_CONNECTING;
			return ld;
		}

		perror("connect");
		close(ld->ld_sb.fd);
		free(ld);
		return NULL;
	}

	ld->ld_status = LDAP_STAT_OK;
	return ld;
}

int
ldap_simple_bind(LDAP *ld, char *dn, char *pw, int ver)
{
	struct obj	mes_o, id_o, pop_o, ver_o, dn_o, auth_o;
	obj_string	dn_s, pw_s;
	int		rc;
	long		id;

	id = ld->ld_id;
	ld->ld_id = ld->ld_id == LDAP_MAX_INT ? 0 : ld->ld_id + 1;

	dn_s.data = dn;
	if (dn) dn_s.len = strlen(dn);
	else 	dn_s.len = 0;

	pw_s.data = pw;
	if (pw) pw_s.len = strlen(pw);
	else 	pw_s.len = 0;

	ber_make_obj(&mes_o, LBER_CONST | LBER_TYPE_SEQ, 0, &id_o, 0);
	ber_make_obj(&id_o, LBER_TYPE_INT, 0, &id, &pop_o);
	ber_make_obj(&pop_o, LBER_APP | LBER_CONST | LDAP_OP_BIND_REQUEST,
		LBER_TYPE_SEQ, &ver_o, 0);

	ber_make_obj(&ver_o, LBER_TYPE_INT, 0, &ver, &dn_o);
	ber_make_obj(&dn_o, LBER_TYPE_OCTET, 0, &dn_s, &auth_o);
	ber_make_obj(&auth_o, LBER_CONTEXT | LDAP_AUTH_SIMPLE, LBER_TYPE_OCTET,
		&pw_s, 0);

	rc = ber_write(ld->ld_sb, &mes_o);

	return id;
}
