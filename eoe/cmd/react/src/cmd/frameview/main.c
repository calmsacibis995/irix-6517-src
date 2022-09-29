/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************

 **************************************************************************
 *	 								  *
 * File:	main.c						          *
 *									  *
 *			                				  *
 * Copyright (c) 1994 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/


#include <stdio.h>
#define MAIN
#include "frameview.h"

#define PORT 22785
#define PORT_NUMB_RETRIES 100
int act_port;

void old_main(int argc, char **argv);
void parse_args(int argc, char **argv);
void parse_proc_str(char *proc_str);


/* our child will tell us when it's about to die by sending us a 
   signal USR1, this just allows us to do a wait and avoid having
   defunct processes sitting around */
void
sig_handler()
{
    int www;
    wait(&www);
}

/* our parent (master) has told us that it was issued a quit so 
   we need to also */
void
sig_int_handler()
{
    exit_frameview();
}

main(int argc, char** argv)
{
    char numb_procs_str[32];
    int i;
    
    sysinfo (_MIPS_SI_NUM_PROCESSORS, numb_procs_str, 32);
    numb_processors = atoi(numb_procs_str);

    if (getenv("TMPDIR") == NULL)
	strcpy(tmp_dir_str, "/usr/tmp/");
    else {
	strcpy(tmp_dir_str, getenv("TMPDIR"));
	strcat(tmp_dir_str, "/");
      }

    parse_args(argc, argv);

    master_main(argc, argv);    


/*
    if (argc > 1) {
	init_stats();
	parse_args(argc, argv);
	old_main();
    }
    else {

    }
  */  
}

void
old_main(int argc, char** argv)
{
    char numb_procs_str[32];
    int i;

    parent_id = get_pid();
    signal(SIGUSR1, sig_handler);

    signal(SIGINT, sig_int_handler);

    usconfig(CONF_INITUSERS, MAX_PROCESSORS+1);

    for (i=0; i<MAX_PROCESSORS; i++) {
	if (cpu_on[i]) {
	    init_no_frame_event_list(i);
	    start_stats(i);
	}
    }
    
    if (sproc(display_graphics, PR_SALL, NULL) == -1) {
	grtmon_message("couldn't display graphics", FATAL_MSG, 40);
	exit(0);
    }

    while(rtmon_program_running)
	sginap(100);
    
}



char base_filename[128];

#define USAGE "frameview [-f filename] {default filename}\n                 [-k on/off] {kernel mode}\n                 [-m message level] {NOTICE, ALERT, WARNING, ERROR}\n                 [-n on/off] {no frame mode}\n"

#define OLD_USAGE "frameview \n          [-d] turn debugging on\n          [-f filename]\n          [-h hostname]\n          [-j major frame event start]\n          [-k minor frame event start]          [-l] turn logging on\n          [-n] force no frame event mode\n          [-o] disable collecting no frame events\n          [-p processor set]\n          [-r] enable rading from live stream\n          [-t frame trigger event]\n"

void
parse_args(int argc,char **argv) 
{
    extern char * optarg; 
    int c;

    strcpy(cl_filename, "");
    strcpy(cl_kern_mode, "");
    strcpy(cl_message_level, "");
    strcpy(cl_no_frame_mode, "");

    while (( c = getopt(argc, argv, "f:k:m:n:")) != EOF) 
	switch(c) {
	  case 'f':
	    strcpy(cl_filename, optarg);
	    break;
	  case 'k':
	    strcpy(cl_kern_mode, optarg);
	    if (!((strcmp(cl_kern_mode, "on") == 0) || 
		  (strcmp(cl_kern_mode, "off") == 0))) {
		fprintf(stderr, "usage: %s", USAGE);
		exit(0);
	    }
	    break;
	  case 'm':
	    strcpy(cl_message_level, optarg);
	    if (!((strcmp(cl_message_level, "22785") == 0) ||
		  (strcmp(cl_message_level, "NOTICE") == 0) ||
		  (strcmp(cl_message_level, "ALERT") == 0) ||
		  (strcmp(cl_message_level, "WARNING") == 0) ||
		  (strcmp(cl_message_level, "ERROR") == 0))) {
		fprintf(stderr, "usage: %s", USAGE);
		exit(0);
	    }
	    break;
	  case 'n':
	    strcpy(cl_no_frame_mode, optarg);
	    if (!((strcmp(cl_no_frame_mode, "on") == 0) || 
		  (strcmp(cl_no_frame_mode, "off") == 0))) {
		fprintf(stderr, "usage: %s", USAGE);
		exit(0);
	    }
	    break;
	  case '?':
	  default :
	    fprintf(stderr, "usage: %s", USAGE);
	    exit(0);
	}

}

