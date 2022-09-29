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
 * File:	init.c						          *
 *									  *
 * Description:	This file contains all the initialization routines   	  *
 *              for the real-time monitoring tools                        *
 *                                                               	  *
 *			                				  *
 * Copyright (c) 1994 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/


#define MAIN extern
#include "frameview.h"


#include <sys/procfs.h>
#include <sys/dir.h>
#include <paths.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <rpc/rpc.h>

#define FRAMEVIEW_ARENA ".frameview_arena"

void init_pid_map_names();
void init_tstamp_event_names();

void
exit_frameview()
{
    int i;
    char sys_str[64];

    sprintf(sys_str, "rm -f %s%s.%d", tmp_dir_str, FRAMEVIEW_ARENA, my_extension);
    system(sys_str);


    for (i=0; i<MAX_PROCESSORS; i++) {
	sprintf(sys_str, "rm -f %s_%d.%d", tmp_dir_str, NO_FRAME_FILENAME, my_extension,i);
	system(sys_str);

	if (cpu_on[i])
	    kill(read_stream_proc_id[i], SIGINT);
    }
    kill(parent_id, SIGINT);

    rtmon_program_running = FALSE;

/*    kill(graphics_proc_id, SIGINT);*/
    exit(0);

}


void
map_timer()
{
    volatile unsigned* phys_addr;
    long cycleval;
    int fd;

    /* map the cycle counter into user space */
    phys_addr = (volatile unsigned *)syssgi(SGI_QUERY_CYCLECNTR, &cycleval);
    if ((__psint_t)phys_addr == -1) {
	perror("syssgi(SGI_QUERY_CYCLECNTR, ...)");
	exit(0);
    }
    if ((fd = open("/dev/mmem", O_RDONLY)) == -1) {
	perror("open /dev/mmem");
	exit(0);
    }

    if ((timer_addr = (volatile long long *)mmap(0, 8, PROT_READ, MAP_PRIVATE, 
					    fd, (__psint_t)phys_addr)) == (long long *)(-1)) {
	perror("mmap");
	exit(0);
    }
    if (close(fd) < 0) {
	perror("close /dev/mmem");
	exit(0);
    }

}


void
init_stats()
{
    int i;
    char tstamp_name[20];
    char sys_str[64];
    char arena[64];
    FILE *fp;
    char msg_str[64];

    rtmon_program_running = TRUE;
    template_matching = TRUE;

    glob_getting_data = TRUE;
    erase_event_list = TRUE;
    reading_count = 0;
    major_frame_account_valid = 0;
    tstamp_adj_time = 5;
    computed_minor_time = 0;
    absolute_start_time = 0x7fffffffffffffff;
    absolute_start_count = 0;
    absolute_start_access = 0;
    
    i = 0;
    sprintf(arena, "%s%s.%d", tmp_dir_str, FRAMEVIEW_ARENA, i);
    while ((fp = fopen(arena, "r")) != NULL) {
	fclose(fp);
	i++;
	sprintf(arena, "%s%s.%d", tmp_dir_str, FRAMEVIEW_ARENA, i);
    }
    my_extension = i;

    sem_shared_arena = usinit(arena);
    sprintf(sys_str, "chmod 666 %s", arena);
    system(sys_str);
    if (sem_shared_arena == NULL) {
	if (debug) {
	    sprintf(msg_str, "could not initialize arena, errno %d\n", errno);
	    grtmon_message(msg_str, FATAL_MSG, 34);
	}
	exit(0);
    }
    event_list_sem = usnewsema(sem_shared_arena, 0);
    usvsema(event_list_sem);

    defining_template = 0;
    defining_template_cpu = -1;

    for (i=0; i<MAX_PROCESSORS; i++) {
	cpu_on[i] = FALSE;
	fix_template[i] = 0;
	fd[i] = NULL;
	fd_fvw[i] = NULL;
	fd_fvr[i] = NULL;
    }

    for (i=0; i<MIN_USER_ID; i++) {
	sprintf(tstamp_name, "%s%d", "undefined ", i);
	strcpy(tstamp_event_name[i], tstamp_name);
    }
    for (i=MIN_USER_ID; i<MIN_KERNEL_ID; i++) {
	sprintf(tstamp_name, "%s%d", "user event ", i-MIN_USER_ID);
	strcpy(tstamp_event_name[i], tstamp_name);
    }
    for (i=MIN_KERNEL_ID; i<MAX_TSTAMP_NUMB; i++) {
	sprintf(tstamp_name, "%s%d", "undefined ", i);
	strcpy(tstamp_event_name[i], tstamp_name);
    }
    
/*    map_timer();*/

    init_tstamp_event_names();

    init_pid_map_names();
}

void
init_pid_map_names()
{
    int i;
    char name_str[16];

    for (i=0;i<MAX_PID;i++) {
	sprintf(name_str, "pid-%d\n", i);
	strcpy(pid_name_map[i], name_str);
    }

}

