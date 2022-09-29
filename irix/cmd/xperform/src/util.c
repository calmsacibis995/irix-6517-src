#include <stdio.h>
#include <stdlib.h>

#define XDIR	"/usr/local/qatools/xperform"
#define HOME	(char *)-1

static char * genvHome ;

typedef struct cmd_t
{
	char * format ;
	char * param1 ;
	char * param2 ;
} CMD ;

typedef struct wrapper
{
	char * name ;
	CMD  * cmd ;
} WRAPPER ;

/*-----------------------------------------------------------------*/

int
Execute(CMD *cmd)
{
	char   buf[256] ;
	char * p1 ;
	char * p2 ;

	if( !cmd->format )
		return 0 ;

	p1 = (cmd->param1 == HOME ? genvHome : cmd->param1) ;
	p2 = (cmd->param2 == HOME ? genvHome : cmd->param2) ;
	sprintf(buf, cmd->format, p1, p2) ;
	fprintf(stdout, "  %s\n", buf) ;
	system(buf) ;

	return 1 ;
}  

/*-----------------------------------------------------------------*/

int
HandleUtil(char * arg)
{
	CMD    halt[] =
	{
		"killall xperform",									NULL,	NULL,
		"mv %s/.sgisession %s/sgisession >& /dev/null",		HOME,	HOME,
		"rm /etc/autologin.on",								NULL,	NULL,
		"endsession -f",									NULL,	NULL,
		NULL,												NULL,	NULL
	} ;
	CMD    setup[] =
	{
		"cp %s/xperform %s",								XDIR,	HOME,
		"cp %s/script %s",									XDIR,	HOME,
		"chmod 666 %s/script",								HOME,	NULL,
		"mv -f %s/.sgisession %s/.sgisession.qa",			HOME,	HOME,
		"cp %s/sgisession %s/.sgisession",					XDIR,	HOME,
		"chown root %s/xperform",							HOME,	NULL,
		"chmod 04755 %s/xperform",							HOME,	NULL,
		"mv -f /etc/autologin /etc/autologin.qa",			NULL,	NULL,
		"echo ${HOME} | awk -F/ '{print $NF}'  > /etc/autologin",					NULL,	NULL,
		"echo > /etc/autologin.on",							NULL,	NULL,
		NULL,												NULL,	NULL
	} ;
	CMD    start[] =
	{
		"rm %s/.xperform >& /dev/null",						HOME,	NULL,
		"mv %s/sgisession %s/.sgisession >& /dev/null",		HOME,	HOME,
		"echo > /etc/autologin.on",							NULL,	NULL,
		"endsession -f",									NULL,	NULL,
		NULL,												NULL,	NULL
	} ;
	CMD    unsetup[] =
	{
		"rm %s/xperform",									HOME,	NULL,
		"rm %s/script",										HOME,	NULL,
		"rm %s/.sgisession",								HOME,	NULL,
		"mv %s/.sgisession.qa %s/.sgisession >& /dev/null",	HOME,	HOME,
		"rm /etc/autologin",								HOME,	NULL,
		"mv /etc/autologin.qa /etc/autologin >& /dev/null",	NULL,	NULL,
		"rm /etc/autologin.on",								NULL,	NULL,
		NULL,												NULL,	NULL
	} ;
	CMD * ptr = NULL ;
	int   i ;

	if( strcmp(arg, "setup") == 0 )
		ptr = setup ;
	else if( strcmp(arg, "unsetup") == 0 )
		ptr = unsetup ;
	else if( strcmp(arg, "start") == 0 )
		ptr = start ;
	else if( strcmp(arg, "halt") == 0 )
		ptr = halt ;
	else if( strcmp(arg, "halt") == 0 )
		ptr = halt ;

	if( !ptr )	return 0 ;

	genvHome = getenv("HOME") ;

	*arg = toupper(*arg) ;
	fprintf(stdout, "\n-- %s --\n\n", arg) ;
	for( i = 0 ; Execute(&ptr[i]) ; i++ )
		;
	fprintf(stdout, "\n-- %s complete --\n\n", arg) ;

	return 1 ;
}


