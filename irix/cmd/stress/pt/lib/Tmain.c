/*
	Surrogate main() for testing.
	Selects and runs test from user supplied table.
 */

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <Tst.h>


static int tstRunByName(Tst_t *, char *);
static int tstRun(Tst_t *);
static void tstDetails(char *);


main(int argc, char *argv[])
{
#define ARGS	"LUO:"
#define USAGE	"Usage: %s [-L][-U][-O <output>\n"

	int	c, errflg = 0;
	Tst_t	*t;
	char	*outstr = 0;
	int	errs = 0;

	optarg = NULL;
	while (!errflg && (c = getopt(argc, argv, ARGS)) != EOF) {
		switch (c) {
		case 'L'	:	/* list */
			printf("Table: %s // %s\n",
				TstTable[0].t_name,
				TstTable[0].t_synopsis);
			tstDetails(TstTable[0].t_details);

			for (t = TstTable + 1; t->t_name; t++) {
				printf("  ---\nTest: %s // %s\n",
					t->t_name,
					t->t_synopsis);
				tstDetails(t->t_details);
			}
			exit(0);
			break;
		case 'O'	:	/* output */
			outstr = optarg;
			break;
		case 'U'	:
			printf(USAGE, argv[0]);
			printf(
				"\tL\t- list test details\n"
				"\tU\t- usage message\n"
				"\tO I\t- info msgs\n"
				"\t  A\t- admin msgs\n"
				"\t  T\t- trace msgs\n"
			);
			exit(0);
			break;
		default :
			errflg++;
		}
	}
	ChkExp( !errflg && optind <= argc, (USAGE, argv[0]) );

	if (!outstr) {
		outstr = getenv("TST_OUTPUT");
	}
	TstSetOutput(outstr);

	if (optind == argc) {
		errs = tstRunByName(TstTable, 0);
	} else {
		argv += optind;
		do {
			errs += tstRunByName(TstTable, *argv);
		} while (*++argv);
	}

	return (errs);
}


/*
 * Run tests by name, null string or entry 0 entry runs all tests.
 */
static int
tstRunByName(Tst_t *tbl, char *str)
{
	Tst_t	*t;
	int	errs;

	if (str == 0 || !strcmp(tbl[0].t_name, str)) {

		TstInfo("\n%s: begin\n", tbl[0].t_name);

		for (t = tbl + 1; t->t_name; t++) {
			errs = tstRun(t);
		}

		TstInfo("%s: end\n", tbl[0].t_name);

	} else {

		for (t = tbl + 1; t->t_name; t++) {
			if (!strcmp(t->t_name, str)) {
				errs = tstRun(t);
				break;
			}
		}
		if (!t->t_name) {
			TstExit("tstRun(%s) not found\n", str);
		}
	}
	TstAdmin("%s: %s\n", tbl[0].t_name, errs ? "FAILED" : "PASSED");

	return (errs);
}


static int
tstRun(Tst_t *t)
{
	int	ret;

	TstInfo("%s: %s: begin\n", t->t_name, t->t_synopsis);

	ret = (*t->t_func)();

	TstInfo("%s: end\n", t->t_name);

	return (ret);
}


/*
 * Cheap and nasty details string indent routine
 */
static void
tstDetails(char *details)
{
	if (!details || !*details) {
		return;
	}
	fputs(details, stdout);
}