void
init_choice_events()
{
    int i;

    i=0;

    strcpy(trigger_event[i].name, "start of major");
    trigger_event[i].numb = TSTAMP_EV_START_OF_MAJOR;
    strcpy(maj_event[i].name, "start of major");
    maj_event[i].numb = TSTAMP_EV_START_OF_MAJOR;
    strcpy(min_event[i].name, "start of major");
    min_event[i].numb = TSTAMP_EV_START_OF_MAJOR;
    default_maj_event = TSTAMP_EV_START_OF_MAJOR;
    choice_maj_event = i;
    i++;

    strcpy(trigger_event[i].name, "start of minor");
    trigger_event[i].numb = TSTAMP_EV_START_OF_MINOR;
    strcpy(maj_event[i].name, "start of minor");
    maj_event[i].numb = TSTAMP_EV_START_OF_MINOR;
    strcpy(min_event[i].name, "start of minor");
    min_event[i].numb = TSTAMP_EV_START_OF_MINOR;
    default_min_event = TSTAMP_EV_START_OF_MINOR;
    choice_min_event = i;
    i++;

    strcpy(trigger_event[i].name, "interrupt entry");
    trigger_event[i].numb = TSTAMP_EV_INTRENTRY;
    strcpy(maj_event[i].name, "interrupt entry");
    maj_event[i].numb = TSTAMP_EV_INTRENTRY;
    strcpy(min_event[i].name, "interrupt entry");
    min_event[i].numb = TSTAMP_EV_INTRENTRY;
    default_trigger_event = TSTAMP_EV_INTRENTRY;
    choice_trigger_event = i;
    i++;

    strcpy(trigger_event[i].name, "yield");
    trigger_event[i].numb = TSTAMP_EV_YIELD;
    strcpy(maj_event[i].name, "yield");
    maj_event[i].numb = TSTAMP_EV_YIELD;
    strcpy(min_event[i].name, "yield");
    min_event[i].numb = TSTAMP_EV_YIELD;
    i++;

    strcpy(trigger_event[i].name, "user defined");
    trigger_event[i].numb = 0;
    strcpy(maj_event[i].name, "user defined");
    maj_event[i].numb = 0;
    strcpy(min_event[i].name, "user defined");
    min_event[i].numb = 0;
    i++;

    numb_trigger_events = numb_maj_events = numb_min_events = i;
}

void
init_tstamp_event_names()
{
    strcpy(tstamp_event_name[3], "name task");
    strcpy(tstamp_event_name[52], "end switch");
    strcpy(tstamp_event_name[54], "end switch rtpri");
    strcpy(tstamp_event_name[56], "run idle");
    strcpy(tstamp_event_name[61], "statechange");
    strcpy(tstamp_event_name[101], "intr exit");
    strcpy(tstamp_event_name[102], "intr entry");
    strcpy(tstamp_event_name[103], "event entry");
    strcpy(tstamp_event_name[107], "yield");
    strcpy(tstamp_event_name[108], "r4kcounter intr");
    strcpy(tstamp_event_name[109], "cccounter intr");
    strcpy(tstamp_event_name[110], "profcounter intr");
    strcpy(tstamp_event_name[111], "group intr");
    strcpy(tstamp_event_name[113], "cpu intr");
    strcpy(tstamp_event_name[114], "net intr");
    strcpy(tstamp_event_name[600], "fork");
    strcpy(tstamp_event_name[601], "exit");
    strcpy(tstamp_event_name[631], "timeout");
    strcpy(tstamp_event_name[10005], "end resume");
    strcpy(tstamp_event_name[10024], "signal received");
    strcpy(tstamp_event_name[10027], "signal sent");
    strcpy(tstamp_event_name[30000], "insert exit to user");
    strcpy(tstamp_event_name[60000], "undef system tstamp");
    strcpy(tstamp_event_name[60001], "undef daemon tstamp");
    strcpy(tstamp_event_name[60002], "end disp");
    strcpy(tstamp_event_name[60003], "yield2");
    strcpy(tstamp_event_name[60004], "over run");
    strcpy(tstamp_event_name[40005], "intr qualfier");
    strcpy(tstamp_event_name[60006], "timein");

    strcpy(tstamp_event_name[TSTAMP_EV_DETECTED_UNDERRUN], "under run");
    strcpy(tstamp_event_name[TSTAMP_EV_DETECTED_OVERRUN], "over run");
    strcpy(tstamp_event_name[TSTAMP_EV_RECV_NORECOVERY], "no recover");
    strcpy(tstamp_event_name[TSTAMP_EV_RECV_INJECTFRAME], "inject");
    strcpy(tstamp_event_name[TSTAMP_EV_RECV_EFRAME_STRETCH], "stretch");
    strcpy(tstamp_event_name[TSTAMP_EV_RECV_EFRAME_STEAL], "steal");
    strcpy(tstamp_event_name[TSTAMP_EV_RECV_TOOMANY], "too many");

    strcpy(tstamp_event_name[TSTAMP_EV_START_OF_MAJOR], "start of major");
    strcpy(tstamp_event_name[TSTAMP_EV_START_OF_MINOR], "start of minor");

    strcpy(tstamp_event_name[START_KERN_FUNCTION], "start K funct");
    strcpy(tstamp_event_name[END_KERN_FUNCTION],   "end   K funct");
}


