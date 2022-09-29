
/*
 * rtap -- RealTime Application Program: Test program for RSVP
 *
 *		rtap_cmds.c: Body of program
 *
 *
 *  This file contains the common elements are rtap that are needed
 *  when rtap is either integral with rsvpd or a separate process.
 *  Thus, it includes command parsing, command execution, and the
 *  RSVP API rapi_xxx interface calls.
 *
 *  Separate process: rtap = rtap_main.c + rtap_cmds.c
 *
 *  Integral with rsvpd: rtap = rsvp_rtap.c + rtap_cmds.c +
 *						<hooks in rsvp_main.c>
 * 
 * 		Written by Bob Braden, ISI
 */

/*******************************************************************

  Copyright (c) 1996 by the University of Southern California
  All rights reserved.

  Permission to use, copy, modify, and distribute this software and its
  documentation in source and binary forms for any purpose and without
  fee is hereby granted, provided that both the above copyright notice
  and this permission notice appear in all copies. and that any
  documentation, advertising materials, and other materials related to
  such distribution and use acknowledge that the software was developed
  in part by the University of Southern California, Information
  Sciences Institute.  The name of the University may not be used to
  endorse or promote products derived from this software without
  specific prior written permission.

  THE UNIVERSITY OF SOUTHERN CALIFORNIA makes no representations about
  the suitability of this software for any purpose.  THIS SOFTWARE IS
  PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

  Other copyrights might apply to parts of this software and are so
  noted when applicable.

********************************************************************/

#include "config.h"  /* must come first */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <math.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <cap_net.h>

#include "rapi_lib.h"
#include "rapi_err.h"
#include "rsvp.h"
#include "rsvp_api.h"	/* To include management interface */

#ifdef INCLUDE_SYSTEM_H
#include "system.h"
int printf(), strcasecmp();
#endif

#include "Pm_parse.h"

#define show_help(op)  printf(cmd_table[op].cmd_help)
#define RAPI_OBJLEN(objp)  ( (*(int *) objp)? *(int *)objp : sizeof(int))

/* Define standard header for an area to hold vector of variable-length
 *      flowspecs, filterspecs, etc.
 */
typedef struct {
        short   area_size;      /* total size of area */
        short   area_offset;    /* Offset of next available space in area */
        int     area_cnt;       /* Number of objects in area */
} area_t;

area_t  *init_Area(area_t **, int size);
char    *Area_offset(area_t *);
void    Cat_Area(area_t *orig, rapi_hdr_t *new);
#define Area_Data(cast, p)   (cast ((p)? (rapi_hdr_t *) (p)+1: NULL))
        
/*
 * Forward & External Declarations
 */
void		rtap_sleep(int), rtap_sleepdone();
void		rapi_fmt_flowspec(rapi_flowspec_t *, char *, int);
void		rapi_fmt_tspec(rapi_tspec_t *, char *, int);
void		rapi_fmt_adspec(rapi_adspec_t *, char *, int);
void		rapi_fmt_filtspec(rapi_filter_t *, char *, int);
void		Send_data(void);
int		Do_Command(char *, FILE *);
void		saddr_to_rapi_filt(struct sockaddr_in *, rapi_filter_t *);
int		pfxcmp(char *, char *);
void		print_errno(char *, int);
void		print_status(void);
void		Set_Recv(struct sockaddr_in *, struct sockaddr_in *);
void		Set_Send(struct sockaddr_in *, struct sockaddr_in *, int);
int		next_word(char **, char *);
int		get_sockaddr(int, struct sockaddr_in *);
int		get_flowspec(int, area_t **, int);
int		get_tspec(int, area_t **, int);
int		get_filter(int, area_t **, int);
char		*fmt_filterspec(rapi_filter_t *);
char		*rsvp_timestamp();
int		asynch_handler(rapi_sid_t, rapi_eventinfo_t, int,
			int, int, struct sockaddr *, u_char,
			int, rapi_filter_t *, int, rapi_flowspec_t *,
			int, rapi_adspec_t *, void *);
int		Do_rsvp_cmd(int, int);
rapi_sid_t	rapi_rsvp_cmd(rapi_cmd_t *, int *);
int		Get_Socket(int);
int		tolower(int);
void		rtap_hexmsg(int, int, struct in_addr *, u_char *, int);
int		get_GPIfilter(int, area_t **, int);
/*
 * Globals
 */
#define MAX_NFLWDS      64
#define MAX_T           64

int             T = 1;  	/* Current Thread number */
 
area_t		*Filter_specs[MAX_T];
area_t		*Flowspecs[MAX_T];
area_t		*snd_template[MAX_T];
area_t		*snd_flowspec[MAX_T];
rapi_sid_t      Sid[MAX_T];
int		ttl[MAX_T];
rapi_styleid_t	Style[MAX_T];
struct sockaddr_in dest[MAX_T];
struct sockaddr_in src[MAX_T];
struct sockaddr_in rcvr[MAX_T];
int             Mode[MAX_T];
#define NO_MODE 0
#define S_MODE	0x01	/* Send only */
#define R_MODE	0x02	/* Receive only */
#define RS_MODE S_MODE+R_MODE	/* Send/Receive */
#define GPI_MODE 0x10
int		protid[MAX_T];
int		resv_flags[MAX_T]; /* reservation flags */
int		udpsock[MAX_T];	/* Data socket */

extern	int 	rtap_fd;	/* Unix socket to rsvpd */

#ifdef DATA
struct timeval	timeout, nexttime;
struct timeval	Default_interval = {1, 0};	/* Default is 1 per second */

#define MAXDATA_SIZE		1500
#define DEFAULTDATA_SIZE	512
char		data_buff[MAXDATA_SIZE];
int		data_size[MAX_T] = 0;	/* size of data packet to send */
long		send_data_bytes[MAX_T] = 0;
long		send_data_msgs = 0;
long		recv_data_bytes = 0;
long		recv_data_msgs = 0;
#endif

#define		MAX_PACKET	1024
u_char		packet_buff[MAX_PACKET];
int		packet_size;

#undef		FD_SETSIZE
#define		FD_SETSIZE      32

#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

enum op_codes {
        /* order must be same as entries cmd_table[].cmd_op below!! */
        OP_HELP = 1, OP_QUIT, OP_DEST,   OP_SESS,  OP_SENDER, OP_RESV,
        OP_CLOSE, OP_STATUS,  OP_DATA,	 OP_RECV,
        OP_TTL,   OP_CONFIRM, OP_TEMPLATE, OP_AUTH,
        OP_ASYNC, OP_SLEEP,   OP_DLEVEL, OP_DMASK, OP_MEMORY, OP_HEXMSG,
	OP_ECHO,  OP_BUG,     OP_NULL,
	OP_proto, OP_hostport, OP_hp1,	 OP_hp2,   OP_tspec,
	OP_wf,    OP_ff,      OP_se,	 OP_flowspec, OP_filter
};


