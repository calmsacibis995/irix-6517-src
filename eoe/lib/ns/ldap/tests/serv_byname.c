#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>


char STCP[][30]={ "discard", "ftp", "telnet", "smtp", "mtp", "http", "link",\
"supdup", "hostnames", "pop-3", "nntp", "tempo", "remotefs", "krbupdate",\
"kshell", "x-server", "hp16500", "mbatchd", "sbatchd", "ugplot", "cvspserver",\
"p4d", "echo", "qotd", "whois", "rje", "x400", "pop-2", "loc-srv", "exec",\
"login", "courier", "conference", "ingreslock", "ta-rauth", "kerberos",\
"sgi-dgl", "sgi-arrayd", "wn-http", "phonebook", "fax", "webster",\
"machqueuing", "res", "acscln", "tcpmux", "netstat", "finger", "sftp",\
"printer", "kpasswd", "eklogin", "acsalt", "systat", "ftp-data", "iso-tsap",\
"csnet-ns", "sunrpc", "uucp-path", "imap2", "shell", "netnews", "klogin",\
"oscssd", "acs", "netlisp", "daytime", "chargen", "time", "name", "domain",\
"x400-snd", "auth", "uucp", "sgi_iphone", "zycmgr" };

char SUDP[][30]={ "echo", "ntp", "talk", "netwall", "kerberos",\
"sgi-objectserver", "sgi-mekton4", "radius", "rlp", "domain", "loc-srv",\
"snmp-trap", "xdmcp", "who", "sgi-mekton1", "sgi-mekton3", "daytime",\
"chargen", "time", "timed", "sgi-arena", "sgi-directoryserver", "sgi-mekton0",\
"radacct", "bootpc", "tftp", "sunrpc", "biff", "syslog", "route", "sgi-bznet",\
"sgi-oortnet", "sgi-mekton2", "sbm-comm", "linkage-license", "lim", "discard",\
"bootp", "erpc", "snmp", "ntalk", "albd", "sgi-dogfight" };

void main(){

  int			n, i, j;
  struct servent *	s;

  n = sizeof( STCP )/30;

  for( i = 0; i < n; i++ ){
    
    fprintf( stdout, "main: TCP(%s)\n", STCP[ i ] );
    fflush( stdout );

    if(( s = getservbyname( STCP[ i ], "tcp"))==NULL){
      
      fprintf( stdout, "main: getservbyname() failed, errno=(%d,>%s<)\n",\
	       errno, strerror( errno ) );

    }else{

      fprintf( stdout, "main: s_name->%s<\n", s->s_name );
      fprintf( stdout, "main: s_port->%d<\n", s->s_port );
      fprintf( stdout, "main: s_proto->%s<\n", s->s_proto );
      for( j = 0; s->s_aliases[ j ] != NULL; j++ ){

	fprintf( stdout, "main: s_aliases[%d]->%s<\n", j,\
		 s->s_aliases[ j ] );

      }
      
    }

  }

  n = sizeof( SUDP )/30;

  for( i = 0; i < n; i++ ){
    
    fprintf( stdout, "main: UDP(%s)\n", SUDP[ i ] );
    fflush( stdout );

    if(( s = getservbyname( SUDP[ i ], "udp"))==NULL){
      
      fprintf( stdout, "main: getservbyname() failed, errno=(%d,>%s<)\n",\
	       errno, strerror( errno ) );

    }else{

      fprintf( stdout, "main: s_name->%s<\n", s->s_name );
      fprintf( stdout, "main: s_port->%d<\n", s->s_port );
      fprintf( stdout, "main: s_proto->%s<\n", s->s_proto );
      for( j = 0; s->s_aliases[ j ] != NULL; j++ ){

	fprintf( stdout, "main: s_aliases[%d]->%s<\n", j,\
		 s->s_aliases[ j ] );

      }
      
    }

  }

}