void
start_stats(int cpu)
{
    int local_cpu;
    char filename[32];
    char msg_str[64];

    local_cpu = cpu;

    init_stat_structures(cpu);

    ave_sem[cpu] = usnewsema(sem_shared_arena, 1);



    verify_on[cpu] = FALSE;
    if ((read_stream_proc_id[cpu] = sproc(read_stream, PR_SALL, &local_cpu)) == -1) {
	sprintf(msg_str, "couldn't start get events errno %d\n", errno);
	grtmon_message(msg_str, FATAL_MSG, 35);
	exit_frameview();
    }

    while(! verify_on[cpu]) ;


}


get_procs_from_filelist(char *base_filename)
{
    DIR      *dd;
    FILE *fp;
    char file_str[256], new_str[256];
    int i,j,pos;
    struct direct       *ent;
    int count;
    char cpu_str[128], cmp_str[128];
    int cpu;
    char base_dirname[128];
    char msg_str[64];

    count = 0;


    strcpy(base_dirname, ".");
    for (i=0; i<strlen(base_filename); i++) {
	if (base_filename[i] == '/') {
	    strcpy(base_dirname, base_filename);
	    base_dirname[i] = '\0';
	}
    }

    if (strlen(base_filename) == 0) {
	if ((dd = opendir(base_dirname)) == NULL) {
	    sprintf(msg_str, "couldn't open directory %s for reading\n",base_dirname);
	    grtmon_message(msg_str, FATAL_MSG, 36);
	    exit_frameview();
	}
	
	
	while (ent = readdir(dd)) {
	    if (strcmp(ent->d_name, ".") == 0 ||
		strcmp(ent->d_name, "..") == 0)
		continue;
	    if (strstr(ent->d_name, ".wvr") != NULL) {
		strcpy(new_str, strstr(ent->d_name, ".wvr"));
		strncpy(base_filename, ent->d_name, 
			strlen(ent->d_name) - strlen(new_str));
		pos = strlen(base_filename);
		while (base_filename[pos] != '.') pos--;
		base_filename[pos] = '\0';
		base_filename[strlen(ent->d_name) - strlen(new_str)] = '\0';
		closedir(dd);
		goto get_procs;
	    }
	}
	closedir(dd);
	return(0);
    }

  get_procs:
    if ((dd = opendir(base_dirname)) == NULL) {
	sprintf(msg_str, "couldn't open directory %s for reading\n",base_dirname);
	grtmon_message(msg_str, FATAL_MSG, 37);
	exit_frameview();

    }
	
    sprintf(cmp_str, "%s.", base_filename);
      for (i=0; i<strlen(base_filename); i++) {
	if (base_filename[i] == '/') {
	    for (j=i+1; j<=strlen(base_filename); j++) {
		cmp_str[j-(i+1)] = base_filename[j];
	    }
	}
    }
    while (ent = readdir(dd)) {
	if (strcmp(ent->d_name, ".") == 0 ||
	    strcmp(ent->d_name, "..") == 0)
	    continue;
	if (strstr(ent->d_name, cmp_str) != NULL) {
	    if (strstr(ent->d_name, ".wvr") != NULL) {
		/* verify that it ends in .wvr */
		if ((ent->d_name[strlen(ent->d_name) - 4] == '.') &&
		    (ent->d_name[strlen(ent->d_name) - 3] == 'w') &&
		    (ent->d_name[strlen(ent->d_name) - 2] == 'v') &&
		    (ent->d_name[strlen(ent->d_name) - 1] == 'r')) {
		    strcpy(new_str, strstr(ent->d_name, "."));
		    pos = 0;
		    while (new_str[pos+1] != '.') {
			cpu_str[pos] = new_str[pos+1];
			pos++;
		    }
		    cpu_str[pos] = '\0';
		    cpu = atoi(cpu_str);
		    if (cpu >= MAX_PROCESSORS) {
			sprintf(msg_str, "processor %d out of range [0-%d]\n", 
			       cpu, MAX_PROCESSORS - 1);
			grtmon_message(msg_str, FATAL_MSG, 38);
			exit_frameview();
		    }
		    if (cpu_on[cpu]) {
			grtmon_message("two files for the same processor were found", WARNING_MSG, 39);
		    }
		    cpu_on[cpu] = TRUE;
		    count++;
		}
	    }
	}
    }

    closedir(dd);
    return(count);

}