/*	Following table MUST match enum order of op_codes !
 */
struct cmds {
	char           *cmd_verb;
	int             cmd_op;
	char           *cmd_help;
}		cmd_table[] = {

  	{ "?",		OP_HELP, 	"Commands are:\n" },
	{ "help",	OP_HELP, 	" help | ?\n" },
	{ "quit",	OP_QUIT, 	" quit\n" },
	{ "T",		OP_HELP,	" T <#>\n" },
	{ "dest", 	OP_SESS,	" dest [gpi] [intserv] <proto> <dest addr>[:<dest port>]\n" },
	{ "session",    OP_SESS,	" session (same as 'dest')\n" },
	{ "sender",	OP_SENDER,	"\
 sender [<src host>:]<src port> <Tspec>\n" },
	{ "reserve",	OP_RESV, 	"\
 reserve [recv addr] wf [*] <flowspec>  |\n\
 reserve [rcv addr] ff <filtspec> <flowspec> [<filtspec> <flowspec>] ... |\n\
 reserve [rcv addr] se [<filtspec> ...] <flowspec>\n" },
	{ "close",	OP_CLOSE, 	" close\n" },
	{ "status",	OP_STATUS,	" status\n" },
	{ "data",	OP_DATA,	" data [<# per sec> [<size>]]\n" },
	{ "receive",	OP_RECV,	" receive [recv host]\n" },
	{ "ttl",	OP_TTL,		" ttl <integer>\n" },	
	{ "confirm",	OP_CONFIRM,	" confirm  (Req confirmation)\n" },	
	{ "template",	OP_TEMPLATE,    " (Not supported)\n"} ,
	{ "auth",	OP_AUTH,	" auth <user credential string>\n" },
	{ "async",	OP_ASYNC,	" async\n" },
	{ "sleep",	OP_SLEEP,	" sleep <seconds>\n" },
	{ "dlevel",	OP_DLEVEL,	" dlevel <logging level>\n" },
	{ "dmask",	OP_DMASK,	" dmask <debug mask>\n" },
#ifdef ISI_TEST
	{ "memory",	OP_MEMORY,	" memory  (=>mallocmap)\n"},
#endif
	{ "hexmsg",	OP_HEXMSG,	" hexmsg <type> <host> [vif] x<hex string>\n"},
	{ "echo",	OP_ECHO,	" echo <text>\n\n" },
	{ "bug",	OP_BUG,		" bug (Toggles on/off)\n" },
	{ "",		OP_NULL,	"" } /* MUST BE LAST IN CMD TABLE */
};

char           *Other_help = "\
 Notes:\n\
        Commands 'template' and 'ttl' are optional.\n\
        Any command may be preceded by 'T<thread#><sp>'\n\
\n\
<proto> ::= 'tcp' | 'udp' | 'proto' <integer>\n\
\n\
<Flowspec> ::= \n\
    '[' 'g' <R> <S>  <r(Bps)> <b(B)> <p(Bps)> <m(B)> <M(B)> ']'  |\n\
    '[' 'cl'  <r(Bps)> <b(B)> <m(B)> <M(B)> ']'  |\n\
\n\
<Tspec> ::= \n\
    '[' 't' <r(Bps)> <b(B)> <p(Bps)> <m(B)> <M(B)> ']'\n\
\n\
<Filtspec> ::= <host name|number>:<port #>\n\
\n\
<type> ::= 'path'|'resv'|'ptear'|'rtear'|'perr'|'rerr'|'confirm'\n\n";

