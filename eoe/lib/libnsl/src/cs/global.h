/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libnsl:cs/global.h	1.2.8.3"

#define MSGSIZE		512	/* size of scratch buffer */
#define STRSIZE		128
#define LRGBUF		5120
#define HOSTSIZE 	256
#define ALARMTIME 	60
#define NOTNULLPTR 	1
#define NULLPTR 	0

/* in order for DUMP to work properly "x" must be of the form
 *      (msg, "string_with_printf_options", [args])
 * where args is optional
 */
#define DUMP(x) if (Debugging) {sprintf x; debug(msg);}
#define CS_LOG(x) {sprintf x; log(msg); if (Debugging) debug(msg);}

struct errmsg{
	char *e_str;	/* error string */
	int e_exitcode; /* and associated exit status */
};

struct	status{
	int cs_error; 	
	int sys_error; 	
	int dial_error;
	int tli_error;
	int unused[10];
};

struct con_request{
	char 	netpath[STRSIZE];
	char 	host[HOSTSIZE];
	char 	service[STRSIZE];
	int	option;
	int	nb_set;
	int	maxlen;
	int	len;
	char 	buf[STRSIZE];
	int	nc_set;
	char	netid[STRSIZE];
	unsigned long	semantics;
	unsigned long	flag;
	char	protofmly[STRSIZE];
	char	proto[STRSIZE];
};

struct schemelist {
	char 	*i_host;
	char	*i_service;
	char	*i_netid;
	char	*i_scheme;
	char	*i_role;
	struct schemelist *i_next;
};

struct dial_request{
	char netpath[STRSIZE];
	int termioptr;
	unsigned long	c_iflag;	/* start TERMIO structure fields */
	unsigned long	c_oflag;
	unsigned long	c_cflag;
	unsigned long	c_lflag;
	char	c_line;
	int	c_ccptr;
	char	c_cc[STRSIZE];		/* end TERMIO structure fields */
	int	baud;
	int	speed;
	int 	lineptr;
	char	line[STRSIZE];
	int 	telnoptr;
	char	telno[STRSIZE];
	int	modem;
	int 	deviceptr;
	int 	serviceptr;
	char	service[STRSIZE];	/* start CALL_EXT structure fields */
	int 	classptr;
	char	class[STRSIZE];
	int 	protocolptr;
	char	protocol[STRSIZE];
	int 	reservedptr;
	char	reserved1[STRSIZE];
	int	version;
	int	dev_len;		/* unused */
};
struct schemeinfo {
	char *s_name;
	int   s_flag;
};


/*
 *	Files accessed
 */

#define CSPIPE		"/etc/.cs_pipe"
#define ROOT		"/"
#define LOGFILE		"/var/adm/log/cs.log"
#define DBGFILE		"/var/adm/log/cs.debug"	
#define AUTHFILE	"/etc/cs/auth"	
#define CACHEFILE	"/var/tmp/.cscache"
#define SERVEALLOW 	"/etc/iaf/serve.allow"
#define SERVEALIAS 	"/etc/iaf/serve.alias"
#define LIDFILE		"/etc/idmap/attrmap/LIDAUTH.map"


/*
 *	Error conditions of cs_connect().
 */

#define	CS_NO_ERROR	0
#define	CS_SYS_ERROR	1
#define CS_DIAL_ERROR	2
#define	CS_MALLOC	3
#define	CS_AUTH		4
#define CS_LIDAUTH	5
#define	CS_CONNECT	6
#define	CS_INVOKE	7
#define	CS_SCHEME	8
#define	CS_TRANSPORT	9
#define	CS_PIPE		10
#define	CS_FATTACH	11
#define	CS_CONNLD	12
#define	CS_FORK		13
#define CS_CHDIR	14
#define	CS_SETNETPATH	15
#define	CS_TOPEN	16
#define	CS_TBIND	17
#define	CS_TCONNECT	18
#define	CS_TALLOC	19
#define	CS_MAC		20
#define CS_DAC		21
#define	CS_TIMEDOUT	22
#define CS_NETPRIV	23
#define CS_NETOPTION	24
#define CS_NOTFOUND	25


#define AU_IMPOSER	0
#define AU_RESPONDER	1
#define NO_EXIT		0
#define CS_EXIT		1
#define DIAL_REQUEST	0	
#define TLI_REQUEST	1	
#define CS_READ_AUTH	2	
