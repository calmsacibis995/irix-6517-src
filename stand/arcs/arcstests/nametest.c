
#include <saioctl.h>

char *make_bootfile(int);

/*
 * test make_bootname
 */
main(int argc, char **argv, char **envp)
{
    char *bf;

    for (bf = make_bootfile(1); bf; bf = make_bootfile(0))
	printf ("%s\n", bf);
    return;
}