enum {
	Ahelp = 1,	Aotherhelp,	Attl,		Aunsupported,
	Asession,	Asender1,	Asender2,	Aconfirm,
	Asleep,		Asynch,		Aclose,
	Astatus,	Aquit,		Adata,		Abug,
	Aflowspec,	Afilter,	Aresv_init,	Aresv_fin,
	Adlevel,	Admask,		Arecv,		Arcvr,
	Amemory,	Ahexmsg
};

	
Pm_inst rtap_mem[400] = {

    Label(	OP_HELP	),
	Action( Ahelp,	NULL,		OK),

    Label(	OP_BUG ),
	Action( Abug,	NULL,		OK),

	/* [gpi] [intserv] <proto> <dest addr>[:<dest port>] */
    Label(	OP_SESS	),
	Not_String(	"gpi",		2),
	SetLit(		1,		2),
	SetLit(		0,		1),
	Not_String(	"intserv",	2),
	SetLit(		1,		2),
	SetLit(		0,		1),

	IsNot(	OP_proto, 	"Missing: udp|tcp|<int>", ERR ),
	Not_Host("Bad dest host",	ERR),
	EOL(	NULL,			3),
	Not_Char(":/",			3),
	Not_Integer(NULL, 		2),
	Action(	Asession, NULL,		OK),
	Print("Bad dest port", 		ERR),

    Label(	OP_proto ),
	Not_String(	"udp",		2),
	SetLit(		17,		OK),
	Not_String(	"tcp", 		2),
	SetLit(	6,			OK),
	Not_String( "proto",		NO),
	Not_Integer( "Pm: Bad proto",	NO),
	Go(				OK),
	
	/* data [<# per sec> [<size>]] */
    Label(	OP_DATA ),
#ifdef DATA
	Not_Integer( NULL, 		3),
	Not_Integer( NULL, 		2),
	Not_EOL("Bad data parms",	ERR),
	Action(	Adata, 	NULL,		OK),
#else
	Action( Aunsupported,	NULL,	OK),
#endif

	/* receive [recv host] */
    Label(	OP_RECV ),
	Not_Host(NULL,			1),
	Action(	Arecv,	NULL,		OK),

	/* <ttl value> */
    Label(	OP_TTL	),
	Not_Integer("Bad TTL parm",	ERR),
	Action( Attl, 	NULL,		OK),
	
	/* Generalized <sender template> */
    Label( 	OP_TEMPLATE ),
	Action( Aunsupported,	NULL,	OK),

	/* [[<sender host>:]<port>] <tspec> OR
	 *  <empty>
	 */
    Label( 	OP_SENDER ),
	IsNot(	OP_hostport, NULL, 	3),
	InFile( OP_tspec, ".rsvp.profile", 2),
	IsNot(	OP_tspec, "Sender Tspec required", ERR),
	Action(	Asender2, NULL,		OK),

    Label(	OP_CLOSE),
	Action( Aclose,	  NULL, 	OK),
	
    Label(	OP_CONFIRM ),
	/* Turn on confirm flag in next reserv call */
	Action(	Aconfirm, NULL,		OK),

    Label(	OP_ASYNC ),
	Action(	Asynch,	  NULL,		OK),

    Label(	OP_SLEEP ),
	Not_Integer(NULL,		1),
	Action(	Asleep,	  NULL,		OK ),

    Label(	OP_STATUS ),
	Action(	Astatus,  NULL,		OK),

    Label(	OP_QUIT ),
	Action( Aquit,    NULL,		OK),

    Label(	OP_DLEVEL),
	Not_Integer(	  NULL,		1),
	Action(	Adlevel,  NULL,		OK),

    Label(	OP_DMASK ),
	Not_Integer(	  NULL,		1),
	Action( Admask,	  NULL,		OK),

#ifdef ISI_TEST
    Label(	OP_MEMORY ),
	Action(	Amemory,  NULL,		OK),
#endif /* ISI_TEST */

    Label(	OP_HEXMSG ),
	Char(		"x",		20),
	Not_String(	"path",		2),
	SetLit(		1,		14),
	Not_String(	"resv",		2),
	SetLit(		2,		12),
	Not_String(	"perr",		2),
	SetLit(		3,		10),
	Not_String(	"rerr",		2),
	SetLit(		4,		8),
	Not_String(	"ptear",	2),
	SetLit(		5,		6),
	Not_String(	"rtear",	2),
	SetLit(		6,		4),
	Not_String(	"confirm",	2),
	SetLit(		7,		2),
	Print("Bad msg type", 		ERR),
	
	Not_Host(	"host?",	ERR),
	Integer(	NULL,		2),
	SetLit(		-1,		1),
	Not_Char(	"x",		1),
	Action( Ahexmsg,  NULL,		OK),

    Label(	OP_hostport ),
	/*	 [host :] port WhSp
	 *		Needs recursive calls, back-tracking
 	 */
	Is(	OP_hp1,	 NULL, 		OK),
	Is(	OP_hp2,	 NULL,		OK),
	Not_Host(	 NULL,		NO),
	SetLit( 	 0,		1),
	Go(				OK),

    Label(	OP_hp1 ),
	/*	host : port WhSp
	 */
	Not_Host(	NULL,		NO),
	Not_Char(	":/",		NO),
	Not_Integer(	"Bad port",	ERR),
	Not_WhSp( 	"Bad port",	ERR),
	Go(				OK),

    Label(	OP_hp2 ),
	/*	[host == 0] port WhSp
	 */
	SetLit(	0,			1),
	Not_Integer(	NULL,		NO),
	Not_WhSp(	NULL,		NO),
	Go(				OK),

    Label(	OP_RESV ),
	Not_Char( "?",			2),
	Action(	Aotherhelp, NULL, 	OK),

	Action(	Aresv_init, 	NULL,	1),
	Not_String("wf",		2),
	Is(	OP_wf,	NULL,		OK),
	Not_String("ff",		2),
	Is(	OP_ff, 	NULL,		OK),
	Not_String("se", 		2),
	Is(	OP_se,	NULL,		OK),
	Not_Host( "Bad style",		ERR),
	Action(Arcvr,	NULL,		1),

	Not_String("wf",		2),
	Is(	OP_wf,	NULL,		OK),
	Not_String("ff",		2),
	Is(	OP_ff, 	NULL,		OK),
	Not_String("se", 		2),
	Is(	OP_se,	NULL,		OK),
	Print( "Bad style", 		ERR),

    Label(	OP_wf ),
	/*
	 *	WF parms: ['*'] <flowspec>
	 */
	SetLit(	RAPI_RSTYLE_WILDCARD,	1),
	EOL(	NULL,			5),
	Char(	"*",			1),
	InFile( OP_flowspec, ".rsvp.profile", 2),
	IsNot(	OP_flowspec, "Flowspec illegal or missing", ERR),
	Action( Aflowspec, NULL,	1),
	Action( Aresv_fin,  NULL,	OK),

    Label( OP_ff ),
	/*
	 *  FF parms: { { <filtspec> }*1 <flowspec> }*
	 *  really: { <filtspec> <flowspec> }*
	 */
	SetLit( RAPI_RSTYLE_FIXED,	1),
	Not_EOL(	NULL,		2),
	Action( Aresv_fin,	NULL,	OK),
	IsNot( OP_filter, "Bad filterspec", ERR),
	Action(	Afilter, NULL,		1),
	InFile( OP_flowspec, ".rsvp.profile", 2),
	IsNot( OP_flowspec, "Bad flowspec", ERR), 
	Action( Aflowspec, NULL,	-6),

	/*
	 * SE parms: [ <filtspec>*1  <flowspec> ]
	 */
    Label( OP_se ),
	SetLit( RAPI_RSTYLE_SE,		1),
	Not_EOL(	NULL,		2),
	Action( Aresv_fin,	NULL,	OK),
	IsNot( OP_filter, "Missing filter spec", ERR),
	Action(	Afilter, NULL,		1),
	Is( OP_filter, 	NULL,		-1),
	InFile( OP_flowspec, ".rsvp.profile", 2),
	IsNot( OP_flowspec, "Missing flowspec", ERR),
	Action( Aflowspec, NULL,	-7),

    Label( OP_tspec ),
	/*	Tspec
	 *	'['  't'   <r> <b> [<p> [<m> [<M>]]] ']'	
	 */
	Not_Char("[",		NO),
	Not_String("t", 	2),
	SetLit(QOS_TSPEC,	2),
	Print("Unknown service", ERR),

	Not_Integer("Missing r: TB depth", ERR),
	Not_Integer("Missing b: TB rate", ERR),
	Not_Integer(NULL,	3),
	Not_Integer(NULL,	2),
	Not_Integer(NULL,	1),
	Char("]",		OK),
	Print("Missing ]", 	ERR),

    Label(	OP_flowspec ),
	/*	'['   'cl'  <b> <r> [<m> [<M>]] ']'
	 *	'['   {'g'|'gx'}  <R> <S> <b> <r> [<p> [<m> [<M>]]] ']'
	 */
	Not_Char("[",		NO),
	Not_String("g", 	2),
	SetLit(QOS_GUARANTEED,	12),
	Not_String("gx", 	2),
	SetLit(QOS_GUARANTEEDX, 10),
	Not_String("cl", 	2),
	SetLit(QOS_CNTR_LOAD,	2),
	Print("Unknown service", ERR),
	Not_Integer("Missing r: TB depth", ERR),
	Not_Integer("Missing b: TB rate", ERR),
	Not_Integer(NULL,	2),
	Not_Integer(NULL,	1),
	Char("]",		OK),
	Print("Missing ]", 	ERR),

	Not_Integer("Missing R: Rate", ERR),
	Not_Integer("Missing S: Slack", ERR),
	Not_Integer("Missing r: TB depth", ERR),
	Not_Integer("Missing b: TB rate", ERR),
	Not_Integer(NULL,	1),
	Go(			-11),	

    Label( OP_filter ),
	/*	host [ : port ]
	 */
	Not_Host(NULL,			NO),
	WhSp(	NULL,			OK),
	Char(":/",			2),
	Print("Bad dest port", 		ERR),
	Not_Integer(NULL, 		-1),
	Go(				OK),

    Label( 0 )	

};


