/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libnsl:cs/csi.c	1.2.9.1"
#ifdef __STDC__
#ifndef _LIBNSL_ABI
        #pragma weak read_status	= _read_status
        #pragma weak write_dialrequest	= _write_dialrequest
        #pragma weak cs_connect = _cs_connect
        #pragma weak cs_perror	= _cs_perror
        #pragma weak dial 	= _dial
        #pragma weak undial 	= _undial
#endif /* _LIBNSL_ABI */
#endif

#include <libc_synonyms.h>
#include <libnsl_synonyms.h>

#include <stdlib.h>
#include <tiuser.h>
#include <fcntl.h>
#include <netconfig.h>
#include <netdir.h>
#include <stdio.h>
#include <stropts.h>
#include <termios.h>
#include <sys/types.h>
#include <pfmt.h>
#include <locale.h>
#include <dial.h>
#include "global.h"
#include <poll.h>
#include <string.h> 
#include <cs.h>
#include <errno.h>

#ifdef	DEBUG_ON
#define DEBUG(x) fprintf x
#else
#define DEBUG(x) 
#endif

static	struct 	con_request	conrequest;
static 	struct 	strrecvfd 	recvfd;
static	int 	circuitfd;		/* fd into service connect  */
static	char	*netpath;
static 	CALL	Call;
static 	CALL	*Callp=&Call;

	int 	read_status();
static 	int	write_request();
static 	CALL	*read_dialrequest();

/*
 *	cs_connect() attempts to establish an authenticated connection
 *	to the service on the specified host.  cs_connect passes a 
 *	host-service structure over a mounted streams pipe to the
 *	CS daemon.  The CS daemon services the request and passes
 *	a status/error structure, and an fd, if the connection was
 *	sucessful, back to the waiting cs_connect().
 */

int
cs_connect(host, service, cs_opt, error)
char 	*host;
char 	*service;
struct 	csopts *cs_opt;
int	*error;
{
	int 	i;				/* counter */
	int	requestype = TLI_REQUEST;	/* request type */
	struct 	netbuf    *nbp;			/* temp pointer */
	struct 	netconfig *ncp;			/* temp pointer */
        char    *where=(char *)&requestype;
        int     reqsize = sizeof(int);
	
	*error = CS_NO_ERROR;

	/* Copy arguments to con_request structure */
	memset(&conrequest, NULL, sizeof(struct con_request));
	if ((netpath = getenv(NETPATH)) != NULL) 
		strcpy(conrequest.netpath,netpath); 
	strcpy(conrequest.host,host);
	strcpy(conrequest.service,service);
	if (cs_opt != NULL) {
		conrequest.option = cs_opt->nd_opt;
		nbp = cs_opt->nb_p;
		if (nbp != NULL) {
			conrequest.nb_set = TRUE;
			conrequest.maxlen = nbp->maxlen;
			conrequest.len = nbp->len;
			(void) memcpy(conrequest.buf,
				     nbp->buf,
				     nbp->len); 
		}
		ncp = cs_opt->nc_p;
		if (ncp != NULL) {
			conrequest.nc_set = TRUE;
			if (ncp->nc_netid) 
				strncpy(conrequest.netid,
					ncp->nc_netid,STRSIZE);
			conrequest.semantics = ncp->nc_semantics;
			conrequest.flag = ncp->nc_flag;
			if (ncp->nc_protofmly) 
				strncpy(conrequest.protofmly,
					ncp->nc_protofmly,STRSIZE);
				conrequest.nc_set = TRUE;
			if (ncp->nc_proto) 
				strncpy(conrequest.proto,
					ncp->nc_proto,STRSIZE);
		}
	}

	/* Open named stream to create a unique connection between the
	 * CS library interface and the CS daemon.
	 */

	if ((circuitfd = open(CSPIPE, O_RDWR)) == -1) {
		*error = CS_SYS_ERROR;	
		return(-1);
	}

	/* Has stream to cs daemon been setup? */
	if ((i=isastream(circuitfd)) == 0){
		*error = CS_SYS_ERROR;	
		return(-1);
	}

	/* Inform daemon a TLI type connection is requested */
        while ((i=write(circuitfd, where, reqsize)) != reqsize) {
                where = where + i;
                reqsize = reqsize -i;
                if (i == -1) {
			*error = CS_SYS_ERROR;	
			return(-1);
		}
        }


	/*	Write connection request to daemon	*/	
	if ((*error = write_request()) != CS_NO_ERROR) {
		return(-1);
	}


	/*	Read result of connection attempt from daemon	*/
	if ((*error = read_status()) != CS_NO_ERROR) {
	  	DEBUG((stderr,"csi:read_status return <%d>\n",*error));
		return(-1);
	}

	/*
	*	Receive file descriptor from daemon if no error
	* 	has been made while making the connection.
	*/

	if (ioctl(circuitfd, I_RECVFD, &recvfd) == 0) {
		return(recvfd.fd);
	} else {
	  	DEBUG((stderr,"csi: ioctl error:errno <%d>\n",errno));
		*error = CS_SYS_ERROR;	
		return(-1);
	}
}





