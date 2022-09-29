#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

/*ARGSUSED*/
main(int argc, char **argv)
{
    char		*name;
    char        	host[MAXHOSTNAMELEN];
    struct hostent      *hep = NULL;

    if (argc == 1) {
	(void)gethostname(host, MAXHOSTNAMELEN);
	name = host;
    }
    else if (argc == 2 && argv[1][0] != '-')
	name = argv[1];
    else {
	fprintf(stderr, "Usage: pmhostname [hostname]\n");
	exit(0);
    }

    hep = gethostbyname(name);
    if (hep == NULL)
        printf("%s\n", name);
    else
        printf("%s\n", hep->h_name);

    exit(0);
    /*NOTREACHED*/

}
