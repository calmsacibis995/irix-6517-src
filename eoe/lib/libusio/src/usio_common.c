#include <sys/serialio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "usio_common.h"

void *
usio_init(int fd)
{
    int id;

    id = ioctl(fd, SIOC_USIO_GET_MAPID);

    switch(id) {
      case USIO_MAP_IOC3:
	return(usio_ioc3_init(fd));
      default:
	return(0);
    }
}

int
usio_write(void *private, char *buf, int len)
{
    return((*(struct usio_vec**)private)->write(private, buf, len));
}

int
usio_read(void *private, char *buf, int len)
{
    return((*(struct usio_vec**)private)->read(private, buf, len));
}

int
usio_get_status(void *private)
{
    return((*(struct usio_vec**)private)->get_status(private));
}