void
old_parse_args(int argc,char **argv) 
{
    extern char * optarg; 
    int c;
    char proc_str[128];
    int got_file, got_proc, got_host;
    int i;
    char open_file[128];
    int count;
    int sock;
    int fromlen;
    struct sockaddr_in from;
    char sys_str[128];
    char msg_str[512];

    major_frame_start_event = TSTAMP_EV_START_OF_MAJOR;
    minor_frame_start_event = TSTAMP_EV_START_OF_MINOR;
    frame_trigger_event = TSTAMP_EV_INTRENTRY;
    got_file = got_proc = got_host = 0;
    debug = FALSE;
    logging = FALSE;
    live_stream = FALSE;
    show_live_stream = FALSE;
    gethostname(hostname, 128);

    while (( c = getopt(argc, argv, "df:h:j:k:lonp:rt:")) != EOF) 
	switch(c) {
	  case 'd':
	    debug = TRUE;
	    break;
	  case 'f':
	    got_file = 1;
	    strcpy(base_filename, optarg);
	    break;
	  case 'h':
	    got_host = 1;
	    strcpy(hostname, optarg);
	    break;
	  case 'j':
	    sscanf(optarg, "%d", &major_frame_start_event);
	    break;
	  case 'k':
	    sscanf(optarg, "%d", &minor_frame_start_event);
	    break;
	  case 'l':
	    logging = TRUE;
	    break;
	  case 'n':
	    no_frame_mode = TRUE;
	    break;
	  case 'o':
	    collecting_nf_events = FALSE;
	    break;
	  case 'p':
	    got_proc = 1;
	    strcpy(proc_str, optarg);
	    break ; 
	  case 'r':
	    live_stream = TRUE;
	    show_live_stream = TRUE;
	    break;
	  case 't':
	    sscanf(optarg, "%d", &frame_trigger_event);
	    break;
	  case '?':
	  default :
	    sprintf(msg_str, "usage: %s", USAGE);
	    grtmon_message(msg_str, FATAL_MSG, 41);

	    exit(0);
	}

    if (live_stream) {
	if (got_proc) {
	    parse_proc_str(proc_str);
	}
	if (! logging) {
	    grtmon_message("without logging you will not be able to go back to frames that have already gone by.", WARNING_MSG, 42);
	}
	if (logging) {
	    if (got_file) {
		; /* we already have a base filename */
	    }
	    else {
		strcpy(base_filename, "fv-data");
	    }
	    for (i=0; i<MAX_PROCESSORS; i++) {
		if (cpu_on[i]) {
		    sprintf(open_file, "%s.%d.wvr", base_filename, i);

		    fd[i] = open(open_file, O_RDONLY);
		    if (fd[i] >= 0) {
			sprintf(msg_str, 
				"file %s already exists, moving to %s.old",
			       open_file, open_file);
			grtmon_message(msg_str, ALERT_MSG, 43);

			sprintf(sys_str, "mv %s %s.old", open_file, open_file);
			system(sys_str);
			close(fd[i]);
		    }
		}
	    }
	    for (i=0; i<MAX_PROCESSORS; i++) {
		if (cpu_on[i]) {
		    sprintf(open_file, "%s.%d.wvr", base_filename, i);
		    fd_fvw[i] = open(open_file, O_WRONLY|O_CREAT,0666);
		    if (fd_fvw[i] <0) {
			sprintf(msg_str, 
				"could not open %s for writing errno %d", 
				open_file, errno);
			grtmon_message(msg_str, ERROR_MSG, 44);
			exit(0);
		    }
		    fd_fvr[i] = open(open_file, O_RDONLY);
		    if (fd_fvr[i] <0) {
			sprintf(msg_str, 
			       "(1) could not open %s for reading errno %d", 
			       open_file, errno);
			grtmon_message(msg_str, ERROR_MSG, 45);
			exit(0);
		    }		    
		}
	    }
	}
	getting_data_from = REAL_DATA;
	scan_type = REAL_DATA;
	count = 0;

	fprintf(stderr, "should not be in old parse args - otherwise fix the followinf\n");
	/*
	        for (i=0; i<MAX_PROCESSORS; i++) {
		if (cpu_on[i]) {
		count++;
		sock = init_socket();

		if (listen(sock, 5) < 0) {
		grtmon_message("listen failed", ERROR_MSG, 53);
		perror("listen");
		exit(0);
		}
		
		rpc_start(i);
		
		fromlen = sizeof(from);
		if ((fd[i] = accept(sock, &from, &fromlen)) < 0) {
		sprintf(msg_str, "accept failed for cpu %d\n", i);
		grtmon_message(msg_str, ERROR_MSG, 46);
		    exit(0);
		    }
		    
		    sprintf(msg_str, "found fd %d for cpu %d\n", fd[i], i);
		    grtmon_message(msg_str, DEBUG_MSG, 48);
		    
		    }
		    }

	  */
	if (count == 0) {
	    grtmon_message("no cpus for real-time stream", FATAL_MSG, 49);
	    exit(0);
	}
	return;
    }
    else if (got_file) {
	if (got_host) {
	    grtmon_message("You can not read from a file and get live data.  Host request ignored.", ALERT_MSG, 50);
	}
	if (logging) {
	    grtmon_message("It does not make sense both to read from and log to a file.  Logging has been disabled", ALERT_MSG, 51);
	    logging = FALSE;
	}
	if (got_proc)
	    parse_proc_str(proc_str);
	else 
	    get_procs_from_filelist(base_filename);

	count = 0;
	for (i=0; i<MAX_PROCESSORS; i++) {
	    if (cpu_on[i]) {
		count++;
		sprintf(open_file, "%s.%d.wvr", base_filename, i);
		fd[i] = open(open_file, O_RDONLY);
		if (fd[i] <0) {
		    sprintf(msg_str, "(2) couldn't open %s for reading errno %d",
			    errno,open_file);
		    grtmon_message(msg_str, ERROR_MSG, 52);
		}
	    }
	}
	if (count == 0) {
	    sprintf(msg_str, "could not find any files with %s prefix\n",base_filename);
	    grtmon_message(msg_str, ERROR_MSG, 54);
	    exit(0);
	}
	getting_data_from = FILE_DATA; 
	scan_type = FILE_DATA; 

	return;
    }
    else if (got_host) {
	if (! live_stream) {
	    grtmon_message("real-time stream not enabled - host ignored", ALERT_MSG, 55);
	    goto check_gen_files;
	}
    }
    else if (logging) {
	if(! live_stream) {
	    grtmon_message("real-time stream not enabled - logging request ignored", ALERT_MSG, 56);
	    goto check_gen_files;
	} 
    }
    else {
	major_frame_account_valid = 1;
	return;
    }
  check_gen_files:
    strcpy(base_filename, "");
    if (got_proc) {
	get_procs_from_filelist(base_filename);
	for (i=0;i<MAX_PROCESSORS;i++) cpu_on[i] = FALSE;
	parse_proc_str(proc_str);
    }
    else 
	get_procs_from_filelist(base_filename);
    
    for (i=0; i<MAX_PROCESSORS; i++) {
	if (cpu_on[i]) {
	    sprintf(open_file, "%s.%d.wvr", base_filename, i);
	    fd[i] = open(open_file, O_RDONLY);
	    if (fd[i] <0) {
		sprintf(msg_str, 
			"(3) couldn't open %s for reading errno %d",
			open_file, errno);
		grtmon_message(msg_str, ERROR_MSG, 57);
		perror(" could not be opened");
		exit(0);
	    }
	}
    }

}




