#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "../inc/courier/courier.h"
#include "../inc/port/port.h"

#include "../inc/erpc/netadmp.h"
#include "netadm.h"
#include "netadm_err.h"

#define NETADM_PORT 530

main()

{

    UINT32              send_param;
    int                 return_code;
    struct sockaddr_in  sin;
    struct hostent      *hp;


    hp = gethostbyname("annexpilot");
    if (hp == 0)
	{
	printf("gethostbyname failed\n");
	exit();
	}

    bzero((char *)&sin,sizeof(sin));
    sin.sin_family = hp->h_addrtype;
    bcopy(hp->h_addr, (char *)&sin.sin_addr, hp->h_length);
    sin.sin_port = htons(NETADM_PORT);

    printf("value to set dla param to? ");
    scanf("%D", &send_param);
    printf("calling set_dla_param()\n");
    return_code = set_dla_param(&sin, 1, 1, LONG_CARDINAL_P,
     (char *)&send_param);
    printf("return_code is %d.\n", return_code);

}
