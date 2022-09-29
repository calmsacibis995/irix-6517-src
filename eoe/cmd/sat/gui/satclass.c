#include <sys/sat.h>

int	sat_eventtoclass( int );
char *	sat_classtostr( int );

#ifndef	NULL
#define	NULL	0
#endif

#define SAT_CL_NONE		0
#define SAT_CL_PATHNAME		1
#define SAT_CL_FILEDES		2
#define SAT_CL_PROCESS		3
#define SAT_CL_SVIPC		4
#define SAT_CL_SOCKET		5
#define SAT_CL_PUBLIC		6
#define SAT_CL_CONTROL		7
#define SAT_CL_INTERNET		8
#define SAT_CL_ADMIN		9
#define	SAT_NUM_CLASSES		SAT_CL_ADMIN


/*
 * sat_eventtoclass - return the event class that a particular event belongs to
 */
int
sat_eventtoclass( int ev )
	{
	     if ( ev <= 0 )
		return SAT_CL_NONE;
	else if ( ev < SAT_FCHDIR )
		return SAT_CL_PATHNAME;
	else if ( ev < SAT_FORK )
		return SAT_CL_FILEDES;
	else if ( ev < SAT_SVIPC_ACCESS )
		return SAT_CL_PROCESS;
	else if ( ev < SAT_BSDIPC_CREATE )
		return SAT_CL_SVIPC;
	else if ( ev < SAT_CLOCK_SET )
		return SAT_CL_SOCKET;
	else if ( ev < SAT_CHECK_PRIV )
		return SAT_CL_PUBLIC;
	else if ( ev < SAT_BSDIPC_RX_OK )
		return SAT_CL_CONTROL;
	else if ( ev < SAT_USER_RECORDS )
		return SAT_CL_INTERNET;
	else if ( ev < SAT_NTYPES )
		return SAT_CL_ADMIN;
	else
		return SAT_CL_NONE;
	}


static	struct {
	int	id;
	char *	name;
	}	classes[] = {
	SAT_CL_NONE,		"Not an event class",
	SAT_CL_PATHNAME,	"Path name events",
	SAT_CL_FILEDES,		"File descriptor events",
	SAT_CL_PROCESS,		"Process events",
	SAT_CL_SVIPC,		"System V IPC events",
	SAT_CL_SOCKET,		"BSD IPC socket layer events",
	SAT_CL_PUBLIC,		"Public object events",
	SAT_CL_CONTROL,		"Control and Privilege events",
	SAT_CL_INTERNET,	"BSD IPC IP layer events",
	SAT_CL_ADMIN,		"User level administrative events",
	NULL,			NULL,
	};


/*
 * sat_classtostr - convert an event class index into a class name string
 */
char *
sat_classtostr( int class )
	{
	static	char	class_string[72];
	int	i;

	if ( class <= 0 || class > SAT_NUM_CLASSES )
		return NULL;

	for ( i=0; classes[i].name != NULL; i++ ) {
		if ( classes[i].id == class ) {
			strcpy( class_string, classes[i].name );
			return class_string;
		}
	}

	return NULL;
	}


