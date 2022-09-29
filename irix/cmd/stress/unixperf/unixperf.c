
/* unixperf - an x11perf-style Unix performance benchmark */

/* unixperf.c contains the main benchmark harness. */

#include "unixperf.h"

#include <string.h>
#include <signal.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/sysmp.h>

#define TOTAL_NUM_TESTS (sizeof(tests)/sizeof(Test))

static int progress = FALSE, describeIt = TRUE;
static unsigned int LoopCnt = 0;  /* overrides time-based duration */

void
Usage(void)
{
    int i;
    char **alias;
    static char *usage_msg[] =
    {
        "usage: unixperf [-options ...]",
        "where options include:",
        "    -repeat <n>          do tests <n> times (default = 5)",
        "    -time <s>            do tests for ~<s> seconds each (default = 5)",
        "    -loop <n>            loop exactly <n> times for a given test",
        "    -progress            report progress to stderr",
        "    -quiet               supress reporting system description",
        "    -tmpdir <dir>        dir to make files under (default=/usr/tmp)",
	"    -mp <n>              run n copies of the tests in parallel",
	"    -mustrun <cpu-list>  run on cpus in cpu-list",
        "    -all                 do all tests",
        NULL};

    for (i = 0; usage_msg[i] != NULL; i++) {
        printf("%s\n", usage_msg[i]);
    }
    for (i = 0; i < NumberOfTests; i++) {
        printf("    %-18s   %s\n", tests[i].name, tests[i].description);
    }
    for (i = 0; i < NumberOfAliases; i++) {
        printf("    %-18s   alias for", aliases[i].name);
        alias = aliases[i].list;
        while (*alias != NULL) {
            printf(" %s", *alias);
            alias++;
        }
        printf("\n");
    }
    printf("\n");
    exit(1);
}

/* CheckTestName - select test for execution if state is TRUE, otherwise
   deselect the test. */
static Bool
CheckTestName(char *name, int state)
{
    char **alias;
    int i;

    for (i = 0; i < NumberOfTests; i++) {
        if (!strcmp(name, tests[i].name)) {
            tests[i].doTest = state;
            return TRUE;
        }
    }
    for (i = 0; i < NumberOfAliases; i++) {
        if (!strcmp(name, aliases[i].name)) {
            alias = aliases[i].list;
            while (*alias != NULL) {
                CheckTestName(*alias, state);
                alias++;
            }
            return TRUE;
        }
    }
    return FALSE;
}

void
ParseArguments(int argc, char *argv[])
{
    DIR *dir;
    int numTests;
    int i, j;

    if (argc < 2)
        Usage();
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-all")) {
            for (j = 0; j < NumberOfTests; j++) {
                /* The TF_NONSTANDARD is marked for tests that shouldn't be
                   run unless explicitly called out for running.  An example
                   of such tests are TCP tests that create TCP connections at 
                   a high rate.  These tests are both unstable and can affect 
                   the timing of later tests when the kernel finally expires
                   hundreds of abandoned TCP connections. */
                if (!(tests[j].flags & TF_NONSTANDARD)) {
                    tests[j].doTest = CHOOSE_ALL;
                }
            }
        } else if (!strcmp(argv[i], "-progress")) {
            progress = TRUE;
        } else if (!strcmp(argv[i], "-quiet")) {
            describeIt = FALSE;
        } else if (!strcmp(argv[i], "-repeat")) {
            i++;
            if (argc <= i)
                Usage();
            TestRepeats = atoi(argv[i]);
            if (TestRepeats <= 0)
                Usage();
        } else if (!strcmp(argv[i], "-time")) {
            i++;
            if (argc <= i)
                Usage();
            TestSeconds = atoi(argv[i]);
            if (TestSeconds <= 0)
                Usage();
        } else if (!strcmp(argv[i], "-loop")) {
            i++;
            if (argc <= i)
                Usage();
            LoopCnt = atoi(argv[i]);
            if (LoopCnt <= 0)
                Usage();
        } else if (!strcmp(argv[i], "-tmpdir")) {
            i++;
            if (argc <= i)
                Usage();
            TmpDir = argv[i];
            dir = opendir(TmpDir);
            if (dir == NULL) {
                printf("Invalid temporary directory specified: %s\n\n", TmpDir);
                Usage();
            }
            closedir(dir);
	} else if (!strcmp(argv[i], "-mp")) {
		if (MpFlags & U_MP_MULTI) {
			printf("-mp must only be specified once\n\n");
			Usage();
		}
		i++;
		if (argc <= i)
			Usage();
		MpNum = atoi(argv[i]);
		if (MpNum <= 1) {
			printf("-mp argument must be greater than 1\n\n");
			Usage();
		}
		MpFlags |= U_MP_MULTI;
		MpChildren = calloc(sizeof(pid_t), MpNum);
		memset(MpMustrunMask, 0, sizeof(MpMustrunMask));
	} else if (!strcmp(argv[i], "-mustrun")) {
		if (MpFlags == U_MP_SINGLE) {
			printf("-mustrun option requires -mp\n\n");
			Usage();
		}
		i++;
		if (argc <= i) {
			printf("-mustrun option requires an argument\n\n");
			Usage();
		}
		MpFlags |= U_MP_MUSTRUN;
		if (!MpParseProcStr(MpMustrunMask, argv[i])) {
			Usage();
		}
        } else {
            Bool found;

            /* Treat "--blah" as indicating that test -blah should be
               supressed. */
            if (argv[i][0] == '-' && argv[i][1] == '-') {
                found = CheckTestName(argv[i] + 1, CHOOSE_NOT);
            } else {
                found = CheckTestName(argv[i], CHOOSE_SELF);
            }
            if (!found)
                Usage();
        }
    }

    if (MpFlags & U_MP_MULTI) {
	    for (j = 0; j < NumberOfTests; j++) {
		if ((tests[j].flags & TF_NOMULTI) &&
			tests[j].doTest &&
			!(tests[j].doTest & CHOOSE_SELF)) {
			tests[j].doTest = CHOOSE_NOT;
		}
	    }
    }

    numTests = 0;
    for (j = 0; j < NumberOfTests; j++) {
        if (tests[j].doTest) {
            numTests++;
        }
    }
    if (numTests == 0)
        Usage();
}

