#ident	"lib/libsc/cmd/rb_cmd.c: $Revision: 1.3 $"

/*
 * Commands to test the restart block save/restore
 */

#include <arcs/restart.h>
#include <libsc.h>

int
rb_cmd (int argc, char **argv, char **envp)
{
    int i, newargc;
    char **env;
    char **newargv, **newenvp;

    printf ("Saving to restart block:\n");
    printf ("\targc = %d\n", argc);
    for (i = 0; i < argc; ++i)
	printf ("\targv[%d] = %s\n", i, argv[i]);
    for (i = 0, env = envp; env && *env; ++env, ++i)
	printf ("\tenvp[%d] = %s\n", i, *env);

    if (save_rb (argc, argv, envp)) {
	printf ("Save of environment failed.\n");
	return 0;
    }

    if (restore_rb (&newargc, &newargv, &newenvp)) {
	printf ("Restore of environment failed.\n");
	return 0;
    }

    printf ("+++++++++++++++++++++++++++\n");

    printf ("Restored environment:\n");
    printf ("\tnewargc = %d\n", newargc);
    for (i = 0; i < newargc; ++i)
	printf ("\tnewargv[%d] = %s\n", i, newargv[i]);
    for (i = 0; newenvp && *newenvp; ++newenvp, ++i)
	printf ("\tnewenvp[%d] = %s\n", i, *newenvp);

    return 0;
}
