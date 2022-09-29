#ident	"lib/libsc/cmd/env_cmd.c: $Revision: 1.31 $"

/*
 * Commands that deal with the environment variables
 */

#include <stringlist.h>
#include <saioctl.h>
#include <arcs/io.h>
#include <arcs/errno.h>
#include <libsc.h>

char *find_str(char *, struct string_list *);

extern struct string_list environ_str;

static char *readonly_errmsg = "Environment variable \"%s\" is"
    " informative only and cannot be changed.\n";

static char *nvram_errmsg = "Error writing \"%s\" to the non-volatile RAM.\n"
    "Variable \"%s\" is not saved in the permanent environment.\n";

/*
 * setenv_cmd -- set environment variables
 */

/*ARGSUSED*/
int
setenv_cmd(int argc, char **argv, char **bunk1, struct cmd_table *bunk2)
{
    int rval, override;
    char *var, *val;
    ULONG fd;

    /*
     * usage should be
     *		setenv variable newvalue, or
     *		setenv -f variable newvalue
     *		setenv -p variable newvalue ("persistent" variable)
     *		    (not supported on all machines)
     */
#ifdef VERBOSE_XXX
    printf("setenv_cmd(%d,", argc);
    { int i;
      for (i=1; i<argc; i++) printf("%s,",argv[i]);
      printf(")\n");
    }
#endif
    if (argc == 4) {
	if (strcmp (argv[1], "-f") == 0) {
	    override = 1;
	    var = argv[2];
	    val = argv[3];
	}
	else if (strcmp (argv[1], "-p") == 0) {
	    override = 2;
	    var = argv[2];
	    val = argv[3];
	}
	else
	    return(1);
    }
    else if (argc != 3)
	return(1);
    else {
	override = 0;
	var = argv[1];
	val = argv[2];
    }

    rval = _setenv (var, val, override);
    
    if (rval == EACCES) {
	printf(readonly_errmsg, var);
	return 0;
    }
    else if (rval != 0)
	printf(nvram_errmsg, var, var);

    /* other special environment variables
     */
    if (!strcasecmp(var, "dbaud") &&
	(Open((CHAR *)"serial(0)", OpenReadWrite, &fd) == ESUCCESS)) {
	    ioctl(fd, TIOCREOPEN, 0);
	    Close(fd);
    }

    if (!strcasecmp(var, "rbaud") &&
	(Open((CHAR *)"serial(1)", OpenReadWrite, &fd) == ESUCCESS)) {
	    ioctl(fd, TIOCREOPEN, 0);
	    Close(fd);
    }

    return(0);
}

/*
 * unsetenv_cmd -- unset environment variables
 */

/*ARGSUSED*/
int
unsetenv_cmd(int argc, char **argv, char **bunk1, struct cmd_table *bunk2)
{
    int rval;

    if (argc != 2)
	return(1);

    if ((rval = _setenv (argv[1], "", 2)) != 0) {
	if (rval == EACCES)
	    printf(readonly_errmsg, argv[1]);
	else
	    printf(nvram_errmsg, argv[1], argv[1]);
    }

    return(0);
}

/*
 * printenv -- show an environment variable
 */
void
printenv(char *var)
{
    char *cp;

    if (cp = find_str(var, &environ_str))
	    printf("%s\n", cp);
}

/*
 * printenv -- show environment variables
 */

/*ARGSUSED*/
int
printenv_cmd(int argc, char **argv, char **bunk1, struct cmd_table *bunk2)
{
    int i;

    if (argc == 1) {
	for (i = 0; i < environ_str.strcnt; i++)
	    printf("%s\n", environ_str.strptrs[i]);
    }
    else while (--argc > 0)
	printenv(*++argv);

    return(0);
}