int
Pm_Action(int op, int vi, int parm)
	{
	int		i, sid, rc;
	char		*cp;
	u_char		u_ch, flags;
#ifdef DATA
	double		F_intvl;
	long		I_intvl;
#endif
#ifdef ISI_TEST
	void		mallocmap();
#endif

	switch (op) {

	case Ahelp:
		for (i = 0; i < OP_NULL; i++)
			printf("%s", cmd_table[i].cmd_help);
	case Aotherhelp:
		printf(Other_help);
		break;

	case Abug:
		Pm_debug = !Pm_debug;
		break;

	case Asession:
		/*	val stack: 
		 *	    gpi?1|0 intserv?1:0 proto desthost [destport]
		 */
		flags = 0;
		if (val0) {
			flags = RAPI_GPI_SESSION;
			Mode[T] |= GPI_MODE;
		}
		if (val1)
			flags |= RAPI_USE_INTSERV;

		protid[T] = val2;
		get_sockaddr(3, &dest[T]);

		if (Sid[T] != NULL_SID) {
			printf("close first\n");
			break;
		}
		sid = rapi_session((struct sockaddr *)&dest[T],
				protid[T], flags,
				(rapi_event_rtn_t) asynch_handler, 0, &rc);
		if (sid == NULL_SID)
			print_errno("rsvp_session()", rc);
		else {
			Sid[T] = sid;
			rtap_fd = rapi_getfd(sid);
			printf("T%d: rapi_session => sid= %d, fd= %d\n",
							 T, sid, rtap_fd);
		}
#ifdef DATA
		send_data_bytes = recv_data_bytes = 0;
		send_data_msgs = recv_data_msgs = 0;
#endif
		break;

#ifdef DATA
	case Adata:
		if (Pm_vi > 0) {
			F_intvl = atof(val0);
			I_intvl = (long) rint(1e6/F_intvl);
			timeout.tv_sec = I_intvl/1000000;
			timeout.tv_usec = I_intvl%1000000;
			if (Pm_vi > 1)
				data_size = val1;
			else
				data_size = DEFAULTDATA_SIZE;
		}
		gettimeofday(&nexttime, NULL);
		tvadd(&nexttime, &timeout);
		break;
#endif /* DATA */

	case Arecv:
		if (Sid[T] == NULL_SID) {
			printf("Must issue dest cmd first\n");
			break;
		}
               	memset(&rcvr[T], 0, sizeof(struct sockaddr_in));
		if (udpsock[T] < 0) 
			udpsock[T] = Get_Socket(dest[T].sin_port);
		Set_Recv(&dest[T], &rcvr[T]);
		break;

	case Attl:
		if (Pm_vi > 0)
			ttl[T] = val0;
#ifdef DATA
		if (( Mode & S_MODE ) && data_size > 0)
			Set_Send(&dest[T], &src[T], ttl[T]);
#endif
		break;

	case Aunsupported:
		printf("Not supported\n");
		break;

	case Asender2:
		if (Sid[T] == NULL_SID) {
			printf("Must issue dest cmd first\n");
			break;
		}
		if (Pm_vi) {
			if (Mode[T] & GPI_MODE) {
				init_Area((area_t **)&snd_template[T],
						sizeof(rapi_filter_t));
				get_GPIfilter(vi, &snd_template[T],
						sizeof(rapi_filter_t));
			}
			else
				get_sockaddr(vi, &src[T]);

			init_Area((area_t **)&snd_flowspec[T], 
						sizeof(rapi_flowspec_t));
			get_tspec(vi+2, &snd_flowspec[T], 
						sizeof(rapi_flowspec_t));
		}

		/*
		 *	Make rapi_sender call to RSVP API
		 */
		rc = rapi_sender(Sid[T], 0,
				(Pm_vi>0)? (struct sockaddr *)&src[T]: NULL,
                       		Area_Data((rapi_filter_t *), snd_template[T]),
                       		Area_Data((rapi_tspec_t *), snd_flowspec[T]),
				NULL, NULL, ttl[T]);
		if (rc)
			print_errno("rsvp_sender()", rc);
		else
			printf("rapi_sender() OK\n");
		Mode[T] |= S_MODE;
		break;

	case Aclose:
		if (Sid[T] == NULL_SID) {
			printf("Already closed\n");
			break;
		}
		rc = rapi_release(Sid[T]);
		if (rc)
			print_errno("rapi_release()", rc);
		printf("T%d: rapi_release(): sid= %d, fd= %d\n",
					T, Sid[T], rtap_fd);
		Sid[T] = NULL_SID;
		Mode[T] = NO_MODE;
		if (udpsock[T] >= 0)
			close(udpsock[T]);	/* close data socket */
		udpsock[T] = -1;
		break;
	
	case Aconfirm:
		resv_flags[T] = RAPI_REQ_CONFIRM;
		break;
		
	case Asynch:
		rc = rapi_dispatch();
		if (rc)
			print_errno("rapi_dispatch()", rc);

	case Amemory:
#if defined(ISI_TEST) && defined(SunOS)
		mallocmap();
#else
		printf("Not supported.\n");
#endif
		break;

	case Ahexmsg:	/* To send arbitrary RSVP message, use:
			 *	hexmsg <type> <host> <vif> x<hex string>
			 * If the packet is larger than 100 bytes or for
			 * convenience, can split into multiple lines, ended
			 * with the ! :
			 *	hexmsg x<hex string1>
			 *	hexmsg x<hex string2>
			 *      ...
			 *	hexmsg <tyhpe> <host> [vif] x<last hex string>
 			 *
			 * Val stack: [<msg type #> <target host> <vif|-1>]
			 */
#ifdef RTAP
		u_ch = i = 0;
		for (cp = Pm_cp; *cp; cp++) {
			if (isxdigit(*cp)) {
#define tohex(c)  (isdigit(c)?c-'0':tolower(c)-'a'+10)
				u_ch = (u_ch<<4)|tohex(*cp);
				if (++i == 2) {
					packet_buff[packet_size++] = u_ch;
					i = u_ch = 0;
				}
			}
			else if ((*cp == ' '||*cp == '#') &&(i)) {
				printf("Bad hex @ +%d\n", cp-Pm_cp-1);
				break;
			}
			if (*cp == '#')
				break;
		}
		/* Only last line of multi-line message specifies host
		 */
		if (Pm_vi == 0)
			break;

		rtap_hexmsg(val0, val2, (struct in_addr *)&val1,  packet_buff,
							packet_size);
		packet_size = sizeof(common_header);  /* somewhat of a kludge */
#else
		printf("Unsupported by remote rtap\n");
#endif
		break;		

	case Asleep:
		rtap_sleep((Pm_vi > 0)? val0:0);
		break;

	case Astatus:
		print_status();
		break;

	case Aquit:
		if (Sid[T] != NULL_SID) {
			rc = rapi_release(Sid[T]);
			if (rc)
				print_errno("rapi_release()", rc);
			printf("Closed sid= %d, fd= %d\n", Sid[T], rtap_fd);
		}
		if (udpsock[T] >= 0)
			close(udpsock[T]);	/* close data socket */
		exit(0);
		
	case Afilter:
		get_filter(1+vi, &Filter_specs[T], sizeof(rapi_filter_t));
		Pm_vi = vi+1;
		break;

	case Aflowspec:
		/* Value stack: <style> <service> <parm> ... 
		 */
		get_flowspec(1+vi, &Flowspecs[T], sizeof(rapi_flowspec_t));
		Pm_vi = vi+1;
		break;
	
	case Aresv_fin:
		Mode[T] |= R_MODE;
		Style[T] = (rapi_styleid_t) Pm_val[vi];
		rc = rapi_reserve(Sid[T], resv_flags[T],
			    (struct sockaddr *) &rcvr[T],
			    Style[T], NULL, NULL,
			    Filter_specs[T]->area_cnt,
			    Area_Data((rapi_filter_t *), Filter_specs[T]),
			    Flowspecs[T]->area_cnt,
			    Area_Data((rapi_flowspec_t *), Flowspecs[T]));
		if (rc)
			print_errno("rapi_reserve()", rc);
		resv_flags[T] = 0;
		break;

	case Arcvr:
		Append_Val(0);	/* No port */
		get_sockaddr(vi, &rcvr[T]);
		break;
		
	case Aresv_init:
               	memset(&rcvr[T], 0, sizeof(struct sockaddr_in));
		init_Area((area_t **)&Filter_specs[T], 
					MAX_NFLWDS*sizeof(rapi_filter_t));
		init_Area((area_t **)&Flowspecs[T], 
					MAX_NFLWDS*sizeof(rapi_flowspec_t));
		break;

	case Adlevel:
		Do_rsvp_cmd((Pm_vi)?Pm_val[0]: -1, RAPI_CMD_DEBUG_LEVEL);
		break;

	case Admask:
		Do_rsvp_cmd((Pm_vi)?Pm_val[0]: -1, RAPI_CMD_DEBUG_MASK);
		break;
	}
	return(OK);
}