/*
 *	dial() attempts to establish an authenticated connection
 *	to the service on the specified host.  dial() passes a 
 *	dial request structure over a mounted streams pipe to the
 *	CS daemon. The dialrequest structure contains all dial data
 *	from the old dial interface plus host and service information in
 *	the CALL_EXT structure. The cs daemon processes the request
 *	and passes a status/error structure, and an fd, if the connection
 *	was sucessful, back to the waiting dial().  dial() uses it's
 *	own old interface for error handling and debugging.
*/

int
dial(i_call)
CALL	i_call;
{
	int 	i;
	int	requestype = DIAL_REQUEST;	/* request type */
	int	rval;
        char    *where=(char *)&requestype;
        int     reqsize = sizeof(int);
	
		
	/* Open named stream to create a unique connection between the
	*  CS library interface and the CS daemon.
	*/

	if ((circuitfd= open(CSPIPE, O_RDWR)) == -1) {
		return(CS_PROB);
	}
	
	if ((i=isastream(circuitfd)) == 0) {
		return(CS_PROB);
	}

	/* Inform daemon a dial type connection is requested */

        while ((i=write(circuitfd, where, reqsize)) != reqsize) {
                where = where + i;
                reqsize = reqsize -i;
                if (i == -1)
			return(CS_PROB);
        }

	if ((i=write_dialrequest(circuitfd,i_call)) == -1)
		return(CS_PROB);
		
	if ((rval = read_status()) == CS_DIAL_ERROR) 
		/* dial error codes are held in the fd */
		return(recvfd.fd);
	else if (rval != CS_NO_ERROR) 
		return(rval);
	else {
		if (ioctl(circuitfd, I_RECVFD, &recvfd) != 0)
			return(CS_PROB);
		Callp = read_dialrequest(circuitfd);
		/* check Callp for successful return, but don't
		 * use it because it points to garbage. Use the
		 * global Call instead.
		 */
		if (Callp && i_call.device)
			((CALL_EXT *)i_call.device)->protocol =
				((CALL_EXT *)Call.device)->protocol;
		return(recvfd.fd);
	}
}

void
undial(int fd)
{
	struct termio ttybuf;

	if(fd >= 0) {
		if (ioctl(fd, TCGETA, &ttybuf) == 0) {
			if (!(ttybuf.c_cflag & HUPCL)) {
				ttybuf.c_cflag |= HUPCL;
				(void) ioctl(fd, TCSETAW, &ttybuf);
			}
		}
		(void) close(fd);
	}
	return;
}



/*
*	Writes a con_request structure over the streams pipe to the 
*	CS daemon
*/	

static int
write_request()
{
	int	i=0;
        char    *where=(char *)&conrequest;
        int     reqsize = sizeof(struct con_request);

        while ((i=write(circuitfd, where, reqsize)) != reqsize) {
                where = where + i;
                reqsize = reqsize -i;
                if (i == -1)
                        return(CS_SYS_ERROR);
        }

	return(CS_NO_ERROR);
}


/*	Reads a status structure generated by the CS daemon
*	If the cs daemon exits before writing back a status
*	structure read_status will detect the POLLHUP and
*	timeout.  It is not recommended for read_status() or
*	cs_connect(), or dial()  to timeout (via alarm) since 
*	a connection request, especially a dial request,  can 
*	take a very long time to process.  There is no correct 
*	timeout value.
*/

