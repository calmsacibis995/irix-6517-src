/*
 *  @(#) $Id: netutil.c,v 1.3 1997/11/14 07:36:02 mwang Exp $
 */
/*
 *	netutil.c -- Utility routines for network operations
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/resource.h>
#include <ctype.h>
#include <netinet/in.h>
#include <netdb.h>
#include <net/if.h>
#include <sys/sockio.h>

/*  Remove any final suffix .<chars> (if any) from the end of a given
 *    string by storing a NUL on top of the dot.  Return a pointer to
 *    the beginning of <chars>, or NULL if there is no suffix.
 */
char *rmsuffix(cp)
    char *cp;
{
        char *tp, *ep;

        tp = ep = cp + strlen(cp) - 1;
        while (*tp != '.' && tp >= cp) tp--;
        if (*tp != '.') return(NULL);
        *tp = '\0';
    return((char *)tp+1);
}

    
  /*   Host name lookup...
   *
   *   Resolve host name using domain resolver or whatever.  If
   *   argument is dotted-decimal string (first char is digit), just
   *   convert it without lookup (we don't want to fail just because
   *   the INADDR database is not complete).  
   *
   */
u_long resolve_name( namep )
    char *namep;
{
    struct hostent *hp ;
    
    if (isdigit(*namep))
        return((u_long) inet_addr(namep)) ;
    else if (NULL == (hp =  gethostbyname( namep )))
        return(0) ;         
    return( *( u_long *) hp->h_addr) ;
}


/* This proc capitalizes leading component of string in place */
void
host2upper(str)
	char *str;
	{
	while (*str && *str != '.') {
		*str = toupper(*str);
		str++;
	}
}

/* String copy routine that truncates if necessary to n-1 characters, 
 *   but always null-terminates.
 */
char *strtcpy(s1, s2, n)
char *s1, *s2;
int n;
    {
    strncpy(s1, s2, n);
    *(s1+n-1) = '\0';
    return(s1);
}

/* This routine returns the system's default interface ip address. 
 *
 */
u_long
default_if_addr(int kern_socket) {
        char            ifbuf[5000];
        struct ifconf   ifc;
        struct ifreq   *ifr;
        char		*cifr, *cifrlim;
        int if_num = 0;

	if (kern_socket <= 0) {

		if ((kern_socket = socket(AF_INET, SOCK_DGRAM, 0))< 0) {
			perror("Socket Error") ;
			exit(1) ;
		}
	}

        ifc.ifc_buf = ifbuf;
        ifc.ifc_len = sizeof(ifbuf);
        if (ioctl(kern_socket, SIOCGIFCONF, (char *) &ifc) < 0) {
                perror("ioctl SIOCGIFCONF");
                exit(1);
        }

        ifr = ifc.ifc_req;
        cifr = (char *)ifr;
        cifrlim = cifr + ifc.ifc_len;
        while(cifr < cifrlim) {
                if (strcmp(ifr->ifr_name, "lo0") 
#ifdef NET2_STYLE_SIOCGIFCONF
			&& ifr->ifr_addr.sa_family == AF_INET
#endif
			) {
                	return( ((struct sockaddr_in *) &
				(ifr->ifr_addr))->sin_addr.s_addr);
		}
#ifndef NET2_STYLE_SIOCGIFCONF
                cifr += sizeof(struct ifreq);
#else
                cifr += offsetof(struct ifreq, ifr_addr) + ifr->ifr_addr.sa_len;
#endif
                ifr = (struct ifreq *)cifr;
        }
        return (0);
}

