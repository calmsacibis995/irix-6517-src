#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/utsname.h>
#include <ctype.h>

static
void
Usage(void) {
    printf(" dlook [-sample n] \n");
    printf("       [-mapped]\n");
    printf("       [-text]\n");
    printf("       [-p[hyiscal]]\n");
    printf("       [-c[color]]\n");
    printf("       [-v[erbose]]\n");
    printf("       [-out file]\n");
    printf("         program [args]\n");
}

static char dlook_period[] =        "__DLOOK_PERIOD_=0\0          ";
static char dlook_mapped[] =        "__DLOOK_MAPPED_=0\0             ";
static char dlook_text[]   =        "__DLOOK_TEXT_=0\0             ";
static char dlook_outputfile[] =    "__DLOOK_OUTPUTFILE_=stderr\0                                                                                      "; 
static char dlook_verbose[] =       "__DLOOK_VERBOSE_=off\0             "; 
static char dlook_print_physical[] ="__DLOOK_PRINT_PHYSICAL_=off\0      "; 
static char dlook_print_color[] =   "__DLOOK_PRINT_COLOR_=off\0      "; 

int main(int argc, char *argv[]) {
    char filename[BUFSIZ], buffer[BUFSIZ], *c;
    int  n, status;
  
    argc--; argv++;
    /* defaults */
    if (NULL == (c=getenv("_RLD_LIST"))) {
        putenv("_RLD_LIST=DEFAULT:libdlook.so");
    } else {
        strcpy(buffer,"_RLD_LIST=");
        strcat(buffer,c);
	strcat(buffer,":libdlook.so");
        putenv(buffer);
    }

    while (argc) {
        if (argv[0][0] == '-') {
            if (!strcmp(argv[0],"-h")) {
                Usage();
                exit(1);
            } else if (!strcmp(argv[0],"-mapped")) {
                sprintf(dlook_mapped,"__DLOOK_MAPPED_=on");
                putenv(dlook_mapped);
                argc--; argv++;
            } else if (!strcmp(argv[0],"-text")) {
                sprintf(dlook_text,"__DLOOK_TEXT_=on");
                putenv(dlook_text);
                argc--; argv++;
            } else if (!strcmp(argv[0],"-verbose")||!strcmp(argv[0],"-v")) {
                sprintf(dlook_verbose,"__DLOOK_VERBOSE_=on");
                putenv(dlook_verbose);
                argc--; argv++;
            } else if (!strcmp(argv[0],"-physical")||!strcmp(argv[0],"-p")) {
                sprintf(dlook_print_physical,"__DLOOK_PRINT_PHYSICAL_=on");
                putenv(dlook_print_physical);
                argc--; argv++;
            } else if (!strcmp(argv[0],"-color")||!strcmp(argv[0],"-c")) {
                sprintf(dlook_print_color,"__DLOOK_PRINT_COLOR_=on");
                putenv(dlook_print_color);
                argc--; argv++;
            } else if (!strcmp(argv[0],"-sample")) {
                double d;
                if (argc == 1 || 1 != sscanf(argv[1],"%lg",&d)) {
                    fprintf(stderr,
                        "option '%s' needs an numerical argument.\n",
                        argv[0]);
                    exit(1);
                } else {
                    n = d * 1000000; /* convert to micro seconds */
                    sprintf(dlook_period,"__DLOOK_PERIOD_=%d",n);
                    putenv(dlook_period);
                }
                argc -= 2; argv += 2;
            } else if (!strcmp(argv[0],"-out")) {
                if (argc == 1 || 1 != sscanf(argv[1],"%s",filename)) {
                    fprintf(stderr,"option '%s' needs an argument.",argv[0]);
                    exit(1);
                } else {
                    if('/' == filename[0]) {
                        sprintf(dlook_outputfile,"__DLOOK_OUTPUTFILE_=%s",filename);
                    } else {
                        char path[128];
                        if(!getcwd(path,128)) {
                            fprintf(stderr,"getcwd failed.\n");
                            exit(1);
                        }
                        sprintf(dlook_outputfile,"__DLOOK_OUTPUTFILE_=%s/%s",
                            path,filename);
                    }
                    putenv(dlook_outputfile);
                    unlink(filename);
                }
                argc -= 2; argv += 2;
            } else {
                fprintf(stderr,"unknown option '%s'\n",argv[0]);
                Usage();
                exit(1);
            }
        } else {
            break;
        }
    }

    if (argc == 0) {
        Usage();
        exit(1);
    }

    /* program should be in argv[0] */
    /* fork child as executable from cmd line */
    if ((n=fork()) < 0) {
        fprintf(stderr,"fork failed.\n");
        exit(1);
    }
    if(n == 0) {
        setgid(getgid());
        execvp(*argv, argv);
        fprintf(stderr,"Exec of %s failed\n",*argv);
        exit(1);
    }
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    while (wait(&status) != n);
    if ((status&0377) != 0)
        fprintf(stderr,"Command terminated abnormally.\n");
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    return 0;
}