void
parse_proc_str(char *proc_str)
{
    char temp_str[32];
    int i, str_len, pos;
    int j, cpu;
    int first_cpu;
    char msg_str[64];

    first_cpu = -1;
    str_len = strlen(proc_str);
    i = 0;
    while (i<str_len) {
	pos = 0;
      next_char:
	if ((proc_str[i] >= '0') && (proc_str[i] <= '9')) {
	    temp_str[pos] = proc_str[i];
	}
	else {
	    grtmon_message("incorrect format for processor list, ex: 1,3-6", ERROR_MSG, 58);
	    exit(0);
	}
	i++;
	pos++;
	if (proc_str[i] == ',') {
	  convert_number:
	    temp_str[pos] = '\0';
	    cpu = atoi(temp_str);
	    if (first_cpu == -1) {
		first_cpu = cpu;
	    }
	    if (first_cpu > cpu) {
		grtmon_message("error: incorrect format for processor list, ex: 1,3-6", ERROR_MSG, 59);
		exit(0);
	    }
	    for (j=first_cpu; j<=cpu; j++) {
		if (j >= MAX_PROCESSORS) {
		    /* = on an 8 processor machine the last processor is 7 */
		    sprintf(msg_str, 
			    "requested processor %d out of range [0-%d]",
			    j, MAX_PROCESSORS-1);
		    grtmon_message(msg_str, ERROR_MSG, 60);
		    exit(0);
		}
		if (cpu_on[j] == TRUE) {
		    sprintf(msg_str, "cpu %d alrady enabled\n",j);
		    grtmon_message(msg_str, NOTICE_MSG, 61);
		}

		else
		    cpu_on[j] = TRUE;
	    }
	    first_cpu = -1;
	    if (i==str_len) goto end_proc_str;
	    i++; 
	    pos = 0;
	}
	else if (proc_str[i] == '-') {
	    if (first_cpu != -1) {
		grtmon_message("incorrect format for processor list, ex: 1,3-6", ERROR_MSG, 62);
		exit(0);
	    }
	    temp_str[pos] = '\0';
	    i++;
	    pos = 0;
	    first_cpu = atoi(temp_str);
	}
	else {
	    if (i==str_len) {
		goto convert_number;
	    }
	    else
		goto next_char;
	}
    }
  end_proc_str:
    if (first_cpu != -1) {
	grtmon_message("incorrect format for processor list, ex: 1,3-6", ERROR_MSG, 63);
	exit(0);
    }


}


