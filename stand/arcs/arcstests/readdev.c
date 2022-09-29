#include <arcs/io.h>

main(int argc, char **argv, char **envp)
{
    LONG errno;
    ULONG fd,rc;

    errno = Open ((CHAR *)argv[1], OpenReadOnly, &fd);

    if (errno) {
	printf("open FAILED: %s\n",argv[1]);
	perror(errno, 0);
    }
    else {
	printf ("%s opened OK\n", argv[1]);
    }

    while (1) {
	char buf=0;
	errno=Read(fd,&buf,1,&rc);
	if (rc == 0) goto done;
	printf("%x %c\n",buf,buf);
    }

done:
    Close(fd);
    return;
}
