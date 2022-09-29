#include <fcntl.h>
#include "trace.h"


main(int argc, char **argv)
{
        struct nameent nameent;
        int fd;

        fd = open(argv[1], O_RDONLY);
        while (read(fd, &nameent, sizeof nameent)) {
                printf("%s 0x%x\n",nameent.name, nameent.ord);
        }
}