int
read_status()
{
	struct 	pollfd pinfo;	/* poll only 1 stream to CS daemon */
	struct  status	*sp;
        int     reqsize = sizeof(struct status);
	int	i=0;

	if ((sp = (struct status *)
	     malloc(sizeof(struct status))) == NULL) {
		return(CS_MALLOC);
	}

	DEBUG((stderr, "csi:read_status: Waiting to read status\n"));
		
	pinfo.fd = circuitfd;
	pinfo.events = POLLIN | POLLHUP;
	pinfo.revents = 0;

	if (poll(&pinfo, 1, -1) < 0) {
		/* poll error */
		return(CS_TIMEDOUT);
	} else if (pinfo.revents & POLLIN) {
		/* incoming request or message */
		if ((i=read(circuitfd,sp,reqsize)) != reqsize) {
			DEBUG((stderr,"read in read_status failed"));
	                if (i == -1)
				return(CS_SYS_ERROR);
			return(CS_TIMEDOUT);
		}
	  	DEBUG((stderr,"csi: cs_error<%d>\n",sp->cs_error));
	  	DEBUG((stderr,"csi: sys_error<%d>\n",sp->sys_error));
	  	DEBUG((stderr,"csi: dial_error<%d>\n",sp->dial_error));
	  	DEBUG((stderr,"csi: tli_error<%d>\n",sp->tli_error));
		errno = sp->sys_error;
		/*
		t_errno = sp->tli_error; 
		*/
		if (sp->cs_error == CS_DIAL_ERROR) 
			/* dial returns it's error code in the fd */
			recvfd.fd = sp->dial_error;
		return(sp->cs_error);

	} else {
		DEBUG((stderr, "POLL revents=0x%x\n", pinfo.revents));
	  	DEBUG((stderr,"csi:read_status FAILED: nothing read"));

		/* WRONG?  Effectively this is what Nancy did */
		return(CS_TIMEDOUT);
	}
}



/*
 *	Print out returned error value from cs_connect() or cs_dial()
 */
void
cs_perror(str, err)
char *str;
int err;
{
	(void)setlocale(LC_ALL, "");
	(void)setcat("uxnsu");
	setlabel("UX:cs");
	switch (err) {
	case CS_NO_ERROR:
		(void)pfmt(stderr, MM_ERROR, ":134:%s: No Error\n",str);
		break;
	case CS_SYS_ERROR:
		(void)pfmt(stderr, MM_ERROR, ":135:%s: System Error: %s\n",str, strerror(errno));
		break;
	case CS_DIAL_ERROR:
		(void)pfmt(stderr, MM_ERROR, ":136:%s: Dial error\n",str);
		break;
	case CS_MALLOC:
		(void)pfmt(stderr, MM_ERROR, ":137:%s: No Memory\n",str);
		break;
	case CS_AUTH:
		(void)pfmt(stderr, MM_ERROR, 
		      ":139:%s: Authentication scheme specified by server is not acceptable\n",str);
		break;
	case CS_CONNECT:
		(void)pfmt(stderr, MM_ERROR, 
		      ":140:%s: Connection to service failed\n",str);
		break;
	case CS_INVOKE:
		(void)pfmt(stderr, MM_ERROR, 
		      ":141:%s: Error in invoking authentication scheme\n",str);
		break;
	case CS_SCHEME:
		(void)pfmt(stderr, MM_ERROR, 
		      ":142:%s: Authentication scheme unsucessful\n",str);
		break;
	case CS_TRANSPORT:
		(void)pfmt(stderr, MM_ERROR, 
		      ":143:%s: Could not obtain address of service over any transport\n",str);
		break;
	case CS_PIPE:
		(void)pfmt(stderr, MM_ERROR, 
		      ":144:%s: Could not create CS pipe\n",str);
		break;
	case CS_FATTACH:
		(void)pfmt(stderr, MM_ERROR, 
		      ":145:%s: Could not mount remote stream to CS pipe\n",str);
		break;
	case CS_CONNLD:
		(void)pfmt(stderr, MM_ERROR, 
		      ":146:%s: Could not push CONNLD\n",str);
		break;
	case CS_FORK:
		(void)pfmt(stderr, MM_ERROR, 
		      ":147:%s: Could not fork CS child request\n",str);
		break;
	case CS_CHDIR:
		(void)pfmt(stderr, MM_ERROR, ":138:%s: Could not chdir\n",str);
		break;
	case CS_SETNETPATH:
		{
		char *path;
		path = getenv(NETPATH);
		(void)pfmt(stderr, MM_ERROR, 
		      ":148:%s: host/service not found over available transport %s\n",
		      str, path==NULL? "": path);
		}
		break;
	case CS_TOPEN:
		(void)pfmt(stderr, MM_ERROR, 
		      ":149:%s: TLI failure: t_open failed\n",str);
		break;
	case CS_TBIND:
		(void)pfmt(stderr, MM_ERROR, 
		      ":150:%s: TLI failure: t_bind failed\n",str);
		break;
	case CS_TCONNECT:
		(void)pfmt(stderr, MM_ERROR, 
		      ":151:%s: TLI failure: t_connect failed\n",str);
		break;
	case CS_TALLOC:
		(void)pfmt(stderr, MM_ERROR, 
		      ":152:%s: TLI failure: t_alloc failed\n",str);
		break;
	case CS_MAC:
		(void)pfmt(stderr, MM_ERROR, 
	              ":153:%s: MAC check failure or Secure Device access denied\n",str);
		break;
	case CS_DAC:
		(void)pfmt(stderr, MM_ERROR, 
	              ":154:%s: DAC check failure or Secure Device access denied\n",str);
		break;
	case CS_TIMEDOUT:
		(void)pfmt(stderr, MM_ERROR, 
		      ":155:%s: Connection attempt timed out\n",str);
		break;
	case CS_NETPRIV:
		(void)pfmt(stderr, MM_ERROR, 
		      ":156:%s: Privileges not correct for requested network options\n",str);
		break;
	case CS_NETOPTION:
		(void)pfmt(stderr, MM_ERROR, 
		      ":157:%s: Netdir option incorrectly set in csopts struct\n",str);
		break;
	case CS_NOTFOUND:
		(void)pfmt(stderr, MM_ERROR, 
		      ":158:%s: Service not found in server's _pmtab\n",str);
		break;
	case CS_LIDAUTH:
		(void)pfmt(stderr, MM_ERROR, 
		      ":159:%s: Connection not permitted by LIDAUTH.map\n",str);
		break;
	default:
		(void)pfmt(stderr, MM_ERROR, 
		      ":160:%s:  cs_perror error in message reporting\n",str);
		break;
	}
}

