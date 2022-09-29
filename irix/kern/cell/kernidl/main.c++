#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include "globals.h"
#include "node.h"

// Global variables (see descriptions in "globals.h"):
//
char *myname;
int main_argc;
char **main_argv;
char *output_dir;

static struct msgtypes {
    char *name;
    msgsystem_t type;
    int (*fcn)();
} msgnames[] = {
    { "xpc", MSG_XPC, emit_files },
};
static int nmsgtypes = sizeof(msgnames) / sizeof(char *);

static void delete_tree(list_t &parsetree);
#if defined(DUMP_TREE)
static void dump_tree(list_t &parsetree);
#endif

void
main(int argc, char **argv)
{
    struct msgtypes *msgsystem = &msgnames[0];

    // Squirrel away main program parameters.
    //
    myname = strrchr(argv[0], '/');
    if (myname) myname++;
    else        myname = argv[0];

    // parse off kerndil specific args - these MUST be first
    // and separated by a '--' from other cpp args.
    int c;
    while ((c = getopt(argc, argv, "T:D:")) != EOF) {
	switch (c) {
	case 'T':
		int i;
		for (i = 0; i < nmsgtypes; i++) {
			if (strcmp(optarg, msgnames[i].name) == 0) {
				msgsystem = &msgnames[i];
				break;
			}
		}
		if (i == nmsgtypes) {
			fprintf(stderr, "Unknown msg type %s\n", optarg);
			exit(1);
		}
		break;
	case 'D':
		output_dir = optarg;

		struct stat sb;
		if (stat(output_dir, &sb) != 0) {
			fprintf(stderr, "Cannot stat %s:%s\n", output_dir,
				strerror(errno));
			exit(1);
		}
		break;
	// all other args are for compiles etc..
	}
    }
    main_argc = argc - optind;
    main_argv = &argv[optind];

    if (output_dir == NULL)
	output_dir = ".";

    // Fork off child cpp to handle doing the ``cpp-thang'' to the input file.
    // the parent will process the output of cpp in yyparse().
    //
    int pfd[2];
    pid_t pid;
    if (pipe(pfd) < 0 || (pid = fork()) < 0) {
	perror(myname);
	exit(EXIT_FAILURE);
    }
    if (pid == 0) {
	// Child: close read side of pipe and stdout; dup write side of pipe
	// onto stdout; exec cpp ...
	close(pfd[0]);
	close(STDOUT_FILENO);
	if (fcntl(pfd[1], F_DUPFD, STDOUT_FILENO) < 0) {
		perror(myname);
		exit(EXIT_FAILURE);
	}
	main_argv--;
	main_argc++;
	execv(_PATH_CPP, main_argv);
	fprintf(stderr, "%s: unable to execute %s: %s\n", myname, _PATH_CPP,
		strerror(errno));
	exit(EXIT_FAILURE);
    }
    // Parent: close write side of pipe and stdin; dup read side of pipe onto
    // stdin (for yyparse() below).
    close(pfd[1]);
    close(STDIN_FILENO);
    fcntl(pfd[0], F_DUPFD, STDIN_FILENO);

    // Process output of cpp and wait for cpp child to exit.
    //
    yyparse();
    if (compilation_errors) {
	fprintf(stderr, "%s: compile failed with %d error%s\n", myname,
		compilation_errors, compilation_errors == 1 ? "" : "s");
	delete_tree(parsetree);
	exit(EXIT_FAILURE);
    }
    int child_stat;
    wait(&child_stat);
    if (WEXITSTATUS(child_stat)) {
	delete_tree(parsetree);
	exit(WEXITSTATUS(child_stat));
    }

    #if defined(DUMP_TREE)
	dump_tree(parsetree);
    #endif

    // Emit output files and clean up.
    //
    int status = (msgsystem->fcn)();
    delete_tree(parsetree);
    exit(status ? EXIT_SUCCESS : EXIT_FAILURE);
}


static void
delete_tree(list_t &parsetree)
{
    listiter_t iter(parsetree);
    idl_node_t *nodep;
    while ((nodep = (idl_node_t *)iter.next()) != NULL) {
	nodep->unlink();
	delete nodep;
    }
}


#if defined(DUMP_TREE)
static void
dump_tree(list_t &parsetree)
{
    // XXX need to fill in ...
}
#endif /* DUMP_TREE */