init_socket()
{
#define HNLEN 128
    int s;
    struct hostent *hp;
    struct sockaddr_in sin, from;
    char msg_str[64];
    char myhostname[HNLEN];

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1) {
	grtmon_message("socket failed", ERROR_MSG, 64);
	perror("socket");
	exit(1);
    }

    gethostname(myhostname, HNLEN);
    hp = gethostbyname(myhostname);

    if (hp == NULL) {
	sprintf(msg_str, "host \"%s\" not found.\n", hostname);
	grtmon_message(msg_str, ERROR_MSG, 65);
	exit(1);
    }

    bzero((char *) &sin, sizeof(sin));
    bcopy(hp->h_addr, (char *) &sin.sin_addr, hp->h_length);

    sin.sin_family = hp->h_addrtype;
    sin.sin_port = PORT;


  try_again:
    if (bind(s, (caddr_t) &sin, sizeof(sin)) < 0) {
	sin.sin_port = sin.sin_port + 1;
	sprintf(msg_str, "retrying port %d\n", sin.sin_port);
	grtmon_message(msg_str, DEBUG_MSG, 66);
	if (sin.sin_port < (PORT_NUMB_RETRIES + PORT))
	    goto try_again;

	grtmon_message("bind failed", ERROR_MSG, 67);

	perror("bind");
	exit(1);
    }
    
    act_port = sin.sin_port;

    return(s);
}