void
RunTests(void)
{
    double elapsed, totalTime;
    unsigned int count, reps, i, j;
    Bool runnable;

    for (i = 0; i < NumberOfTests; i++) {
        if (tests[i].doTest) {
	    int nproblems = 0;

	    if (MpFlags & U_MP_MULTI) {
		    if (MpBeginTest())
			continue;
	    }

            runnable = (tests[i].initProc) (TestVersion);
            if (runnable) {
                if (progress) {
                    fprintf(stderr, "Running: %s\n", tests[i].description);
                }
                TestProblemOccured = 0;
                if (!(tests[i].flags & TF_NOWARMUP)) {
                    if (LoopCnt == 0) {
                        /* do time-based looping */
                        count = tests[i].warmupCount;
                        for (;;) {
                            reps = 0;
                            StartStopwatch();
                            while (reps < count) {
                                reps += (tests[i].proc) ();
                            }
                            elapsed = StopStopwatch();
                            (tests[i].passCleanup) ();

                            /* won't settle for less than .1 second warmup */
                            if (elapsed >= 100000.0)
                                break;

                            /* try more counts */
                            count *= 2;
                        }
                        count = reps / elapsed * 1000000.0 * TestSeconds;
                        count++;  /* add one to make sure >0 */
                    } else {
                        /* loop an explicit number of times */
                        count = LoopCnt;
                    }
                } else {
                    /* Just use the warm up count. */
                    count = tests[i].warmupCount;
                }
                totalTime = 0;
                for (j = 0; j < TestRepeats; j++) {
                    reps = 0;
                    StartStopwatch();
                    while (reps < count) {
                        reps += (tests[i].proc) ();
                    }
                    elapsed = StopStopwatch();
                    (tests[i].passCleanup) ();
                    totalTime += elapsed;
                    if (TestProblemOccured) {
                        printf("**  The following result is suspect; problems occurred during test.\n");
                        TestProblemOccured = 0;
			nproblems++;
                    }
                    ReportTimes(elapsed, reps, tests[i].description, FALSE);
                }
                if (TestRepeats > 1) {
                    ReportTimes(totalTime,
                        reps * TestRepeats, tests[i].description, TRUE);
                }
                (tests[i].finalCleanup) ();
            } else {
                printf("Unable to test: %s\n", tests[i].description);
            }
            printf("\n");
            fflush(stdout);

	    if (MpFlags & U_MP_MULTI)
		    MpEndTest(nproblems);
        }
    }
}

void
Cleanup(void)
{
    RemoveAnyTmpFiles();
    exit(1);
}

void
main(int argc, char *argv[])
{
    chdir("/");
    ParseArguments(argc, argv);
    if (describeIt)
        DescribeSystem();
    (void) signal(SIGINT, Cleanup);  /* ^C */
    (void) signal(SIGQUIT, Cleanup);
    (void) signal(SIGTERM, Cleanup);
    (void) signal(SIGHUP, Cleanup);
    RunTests();
    exit(0);
}