/*
 *	Write a dialrequest structure over streams pipe to cs daemon.
 *	Account for any NULL pointers that may be in CALL structure
 *	and NETPATH environment variable.
*/
int
write_dialrequest(fd, Call)
int	fd;
CALL	Call;
{
	struct dial_request	dialrequest;
	struct dial_request	*dialrequestp=&dialrequest;
	int	i;

	if (Call.attr != NULL){
		dialrequestp->termioptr=NOTNULLPTR;
		dialrequestp->c_iflag=Call.attr->c_iflag;	
		dialrequestp->c_oflag=Call.attr->c_oflag;	
		dialrequestp->c_cflag=Call.attr->c_cflag;	
		dialrequestp->c_lflag=Call.attr->c_lflag;	
		dialrequestp->c_line=Call.attr->c_line;	
		strcpy(dialrequestp->c_cc,(char *)Call.attr->c_cc); 
	}else
		dialrequestp->termioptr=NULLPTR;

	if (Call.line != NULL){
		dialrequestp->lineptr=NOTNULLPTR;
		strcpy(dialrequestp->line,Call.line); 
	}else
		dialrequestp->lineptr=NULLPTR;


	if (Call.telno != NULL){
		dialrequestp->telnoptr=NOTNULLPTR;
		strcpy(dialrequestp->telno,Call.telno); 
	}else
		dialrequestp->telnoptr=NULLPTR;

	if (Call.device != NULL){
		dialrequestp->deviceptr=NOTNULLPTR;

		if (((CALL_EXT	*)Call.device)->service != NULL){ 
			strcpy(dialrequestp->service,((CALL_EXT	*)Call.device)->service); 
			dialrequestp->serviceptr=NOTNULLPTR;
		}else
			dialrequestp->serviceptr=NULLPTR;

		if (((CALL_EXT	*)Call.device)->class != NULL){ 
			dialrequestp->classptr=NOTNULLPTR;
			strcpy(dialrequestp->class,((CALL_EXT	*)Call.device)->class); 
		}else
			dialrequestp->classptr=NULLPTR;

		if (((CALL_EXT	*)Call.device)->protocol != NULL){ 
			dialrequestp->protocolptr=NOTNULLPTR;
			strcpy(dialrequestp->protocol,((CALL_EXT	*)Call.device)->protocol); 
		}else
			dialrequestp->protocolptr=NULLPTR;

	}else
		dialrequestp->deviceptr=NULLPTR;

	dialrequestp->version=1;	
	dialrequestp->baud=Call.baud;	
	dialrequestp->speed=Call.speed;	
	dialrequestp->modem=Call.modem;	
	dialrequestp->dev_len=Call.dev_len;	
	strcpy(dialrequestp->netpath, getenv(NETPATH));
		
#ifdef DEBUG_ON
	fprintf(stderr, "write_dialrequest:writing request to fd:<%d>\n",fd);
	fprintf(stderr, "dialrequestp->baud<%d>\n",dialrequestp->baud);
	fprintf(stderr, "dialrequestp->speed<%d>\n",dialrequestp->speed);
	fprintf(stderr, "dialrequestp->modem<%d>\n",dialrequestp->modem);
	fprintf(stderr, "dialrequestp->dev_len<%d>\n",dialrequestp->dev_len);

	if (dialrequestp->termioptr != NULLPTR) {	
	fprintf(stderr, "dialrequestp->c_iflag<%d>\n",dialrequestp->c_iflag);
	fprintf(stderr, "dialrequestp->c_oflag<%d>\n",dialrequestp->c_oflag);
	fprintf(stderr, "dialrequestp->c_cflag<%d>\n",dialrequestp->c_cflag);
	fprintf(stderr, "dialrequestp->c_lflag<%d>\n",dialrequestp->c_lflag);
	fprintf(stderr, "dialrequestp->c_line<%c>\n",dialrequestp->c_line);
	fprintf(stderr, "dialrequestp->c_cc<%s>\n",dialrequestp->c_cc);
	} else
		fprintf(stderr, "dialrequestp->termio = NULL\n");

	if (dialrequestp->lineptr != NULLPTR) {	
	fprintf(stderr, "dialrequestp->line<%s>\n",dialrequestp->line);
	} else
		fprintf(stderr, "dialrequestp->lineptr = NULL\n");

	if (dialrequestp->telnoptr != NULLPTR) {	
		fprintf(stderr, "dialrequestp->telno<%s>\n",dialrequestp->telno);
	}else
		fprintf(stderr, "dialrequestp->telnoptr = NULL\n");

	if (dialrequestp->deviceptr != NULLPTR) {	

		if (dialrequestp->serviceptr != NULLPTR) {	
			fprintf(stderr, "dialrequestp->service<%s>\n",dialrequestp->service);
		}else
			fprintf(stderr, "dialrequestp->serviceptr = NULL\n");

		if (dialrequestp->classptr != NULLPTR) {	
			fprintf(stderr, "dialrequestp->class<%s>\n",dialrequestp->class);
		}else
			fprintf(stderr, "dialrequestp->classptr = NULL\n");


		if (dialrequestp->protocolptr != NULLPTR) {	
			fprintf(stderr, "dialrequestp->protocol<%s>\n",dialrequestp->protocol);
		}else
			fprintf(stderr, "dialrequestp->protocolptr = NULL\n");

		if (dialrequestp->reserved1 != NULLPTR) {	
			fprintf(stderr, "dialrequestp->reserved1<%s>\n",dialrequestp->reserved1);
		}else
			fprintf(stderr, "dialrequestp->reserved1 = NULL\n");
	
	}else
		fprintf(stderr, "dialrequestp->deviceptr = NULL\n");

#endif

	while ((i=write(fd, dialrequestp, (sizeof(dialrequest)))) != (sizeof(dialrequest))){
		if ( i == -1)
		return(-1);
		}

	return(0);
}