/* get_sockaddr: Create sockaddr from host, port values in value list
 *	at offset voff, voff+1.
 */
int
get_sockaddr(voff, sinp)
	int	voff;
	struct sockaddr_in *sinp;
	{

	memset(sinp, 0, sizeof(struct sockaddr_in));
	sinp->sin_family = AF_INET;
#if BSD > 199103
	sinp->sin_len = sizeof(struct sockaddr_in);
#endif	
	sinp->sin_addr.s_addr = Pm_val[voff];
	if (Pm_vi > voff+1)
			sinp->sin_port = hton16((u_short)Pm_val[voff+1]);
		else
			sinp->sin_port = 0;
	return(0);
}

/*
 *	Construct RAPI flowspec object from value list:
 *	voff+: 0=type
 *		type = g|p: 1=r, 2=b, [3=d, [4=dl]]
 *		type = cl:  1=r, 2=b, [3=m, [4=M]]
 *		type = gx:  1=R, 2=S, 3=r, 4=b, [5=p, [6=m, [7=M]]]
 */
int
get_flowspec(int voff, area_t **areapp, int size)
	{
	rapi_flowspec_t		Tflow, *flowsp = &Tflow;
#ifdef OBSOLETE_API
	qos_flowspec_t		*cszp = &flowsp->specbody_qos;
#endif
	qos_flowspecx_t		*csxp = &flowsp->specbody_qosx;
	int			type = Pm_val[voff];
	
	switch (type) {

#ifdef OBSOLETE_API
	case QOS_PREDICTIVE:
	case QOS_GUARANTEED:
		cszp->spec_type = type;
		cszp->spec_r = Pm_val[voff+1];
		cszp->spec_b = Pm_val[voff+2];
		cszp->spec_d = 0;
		cszp->spec_sl = -1;
		if (Pm_vi > voff+3) {
			cszp->spec_d = Pm_val[voff+3];
			if (Pm_vi > voff+4)
				cszp->spec_sl = Pm_val[voff+4];
		}
		flowsp->form = RAPI_FLOWSTYPE_CSZ;
		break;
#endif

	case QOS_CNTR_LOAD:
		csxp->spec_type = type;
		csxp->xspec_r = Pm_val[voff+1];
		csxp->xspec_b = Pm_val[voff+2];
		csxp->xspec_p = INFINITY32f;
		csxp->xspec_m = 0;
		csxp->xspec_M = 65535;
		if (Pm_vi > voff+3) {
			csxp->xspec_m = Pm_val[voff+3];
			if (Pm_vi > voff+4)
				csxp->xspec_M = Pm_val[voff+4];
		}
		flowsp->form = RAPI_FLOWSTYPE_Simplified;
		break;

	case QOS_GUARANTEEDX:
		csxp->spec_type = type;
		csxp->xspec_R = Pm_val[voff+1];
		csxp->xspec_S = Pm_val[voff+2];
		csxp->xspec_r = Pm_val[voff+3];
		csxp->xspec_b = Pm_val[voff+4];
		csxp->xspec_p = INFINITY32f;
		csxp->xspec_m = 0;
		csxp->xspec_M = 65535;
		if (Pm_vi > voff+5) {
			csxp->xspec_p = Pm_val[voff+5];
			if (Pm_vi > voff+6) {
				csxp->xspec_m = Pm_val[voff+6];
				if (Pm_vi > voff+7)
					csxp->xspec_M = Pm_val[voff+7];
			}
		}
		flowsp->form = RAPI_FLOWSTYPE_Simplified;
		break;

	default:
		printf("Unknown flowspec type: %d\n", Pm_val[voff]);
		return(-1);
	}
		
	flowsp->len = sizeof(rapi_flowspec_t);
        Cat_Area(*areapp, (rapi_hdr_t *) flowsp);
 	return (0);
}


