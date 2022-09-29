/*****************************************************************************
 * Copyright 1996, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 ****************************************************************************/

/*
 * Print out the system numa statistics
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/syssgi.h>
#include <sys/pmo.h>
#include <sys/numa_stats.h>
#include <sys/signal.h>

#define	MAXNODES	64
#define NODE_BUFFER_SIZE 120

int verbose;
int _sgi_internal = 0;

extern void run_user_command(char *, char **);
extern void stats_show_diff(numa_stats_t *, numa_stats_t *);

void
usage(char* s)
{
        printf("Usage: %s [options] [nodepath . . .]\n", s);
        printf("Available Options:\n");
        printf("-h               Print this message\n");
        printf("-a               Print statistics for all nodes\n");
        printf("-v               Verbose\n");
        printf("-r command       Run command and print statistics for all nodes\n");
        printf("\n\n");
}

void
node_stat_display(char* node)
{
        numa_stats_t numa_stats;    

	if (syssgi(SGI_NUMA_STATS_GET, node, &numa_stats) < 0) {
		perror("syssgi SGI_NUMA_STATS_GET");
		exit(1);
	}

        if (!verbose) {
                printf("%s\t%08d\t%08d\t%08d\t%08d\t\n",
                       node,
                       numa_stats.refcnt_interrupt_number,      
                       numa_stats.migr_auto_number_out,
                       numa_stats.migr_user_number_out,
                       numa_stats.migr_coalescingd_number_out);
                return;
        } 

        printf("\n\nNUMA STATS FOR: %s\n", node);
        
        printf("Migration Threshold: %u\n", (uint)numa_stats.migr_threshold);
        printf("Counter Interrupts: %u\n", numa_stats.refcnt_interrupt_number);
        printf("Auto Migrations To Node: %u\n", numa_stats.migr_auto_number_in);
        printf("Auto Migrations From Node: %u\n", numa_stats.migr_auto_number_out);
        printf("Auto Migration Skips: %u\n", numa_stats.migr_auto_number_fail);
        printf("User Migrations To Node: %u\n", numa_stats.migr_user_number_in);
        printf("User Migrations From Node: %u\n", numa_stats.migr_user_number_out);
        printf("User Migration Skips: %u\n", numa_stats.migr_user_number_fail);
        printf("CoalD Migrations To Node: %u\n", numa_stats.migr_coalescingd_number_in);
        printf("CoalD Migrations From Node: %u\n", numa_stats.migr_coalescingd_number_out);
        printf("CoalD Migration Skips: %u\n", numa_stats.migr_coalescingd_number_fail);
        printf("Frozen Pages In Node: %u\n", numa_stats.migr_bouncecontrol_frozen_pages);        
        printf("Melted Pages In Node: %u\n", numa_stats.migr_bouncecontrol_melt_pages);
        
        if (_sgi_internal) {
                printf("refcnt_invalid_candidate_intrnumber: %u\n", numa_stats.refcnt_invalid_candidate_intrnumber);
                printf("refcnt_intrhandler_numloops: %u\n", numa_stats.refcnt_intrhandler_numloops);
                printf("refcnt_intrfail_absmigrnotfrz: %u\n", numa_stats. refcnt_intrfail_absmigrnotfrz);
                printf("refcnt_introk_absolute: %u\n", numa_stats.refcnt_introk_absolute);
                printf("refcnt_intrfail_relnotmigr: %u\n", numa_stats.refcnt_intrfail_relnotmigr);
                printf("refcnt_introk_relative: %u\n", numa_stats.refcnt_introk_relative);
                printf("migr_pagemove_number[MIGR_VEHICLE_IS_BTE]: %u\n", numa_stats.migr_pagemove_number[0]);
                printf("migr_pagemove_number[MIGR_VEHICLE_IS_TLBSYNC]: %u\n", numa_stats.migr_pagemove_number[1]);
                printf("migr_interrupt_failstate: %u\n", numa_stats.migr_interrupt_failstate);
                printf("migr_interrupt_failenabled: %u\n", numa_stats.migr_interrupt_failenabled);
                printf("migr_interrupt_failfrozen: %u\n", numa_stats.migr_interrupt_failfrozen);
                printf("migr_auto_number_queued: %u\n", numa_stats.migr_auto_number_queued);
                printf("migr_queue_number_in: %u\n", numa_stats.migr_queue_number_in);
                printf("migr_queue_number_out: %u\n", numa_stats.migr_queue_number_out);
                printf("migr_queue_number_fail: %u\n", numa_stats.migr_queue_number_fail);
                printf("migr_queue_number_overflow: %u\n", numa_stats.migr_queue_number_overflow);
                printf("migr_queue_number_failstate: %u\n", numa_stats.migr_queue_number_failstate);
                printf("migr_queue_number_failq: %u\n", numa_stats.migr_queue_number_failq);
                printf("migr_user_number_queued: %u\n", numa_stats.migr_user_number_queued);
                printf("pagealloc_lent_memory: %u\n", numa_stats.pagealloc_lent_memory);  
                printf("migr_unpegger_calls: %u\n", numa_stats.migr_unpegger_calls);
                printf("migr_unpegger_npages: %u\n", numa_stats.migr_unpegger_npages);
                printf("migr_unpegger_lastnpages: %u\n", numa_stats.migr_unpegger_lastnpages);
                printf("migr_bouncecontrol_calls: %u\n", numa_stats.migr_bouncecontrol_calls);
                printf("migr_bouncecontrol_last_melt_pages: %u\n", numa_stats.migr_bouncecontrol_last_melt_pages);
                printf("migr_bouncecontrol_dampened_pages: %u\n", numa_stats.migr_bouncecontrol_dampened_pages);
                printf("migr_queue_capacity_trigger_count: %u\n", numa_stats.migr_queue_capacity_trigger_count);
                printf("migr_queue_time_trigger_count: %u\n", numa_stats.migr_queue_time_trigger_count);
                printf("migr_queue_traffic_trigger_count: %u\n", numa_stats.migr_queue_traffic_trigger_count);
                printf("repl_pagecount: %u\n", numa_stats.repl_pagecount);
                printf("repl_page_destination: %u\n", numa_stats.repl_page_destination);
                printf("repl_page_reuse: %u\n", numa_stats.repl_page_reuse);
                printf("repl_control_refuse: %u\n", numa_stats.repl_control_refuse);
                printf("repl_page_notavail: %u\n", numa_stats.repl_page_notavail);
                
                printf("memoryd_periodic_activations: %u\n", numa_stats.memoryd_periodic_activations);
                printf("memoryd_total_activations: %u\n", numa_stats.memoryd_total_activations);        
                
                printf("migr_pressure_frozen_pages: %u\n", numa_stats.migr_pressure_frozen_pages);
                printf("migr_distance_frozen_pages: %u\n", numa_stats.migr_distance_frozen_pages);
                printf("migr_cost_frozen_pages: %u\n", numa_stats.migr_cost_frozen_pages);
        }
                
        /*printf("xxx: %u\n", numa_stats.xxx);*/
}


        
void
main(int argc, char** argv)
{
        char* node;
        char* command;
        int errflag;
        int migr;
        int config;
        int c;
        int allnodes;
	int run_command;

        errflag = 0;
        migr = 0;
        config = 0;
        node = NULL;
        allnodes = 0;
        run_command = 0;

        command = argv[0];
        
        if (argc < 2) {
                usage(command);
                exit(1);
        }                

        verbose = 0;
        
        while ((c = getopt(argc, argv, "havrs")) != -1) {
                switch (c) {
                case 'h':
                        errflag++;
                        break;
                case 'a':
                        allnodes++;
                        break;

		case 'v':
			verbose++;
			break;
                case 'r':
                        run_command++;
                        break;
                case 's':
                        _sgi_internal++;
                        break;         
                default:
                        errflag++;
                        break;
                }
        }
        
	if (run_command && allnodes) {
		fprintf(stderr,"nstats: -r and -a cannot be used together\n");
		usage(command);
		exit(1);
	}

        if (errflag) {
                usage(command);
                exit(1);
        }

        if (allnodes) {
                char *cmd = "/usr/bin/find /hw -name node -print";
                char node_buffer[NODE_BUFFER_SIZE];
                FILE* ptr;

                if ((ptr = popen(cmd, "r")) == NULL) {
                        perror("popen");
                        exit(1);
                }
                if (!verbose) {
                        printf("Summary of numa activity:\n");
                        printf("Node\t\t\t\tMigrIntrs\tAutoMigr\tUserMigr\tCoalMigr\n");
                }
                while (fgets(node_buffer, NODE_BUFFER_SIZE, ptr) != NULL) {
                        char* p = strchr(node_buffer, '\n');
                        if (p != NULL) {
                                *p = '\0';
                        }
                        node_stat_display(node_buffer);
                }

                if (pclose(ptr) < 0) {
                        perror("pclose");
                        exit(1);
                }
        } else if (run_command) {
			run_user_command(argv[optind], &argv[optind]);
	} else {
                if (!verbose) {
                        printf("Node\t\t\t\tMigrIntrs\tAutoMigr\tUserMigr\tCoalMigr\n");
                }
                for ( ; optind < argc; optind++) {
                        node_stat_display(argv[optind]);
                }
        }
        
        exit(0);
}

