/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1986, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>

extern char* optarg;

char* error_string = "usage: affinity -t on|off [-p pid] | [-c cmd args]\n";
int main(int argc, char **argv) 
{
        int c;
	int status = -5;
        pid_t pid = 0;
        int command = 0;
        int pidset = 0;
        int toggleset = 0;
        int toggle = 0;
        while ((c = getopt(argc, argv, "t:p:c:")) != -1) {
                switch (c) {
                case 't':
                        if (strcmp(optarg,"on") == 0) 
                                toggleset = toggle = 1;
                        else if (strcmp(optarg, "off") == 0) {
                                toggleset = 1;
                                toggle = 0;
                        }
                        break;
                case 'p':
                        sscanf(optarg, "%d", &pid);
                        pidset = 1;
                        break;
                case 'c':
                        command = optind - 1;
                        break;
                default:
                        fprintf (stderr, error_string);
                        exit(1);
                }
                if (command) break;
                
        }
        if (!toggleset) {
                fprintf (stderr, "Did not specify or incorrectly specified toggle value. \n %s\n", 
                         error_string);
                exit(1);
        }

        if (pidset && command) {
                fprintf (stderr, "Specified pid and command. \n %s\n", 
                         error_string);
                exit(1);
        }
        if (!pidset && !command) 
                pid = getppid();
 	if (command)
		pid = getpid();        

	if (toggle && schedctl(AFFINITY_ON, pid) == -1) {
		perror ("Could not turn affinity on");
		exit(1);
	} else if (!toggle && schedctl(AFFINITY_OFF, pid) == -1) {
		perror ("Could not turn affinity off");
		exit(1);
        }
	if (!command) return 1;
        execvp(argv[command],&argv[command]);
        perror("Could not exec process");
}
        