/*
 *	Construct RAPI sender-tspec object from value list:
 *	voff: +0=type
 *		type = g|p:  +1=r, +2=b, [+3=d, [+4=dl]]
 *		type = cl:   +1=r, +2=b, [+3=m, [+4=M]]
 *		type = gx|t: +1=r, +2=b, [+3=p, [+4=m, [+5=M]]]
 */
int
get_tspec(int voff, area_t **areapp, int size)
	{
	rapi_tspec_t		Tspec, *tspecp = &Tspec;
#ifdef OBSOLETE_API
	qos_flowspec_t		*cszp = &tspecp->specbody_qos;
#endif /* OBSOLETE_API */
	qos_tspecx_t		*ctxp = &tspecp->tspecbody_qosx;
	int			type = Pm_val[voff];
	
	switch (type) {

#ifdef OBSOLETE_API
	case QOS_PREDICTIVE:
	case QOS_GUARANTEED:
		cszp->spec_type = QOS_TSPEC;
		cszp->spec_r = Pm_val[voff+1];
		cszp->spec_b = Pm_val[voff+2];
		cszp->spec_d = 0;
		cszp->spec_sl = -1;
		if (Pm_vi > voff+3) {
			cszp->spec_d = Pm_val[voff+3];
			if (Pm_vi > voff+4)
				cszp->spec_sl = Pm_val[voff+4];
		}
		tspecp->form = RAPI_FLOWSTYPE_CSZ;
		tspecp->len = sizeof(rapi_hdr_t) + sizeof(qos_flowspec_t);
		break;
#endif /* OBSOLETE_API */

	case QOS_CNTR_LOAD:
		ctxp->spec_type = type;
		ctxp->xtspec_r = Pm_val[voff+1];
		ctxp->xtspec_b = Pm_val[voff+2];
		ctxp->xtspec_p = INFINITY32f;
		ctxp->xtspec_m = 0;
		ctxp->xtspec_M = 65535;
		if (Pm_vi > voff+3) {
			ctxp->xtspec_m = Pm_val[voff+3];
			if (Pm_vi > voff+4)
				ctxp->xtspec_M = Pm_val[voff+4];
		}
		tspecp->form = RAPI_TSPECTYPE_Simplified;
		tspecp->len = sizeof(rapi_hdr_t) + sizeof(qos_tspecx_t);
		break;

	case QOS_GUARANTEEDX:
	case QOS_TSPECX:
		ctxp->spec_type = QOS_GUARANTEEDX;
		ctxp->xtspec_r = Pm_val[voff+1];
		ctxp->xtspec_b = Pm_val[voff+2];
		ctxp->xtspec_p = INFINITY32f;
		ctxp->xtspec_m = 0;
		ctxp->xtspec_M = 65535;
		if (Pm_vi > voff+3) {
			ctxp->xtspec_p = Pm_val[voff+3];
			if (Pm_vi > voff+4) {
				ctxp->xtspec_m = Pm_val[voff+4];
				if (Pm_vi > voff+5)
					ctxp->xtspec_M = Pm_val[voff+5];
			}
		}
		tspecp->len = sizeof(rapi_hdr_t) + sizeof(qos_tspecx_t);
		tspecp->form = RAPI_TSPECTYPE_Simplified;
		break;

	default:
		printf("Unknown flowspec type: %d\n", Pm_val[voff]);
		return(-1);
	}
		
        Cat_Area(*areapp, (rapi_hdr_t *) tspecp);
 	return (0);
}

int
get_filter(int voff, area_t **areap, int size)
	{
	struct sockaddr_in sin;
	rapi_filter_t	Tfilt;

	get_sockaddr(voff, &sin);
	saddr_to_rapi_filt(&sin, &Tfilt);
        Cat_Area(*areap, (rapi_hdr_t *) &Tfilt);
	return (0);
}

int
get_GPIfilter(int voff, area_t **areap, int size)
	{
	rapi_filter_t	Tfilt;

	Tfilt.filt_u.gpi.sender.s_addr = Pm_val[voff];
	Tfilt.filt_u.gpi.gpi = Pm_val[voff+1];
	Tfilt.len = sizeof(rapi_hdr_t)+sizeof(rapi_filter_gpi_t);
	Tfilt.form = RAPI_FILTERFORM_GPI;
        Cat_Area(*areap, (rapi_hdr_t *) &Tfilt);
	return (0);
}
	

/*
 *	rtap_cmds_init(): Initialization routine, called to start things off.
 */
void
rtap_cmds_init()
	{
	int i;

	Pm_Init(rtap_mem);

	for (i = 0; i < MAX_T; i++) {
		Filter_specs[i] = NULL;
               	Flowspecs[i] = NULL;
                snd_template[i] = NULL;
                snd_flowspec[i] = NULL;
                Sid[i] = ttl[i] = 0;
               	Mode[i] = NO_MODE;
		resv_flags[i] = 0;
		udpsock[i] = -1;
               	memset(&dest[i], 0, sizeof(struct sockaddr_in));
               	memset(&src[i], 0, sizeof(struct sockaddr_in));
               	memset(&rcvr[i], 0, sizeof(struct sockaddr_in));

#ifdef DATA
		timeout = Default_interval;
		data_size = 0;
#endif
	}
	packet_size = sizeof(common_header);
}

/*
 *  Do_Command(): Top-level routine to read the next line from a given
 *	file and execute the command it contains.  Returns 0 if EOF, else 1.
 */
