/* USMID @(#)tcp/usr/etc/nettest/nettest.h	61.0	09/03/90 19:11:52 */
#define	CHUNK	4096
#define	NCHUNKS	100

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/times.h>
#include <sys/param.h>
#define	UNIXPORT	"un_socket"
#define	PORTNUMBER	(IPPORT_RESERVED + 42)