void
run_user_command(char *user_command, char **user_command_args)
{
	numa_stats_t numa_stats_base[MAXNODES], numa_stats_final[MAXNODES];

	char *cmd = "/usr/bin/find /hw -name node -print";
	char node_buffer[MAXNODES][NODE_BUFFER_SIZE];
	FILE* ptr;
	int	cur_node, numnodes, pid, status;

	if ((ptr = popen(cmd, "r")) == NULL) {
		perror("popen");
		exit(1);
	}

	numnodes = 0;
	while (fgets(node_buffer[numnodes], NODE_BUFFER_SIZE, ptr) != NULL) {
		char* p = strchr(node_buffer[numnodes], '\n');
		if (p != NULL) {
			*p = '\0';
		}
		if (syssgi(SGI_NUMA_STATS_GET, node_buffer[numnodes], 
					&numa_stats_base[numnodes]) < 0) {
			perror("syssgi SGI_NUMA_STATS_GET");
			exit(1);
		}
		numnodes++;
	}

	if (pclose(ptr) < 0) {
		perror("pclose");
		exit(1);
	}
	if ((pid = fork()) == 0) {
		setgid(getgid());
		execvp(user_command, user_command_args);
		fprintf(stderr,"Exec of %s failed\n",user_command);
		exit(1);
	}

	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	while(wait(&status) != pid);
	if((status&0377) != 0)
		fprintf(stderr,"Command terminated abnormally.\n");
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);

	for (cur_node = 0; cur_node < numnodes; cur_node++) {
		if (syssgi(SGI_NUMA_STATS_GET, node_buffer[cur_node], 
					&numa_stats_final[cur_node]) < 0) {
			perror("syssgi SGI_NUMA_STATS_GET");
			exit(1);
		}
	}
		
	for (cur_node = 0; cur_node < numnodes; cur_node++) {
		printf("\n\nREL NUMA STATS FOR: %s\n", node_buffer[cur_node]);
		if (verbose) {
			stats_show_diff(&numa_stats_final[cur_node], 
				&numa_stats_base[cur_node]);
		} else {

			printf("Number of pages migrated to this node : %u\n",
				numa_stats_final[cur_node].migr_auto_number_in - 
				numa_stats_base[cur_node].migr_auto_number_in);
			printf("Number of pages migrated out of this node : %u\n",
				numa_stats_final[cur_node].migr_auto_number_out - 
				numa_stats_base[cur_node].migr_auto_number_out);
			printf("Number of pages that failed to migrate : %u\n",
				numa_stats_final[cur_node].migr_auto_number_fail -
				numa_stats_base[cur_node].migr_auto_number_fail);
		}
        }
}
			
			
void
stats_show_diff(numa_stats_t *numa_stats_final, numa_stats_t *numa_stats_base)
{
        printf("Migration Threshold: %u\n", (uint)numa_stats_final->migr_threshold - (uint)numa_stats_base->migr_threshold);
        printf("Counter Interrupts: %u\n", numa_stats_final->refcnt_interrupt_number - numa_stats_base->refcnt_interrupt_number);
        printf("Auto Migrations To Node: %u\n", numa_stats_final->migr_auto_number_in - numa_stats_base->migr_auto_number_in);
        printf("Auto Migrations From Node: %u\n", numa_stats_final->migr_auto_number_out - numa_stats_base->migr_auto_number_out);
        printf("Auto Migration Skips: %u\n", numa_stats_final->migr_auto_number_fail - numa_stats_base->migr_auto_number_fail);
        printf("User Migrations To Node: %u\n", numa_stats_final->migr_user_number_in - numa_stats_base->migr_user_number_in);
        printf("User Migrations From Node: %u\n", numa_stats_final->migr_user_number_out - numa_stats_base->migr_user_number_out);
        printf("User Migration Skips: %u\n", numa_stats_final->migr_user_number_fail - numa_stats_base->migr_user_number_fail);
        printf("CoalD Migrations To Node: %u\n", numa_stats_final->migr_coalescingd_number_in - numa_stats_base->migr_coalescingd_number_in);
        printf("CoalD Migrations From Node: %u\n", numa_stats_final->migr_coalescingd_number_out - numa_stats_base->migr_coalescingd_number_out);
        printf("CoalD Migration Skips: %u\n", numa_stats_final->migr_coalescingd_number_fail - numa_stats_base->migr_coalescingd_number_fail);
        printf("Frozen Pages In Node: %u\n", numa_stats_final->migr_bouncecontrol_frozen_pages - numa_stats_base->migr_bouncecontrol_frozen_pages);        
        printf("Melted Pages In Node: %u\n", numa_stats_final->migr_bouncecontrol_melt_pages - numa_stats_base->migr_bouncecontrol_melt_pages); 
}