int
Do_Command(char *infile, FILE *infp)
	{
	char	cmd_line[256], cmd_op[80];
	struct cmds    *cmdp;
	char	*cp;
	int	rc;

	if (fgets(cmd_line, sizeof(cmd_line), infp) == NULL) {
		printf("No input\n");
		return(0);
	}
	/* remove trailing NL */
	cp = cmd_line + strlen(cmd_line) - 1;
	if (*cp == '\n')
		*cp = '\0';
	else {
		printf("LINE > 255 CHARS: %s.\n", cmd_line);
		exit(1);
	}
	/*** printf("Do_Command: %s\n", cmd_line); ***/
	if (infile)
		printf("%12s > %s\n", rsvp_timestamp(), cmd_line);

	if (cmd_line[0] == '#') {	/* Comment, skip this line */
		return(1);
	}
	cp = cmd_line;
	if (!next_word(&cp, cmd_op))
		return(1);
	if (cmd_op[0] == 'T') {
		T = atoi(&cmd_op[1]);
		if (!next_word(&cp, cmd_op))
			return(1);
	}
	cmdp = cmd_table;
	while ((cmdp->cmd_op != OP_NULL) && pfxcmp(cmd_op, cmdp->cmd_verb))
		cmdp++;
	if (cmdp->cmd_op == OP_NULL) {
		if (cmd_op[0])
			printf(" ?? %s\n", cmd_op);
		return(1);
	}
	/*	Initialize parse machine: semantic stack and scan pointer.
	 */
	Pm_vi = 0;
	Pm_cp = cp;
	if (packet_size > sizeof(common_header) && cmdp->cmd_op != OP_HEXMSG) {
		printf("Hexmsg cmd not terminated \n");
		packet_size = sizeof(common_header);
	}
	rc = Parse_machine(rtap_mem, cmdp->cmd_op);
	if (rc != OK)
		printf("Error.\n");
	if (rc == NO)
		return(0);
	return(1);
}


void
saddr_to_rapi_filt(
	struct sockaddr_in	*host,
	rapi_filter_t		*p)
	{
	p->len = sizeof(rapi_hdr_t) + sizeof(rapi_filter_base_t);
	p->form = RAPI_FILTERFORM_BASE;
	p->filt_u.base.sender = *host;
}


/*
 * Prefix string comparison: Return 0 if s1 string is prefix of s2 string, 1
 * otherwise.
 */
int 
pfxcmp(s1, s2)
	register char  *s1, *s2;
{
	while (*s1)
		if (*s1++ != *s2++)
			return (1);
	return (0);
}

int
asynch_handler(
	rapi_sid_t		sid,
	rapi_eventinfo_t	event,
	int			styleid,
	int			errcode,
	int			errval,
	struct sockaddr		*errnode,
	u_char			errflags,
	int			filt_num,
	rapi_filter_t		*filt_list,
	int			 flow_num,
	rapi_flowspec_t		*flow_list,
	int			 adspec_num,
	rapi_adspec_t		*adspec_list,
	void			*myparm)
	{
	rapi_filter_t *filtp = filt_list;
	rapi_flowspec_t *flowp = flow_list;
	rapi_adspec_t *adsp = adspec_list;
	int	isPath = 1;
	char	buff[128], out1[80];
	int	T, i;
	extern  char *rapi_rstyle_names[];

	for (T=0 ; T < MAX_T; T++)	/* Map sid into thread# T */
		if (Sid[T] == sid)
			break;

	sprintf(out1, "  Session= %.32s:%d", inet_ntoa(dest[T].sin_addr), 
	        			  ntoh16(dest[T].sin_port));


		printf(
	"---------------------------------------------------------------\n");		
	switch (event) {

	case RAPI_PATH_EVENT:
		printf("T%d: Path Event -- %s\n", T, out1);
		if (flow_num == 0)
			printf("    (No Path state)\n");
		break;

	case RAPI_RESV_EVENT:
		printf("T%d: Resv Event -- %s\n", T, out1);
		if (flow_num == 0)
			printf("    (No Resv state)\n");
		isPath = 0;
		break;

	case RAPI_RESV_ERROR:
		isPath = 0;
	case RAPI_PATH_ERROR:
		printf("T%d: sid=%d %s -- RSVP error: %s\n",
			T, sid, out1,
			(errcode==RSVP_Err_API_ERROR)?
				rapi_errlist[errval]:
				rsvp_errlist[errcode]);
		if (event == RAPI_RESV_ERROR)
			printf("    Style=%s", rapi_rstyle_names[styleid]);
		printf("    Code=%d  Val=%d  Node= %s",
			 errcode, errval,
			 inet_ntoa(((struct sockaddr_in *)errnode)->sin_addr));
		if (errflags&RAPI_ERRF_InPlace)
			printf(" *InPlace*");
		if (errflags&RAPI_ERRF_NotGuilty)
			printf(" *NotGuilt*");
		printf("\n");
		break;

	case RAPI_RESV_STATUS:
		isPath = 0;
		printf("T%d: Resv State -- %s:\n", T, out1);
		break;

	case RAPI_PATH_STATUS:
		printf("T%d: Path Stat -- %s:\n", T, out1);
		break;

	case RAPI_RESV_CONFIRM:
		isPath = 0;
		printf("T%d: Confirm Event -- %s:\n", T, out1);
		break;

	default:
		printf("!!?\n");
		break;
	}
	for (i = 0; i < MAX(filt_num, flow_num); i++) {
		if (i < filt_num) {
			printf("\t%s", fmt_filterspec(filtp));
			filtp = (rapi_filter_t *)After_RAPIObj(filtp);
		}
		else
			printf("\t\t");
		if (i < flow_num) {
			if (isPath)
				rapi_fmt_tspec((rapi_tspec_t *)flowp,
							 buff, sizeof(buff));
			else
				rapi_fmt_flowspec(flowp, buff, sizeof(buff));
			printf("\t%s\n", buff);
			flowp = (rapi_flowspec_t *)After_RAPIObj(flowp);
		}
		else
			printf("\n");
		if (i < adspec_num) {
			rapi_fmt_adspec(adsp, buff, sizeof(buff));
			printf("\t\t%s\n", buff);
			adsp = (rapi_adspec_t *)After_RAPIObj(adsp);
		}
			
	}
	printf(
	"---------------------------------------------------------------\n");
	fflush(stdout);
	return 0;
}


/*
 * Skip leading blanks, then copy next word (delimited by blank or zero, but
 * no longer than 63 bytes) into buffer b, set scan pointer to following 
 * non-blank (or end of string), and return 1.  If there is no non-blank text,
 * set scan ptr to point to 0 byte and return 0.
 */
int 
next_word(char **cpp, char *b)
{
	char           *tp;
	int		L;

	*cpp += strspn(*cpp, " \t");
	if (**cpp == '\0' || **cpp == '\n' || **cpp == '#')
		return(0);

	tp = strpbrk(*cpp, " \t\n#");
	L = MIN((tp)?(tp-*cpp):strlen(*cpp), 63);
	strncpy(b, *cpp, L);
	*(b + L) = '\0';
	*cpp += L;
	*cpp += strspn(*cpp, " \t");
	return (1);
}