/*
 *	Read a dialrequest structure over streams pipe from cs daemon.
*/
static CALL	*	
read_dialrequest(fd)
int	fd;
{
	CALL	*Callp;
	char	 buff[LRGBUF];
	char	*where=&buff[0];
	int 	 dialsize;
	struct 	 dial_request	*dialrequestp;
	int	 i;
	dialsize = sizeof( struct dial_request );

	while ((i=read(fd, where, dialsize )) != dialsize) {
		where = where + i;
		dialsize = dialsize -i;
		if ( i == -1)
			return(NULL);
	}

	dialrequestp = (struct dial_request *)buff;
	
	if ((Callp =(CALL *)malloc(sizeof(CALL))) == NULL)
		return(NULL);

	if ((Call.attr =(struct termio *)malloc(sizeof(struct termio))) == NULL)
		return(NULL);
	
	if (((Call.device) = (char *)malloc(sizeof(CALL_EXT))) == NULL)
		return(NULL);
	
	Call.baud=dialrequestp->baud;
	Call.speed=dialrequestp->speed;
	Call.modem=dialrequestp->modem;
	Call.dev_len=dialrequestp->dev_len;


	if (dialrequestp->termioptr != NULLPTR) {	
		Call.attr->c_iflag=dialrequestp->c_iflag;
		Call.attr->c_oflag=dialrequestp->c_oflag;
		Call.attr->c_cflag=dialrequestp->c_cflag;
		Call.attr->c_lflag=dialrequestp->c_lflag;
		Call.attr->c_line=(dialrequestp->c_line);
		strcpy((char *)Call.attr->c_cc,dialrequestp->c_cc); 
	} else
		Call.attr=NULL;

	if (dialrequestp->lineptr != NULLPTR) {	
		Call.line=strdup(dialrequestp->line); 
	} else
		Call.line=NULL;

	if (dialrequestp->telnoptr != NULLPTR) {	
		Call.telno=strdup(dialrequestp->telno); 
	} else
		Call.telno=NULL;

	if (dialrequestp->deviceptr != NULLPTR) {	

		if (dialrequestp->serviceptr != NULLPTR) 
			((CALL_EXT *)Call.device)->service =
			      strdup(dialrequestp->service); 
		else
			((CALL_EXT *)Call.device)->service = NULL;

		if (dialrequestp->classptr != NULLPTR) 
			((CALL_EXT *)Call.device)->class =
			      strdup(dialrequestp->class); 
		else
			((CALL_EXT *)Call.device)->class = NULL;


		if (dialrequestp->protocolptr != NULLPTR) 
			((CALL_EXT *)Call.device)->protocol =
			      strdup(dialrequestp->protocol); 
		else
			((CALL_EXT *)Call.device)->protocol=NULL;

		if (dialrequestp->reserved1 != NULLPTR)
			((CALL_EXT *)Call.device)->reserved1 =
			      strdup(dialrequestp->reserved1); 
		else
			((CALL_EXT *)Call.device)->reserved1=NULL;

	} else
		Call.device = NULL;
		
#ifdef DEBUG_ON
	fprintf(stderr, "Call.baud<%d>\n",Call.baud);
	fprintf(stderr, "Call.speed<%d>\n",Call.speed);
	fprintf(stderr, "Call.modem<%d>\n",Call.modem);
	fprintf(stderr, "Call.dev_len<%d>\n",Call.dev_len);

	if (Call.attr != NULL){
		fprintf(stderr, "Call.attr->c_iflag<%d>\n",Call.attr->c_iflag);
		fprintf(stderr, "Call.attr->c_oflag<%d>\n",Call.attr->c_oflag);
		fprintf(stderr, "Call.attr->c_cflag<%d>\n",Call.attr->c_cflag);
		fprintf(stderr, "Call.attr->c_lflag<%d>\n",Call.attr->c_lflag);
		fprintf(stderr, "Call.attr->c_line<%c>\n",Call.attr->c_line);
		fprintf(stderr, "Call.attr->c_cc<%s>\n",Call.attr->c_cc);
	} else
		fprintf(stderr, "Call.attr = NULL\n");

	fprintf(stderr, "Call.line<%s>\n",Call.line?Call.line:"NULL");
	fprintf(stderr, "Call.telno<%s>\n",Call.telno?Call.telno:"NULL");
	if (Call.device != NULL){
		dialrequestp->deviceptr=NOTNULLPTR;
		fprintf(stderr, 
		   "((CALL_EXT *)Call.device)->service<%s>\n",
		    ((CALL_EXT *)Call.device)->service? 
		    ((CALL_EXT *)Call.device)->service: "NULL");

		fprintf(stderr, 
		   "((CALL_EXT *)Call.device)->class<%s>\n",
		    ((CALL_EXT *)Call.device)->class?
		    ((CALL_EXT *)Call.device)->class: "NULL");

		fprintf(stderr, 
		   "((CALL_EXT *)Call.device)->protocol<%s>\n",
		    ((CALL_EXT *)Call.device)->protocol?
		    ((CALL_EXT *)Call.device)->protocol: "NULL");

		fprintf(stderr, 
		   "((CALL_EXT *)Call.device)->reserved1<%s>\n",
		    ((CALL_EXT *)Call.device)->reserved1?
		    ((CALL_EXT *)Call.device)->reserved1: "NULL");
	} else
		fprintf(stderr, "((CALL_EXT *)Call.device)= NULL\n");
#endif	
	return(Callp);
}
