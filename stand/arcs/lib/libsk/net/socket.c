#ident	"lib/libsk/net/socket.c:  $Revision: 1.9 $"

/*
 * socket.c -- prom socket routines
 */

#include <sys/types.h>
#include <net/in.h>
#include <net/socket.h>
#include <net/arp.h>
#include <net/mbuf.h>
#include <libsc.h>
#include <libsk.h>

struct so_table _so_table[NSO_TABLE];

#ifndef NULL
#define	NULL	0
#endif

void
_init_sockets(void)
{
	bzero(_so_table, sizeof(_so_table));
}

/*
 * _get_socket
 *
 * Find a socket bound to the given UDP port number.
 *
 * If not found, allocate a new socket and bind it
 * to the given port.
 */
struct so_table *
_get_socket(u_short port)
{
	register struct so_table *st;

	/*
	 * Search for an existing socket bound to the port.
	 * Bump the count and return if found.
	 */
	if (st = _find_socket(port)) {
		st->st_count++;
		return(st);
	}

	/*
	 * Try to allocate an unused socket.
	 */
	for (st = _so_table; st < &_so_table[NSO_TABLE]; st++)
		if (st->st_count <= 0) {
			st->st_mbuf = 0;
			st->st_count = 1;
			st->st_udpport = port;
			return(st);
		}
	return(NULL);
}

struct so_table *
_find_socket(u_short port)
{
	register struct so_table *st;

	for (st = _so_table; st < &_so_table[NSO_TABLE]; st++) {
		if (st->st_count > 0 && st->st_udpport == port)
			return(st);
	}
	return(NULL);
}

void
_free_socket(unsigned int sx)
{
	register struct so_table *st;
	struct mbuf *m;

	if (sx >= NSO_TABLE) {
		printf("_free_socket: %u is not a valid socket index\n", sx);
		return;
	}
	st = &_so_table[sx];
	if (--st->st_count < 0) {
		st->st_count = 0;
		printf("_free_socket: ref count < 0\n");
	}
	/*
	 * If the socket is unreferenced, free any mbufs that
	 * are hanging from it.
	 */
	if (st->st_count == 0) {
		while (m = _so_remove(st))
			_m_freem(m);
	}

}

int
_so_append(struct so_table *st, struct sockaddr *src_addr, struct mbuf *m0)
{
	register struct mbuf *m;
	register int nmbufs;

	m0->m_srcaddr = *src_addr;
	m0->m_act = NULL;
	if ((m = st->st_mbuf) == NULL)
		st->st_mbuf = m0;
	else {
		nmbufs = 1;
		while (m->m_act) {
			m = m->m_act;
			nmbufs++;
		}
		if (nmbufs >= MAXMBUFS) {
			_m_freem(m0);
			return(0);
		} else
			m->m_act = m0;
	}
	return(1);
}

struct mbuf *
_so_remove(struct so_table *st)
{
	struct mbuf *m;

	if (m = st->st_mbuf)
		st->st_mbuf = m->m_act;
	return (m);
}