char           *
argscan(argcp, argvp)
	int            *argcp;
	register char ***argvp;
{
	register char  *cp;

	if (*(cp = 2 + **argvp))
		return (cp);
	else if (--*argcp > 0)
		return (*++*argvp);

	exit(1);

	/* make the sgi compiler happy -mwang*/
	return NULL;
}


void
print_errno(char *cp, int rapi_errno)
	{
	printf("RAPI: %s err %d : %s\n", cp, rapi_errno,
					rapi_errlist[rapi_errno]);
}

void
print_status()
	{
	char	*state_str;
	char	buff[128];
	int	mode;
#ifdef ISI_TEST
	int	rc;
#endif
		
	mode = Mode[T] & (RS_MODE);
	if (mode == RS_MODE) state_str = "Open SR";
	else if (mode == R_MODE) state_str = "Open R";
	else if (mode == S_MODE) state_str = "Open S";
	else state_str = "Closed";

	printf("T%d STATUS:\n", T);
	printf("  State: %s  sid= %d \n", state_str, Sid[T]);
	printf("  Session:\n     Dest= %s:%d\n",
		inet_ntoa(dest[T].sin_addr), 
	        ntoh16(dest[T].sin_port));

        if (src[T].sin_addr.s_addr) {
		printf("  Sender:\n    %s:%d", inet_ntoa(src[T].sin_addr), 
						ntoh16(src[T].sin_port));
		if (snd_template[T])
			printf("  Snd_Templ= %s",
			       fmt_filterspec(Area_Data((rapi_filter_t *), 
					snd_template[T])));
		if (snd_flowspec[T]) {
			rapi_fmt_tspec(Area_Data((rapi_tspec_t *),
					snd_flowspec[T]), buff, 128);
			printf("  Tspec= %s", buff);
		}
		printf("\n");
	}
#ifdef DATA
	printf("  Data:\n    Size= %d  Timeout= %ld.%06ld  TTL= %d\n",
		data_size, timeout.tv_sec, timeout.tv_usec, ttl);
	printf("    Sent %ld bytes, %ld msgs; Rcvd %ld bytes, %ld msgs\n",
	    send_data_bytes, send_data_msgs, recv_data_bytes, recv_data_msgs);
#endif
#ifdef ISI_TEST
	rc = rapi_status(Sid[T], RAPI_STAT_PATH+RAPI_STAT_RESV);
	if (rc)
		print_errno("rapi_status()", rc);
#endif
	sleep(2);
}

#define MinRAPILen	8	/* minimum RAPI filterspec length */

char *
fmt_filterspec(rapi_filter_t *rfiltp)
	{
	static char FIbuff[256];

	rapi_fmt_filtspec(rfiltp, FIbuff, 256);
	return(FIbuff);
}

area_t *
init_Area(area_t **app, int size)
	{
	area_t *ap = *app;

	if (ap == NULL) {
       		size += sizeof(area_t);
       		if (size <= 0 || (ap = (area_t *) malloc(size)) == NULL)
                	return(NULL);
		*app = ap;
        	ap->area_size = size;
	}
	ap->area_offset = sizeof(area_t);
	ap->area_cnt = 0;
        return(ap);
}

char *
Area_offset(area_t *ap) {
        return((char *) ap + ap->area_offset);
}

void
Cat_Area(area_t *orig, rapi_hdr_t *objp) {
        if (orig->area_offset + objp->len > orig->area_size) {
                printf("!!Area overflow\n");
                return;
        }
        memcpy((rapi_hdr_t *)Area_offset(orig), objp, objp->len);
        orig->area_offset += objp->len;
        orig->area_cnt++;
}

int
Get_Socket(int port)
	{
	struct sockaddr_in	sin;
	int			one = 1;
	int			sock;

	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("rtap socket:");
		exit(1);
	}

	memset((char *) &sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
#if BSD >= 199103
        sin.sin_len = sizeof(sin);
#endif
	sin.sin_port = port;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, 
						(char *)&one, sizeof(one))){
		perror("rtap sockopt:");
		exit(1);
	}
	if (bind(sock, (struct sockaddr *) &sin, sizeof(sin))) {
		perror("rtap bind:");
		exit(1);
	}
	return (sock);
}

void
Set_Recv(dstp, srcp)
	struct sockaddr_in	*dstp;
	struct sockaddr_in 	*srcp;
{
	struct ip_mreq  	mreq;

	if (!IN_MULTICAST(ntoh32(dstp->sin_addr.s_addr)))
		return;
	mreq.imr_multiaddr = dstp->sin_addr;
	mreq.imr_interface = srcp->sin_addr;
	if (0 > cap_setsockopt(udpsock[T], IPPROTO_IP, IP_ADD_MEMBERSHIP,
				(char *) &mreq, sizeof(mreq))) {
		perror("Can't join multicast group");
		return;
	}
	printf("Joined multicast group %s\n", inet_ntoa(dstp->sin_addr));
}

#ifdef DATA
/* Following code is carried over from earlier version of rtap,
 * which could send data.  This code is currently incomplete.
 */
void
Set_Send((struct sockaddr_in *, struct sockaddr_in *, int T)
	{
        int     sock = udpsock[T];
        u_char  zero = 0;
        u_char  ttlv = ttl[T];

	if (0 > cap_setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL,
                      			&ttlv, sizeof(ttlv))) {
		perror("Can't set multicast TTL");
		exit(1);
	}
	if (0 > cap_setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP,
					&zero, sizeof(zero))) {
		perror("Can't set no loopback");
		exit(1);
	}
	if (0 > cap_setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF,
				&srcp->sin_addr, sizeof(struct in_addr))) {
		perror("Can't set mcast iface");
		exit(1);
	}
}

void
Send_data(T)
	{
	int rc;

	rc = sendto(udpsock[T], data_buff, data_size, 0,
			(struct sockaddr *) dstp, sizeof(struct sockaddr_in));
        if (rc < 0) {
                perror("sendto");
                return;
        }
        send_data_bytes[T] += rc;
        send_data_msgs[T] ++;
}
#endif /* DATA */

int
Do_rsvp_cmd(int flags, int type)
	{
	int		old_level, errnum;
	rapi_cmd_t	cmd;

	cmd.rapi_cmd_len = 1;
	cmd.rapi_cmd_type = type;
	cmd.rapi_filler = flags;
	old_level = rapi_rsvp_cmd(&cmd, &errnum);
	if (type == RAPI_CMD_DEBUG_LEVEL)
		printf("Rsvpd logging level:\n");
	else
		printf("Rsvpd debug mask:\n");
	if (flags != -1)
		printf("	Changing to: %d\n", flags);
	if (old_level < 0)
		printf("rapi_debug_level error: %d\n", errnum);
	return(0);
}

