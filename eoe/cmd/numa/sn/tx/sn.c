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
 * Simple command line interface to control
 * numa parameters for SN0
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/syssgi.h>
#include <sys/pmo.h>
#include <sys/migr_parms.h>
#include "sn.h"

typedef struct migr_parms_update {
        int migr_base_mode_update;
        int migr_base_threshold_update;
        int migr_unpegging_control_enabled_update;
        int migr_unpegging_control_threshold_update;
        int migr_unpegging_control_interval_update;
        int migr_auto_migr_mech_update;
        int migr_user_migr_mech_update;
        int migr_queue_control_interval_update;
        int migr_queue_capacity_trigger_threshold_update;
        int migr_queue_time_trigger_interval_update;
	int migr_vehicle_mode_update;
        int migr_dampening_enabled_update;
        int migr_dampening_factor_update;
        int migr_min_distance_update;
        int migr_refcnt_mode_update;
} migr_parms_update_t;

#define NODE_BUFFER_SIZE 4096

char* command = 0;
        

void
usage(char* s)
{
        printf("Usage: %s [options] <nodepath . . .> | %s -a [options]\n", s, s);
        printf("Available Options (thresholds in range [0..100], intervals in seconds):\n");
        printf("-v                   Show full configuration (default)\n");
        printf("-s                   Silent Operation\n");
        printf("-a                   Apply to all nodes\n");
        printf("-b <on|off>          Turn on/off BTE for migration\n");	 
        printf("-m <foff|fon|on|off> Set migration mode to always off, always on, normally on, normally off\n");
        printf("-t <threshold>       Set default migration threshold\n");
        printf("-z <distance>        Set the cutoff distance for migration\n");
        printf("-u <off|on>          Turn unpegger off or on\n");
        printf("-p <threshold>       Set unpegger pegged level\n");
        printf("-w <interval>        Set unpegger period\n");
#ifdef NONO        
        printf("-c <i|q>             Set auto migration mechanism to immediate or queued\n");
        printf("-d <i|q>             Set user migration mechanism to immediate or queued\n");
        printf("-q <interval>        Set migration queue control interval\n");
        printf("-g <threshold>       Set migration queue capacity trigger threshold\n");
        printf("-x <interval>        Set migration queue time trigger interval\n");
#endif        
        printf("-i <on|off>          Enable/disable migration dampening\n");
        printf("-f <factor>          Set dampening factor (percentage)\n");
        printf("-r <foff|fon|on|off> Set refcnt mode to always off, always on, normally on, normally off\n");
        printf("\n\n");
}

void
migr_parms_update(char* node,
                  migr_parms_update_t* mpu,
                  migr_parms_t* migr_parms_new,
                  int showconfig)
{
        migr_parms_t migr_parms_base;
        int update = 0;
        
        /*
         * Get the current parameters
         */
        if (syssgi(SGI_NUMA_TUNE, MIGR_PARMS_GET, node, &migr_parms_base) < 0) {
                perror("syssgi, MIGR_PARMS_GET");
                usage(command);
                exit(1);
        }
        if (mpu->migr_base_mode_update) {
                migr_parms_base.migr_as_kparms.migr_base_mode =
                        migr_parms_new->migr_as_kparms.migr_base_mode;
                update++;
        }


        if (mpu->migr_base_threshold_update) {
                migr_parms_base.migr_as_kparms.migr_base_threshold =
                        migr_parms_new->migr_as_kparms.migr_base_threshold;
                update++;
        }

        if (mpu->migr_unpegging_control_enabled_update) {
                migr_parms_base.migr_system_kparms.migr_unpegging_control_enabled =
                        migr_parms_new->migr_system_kparms.migr_unpegging_control_enabled;
                update++;
        }

        if (mpu->migr_unpegging_control_threshold_update) {
                migr_parms_base.migr_system_kparms.migr_unpegging_control_threshold =
                        migr_parms_new->migr_system_kparms.migr_unpegging_control_threshold;
                update++;
        }

        if (mpu->migr_unpegging_control_interval_update) {
                migr_parms_base.migr_system_kparms.migr_unpegging_control_interval =
                        migr_parms_new->migr_system_kparms.migr_unpegging_control_interval;
                update++;
        }

        if (mpu->migr_auto_migr_mech_update) {
                migr_parms_base.migr_system_kparms.migr_auto_migr_mech =
                        migr_parms_new->migr_system_kparms.migr_auto_migr_mech;
                update++; 
        }  

        if (mpu-> migr_user_migr_mech_update) {
                migr_parms_base.migr_system_kparms. migr_user_migr_mech =
                        migr_parms_new->migr_system_kparms. migr_user_migr_mech;
                update++;
        }  

        if (mpu->migr_queue_control_interval_update) {
                migr_parms_base.migr_system_kparms.migr_queue_control_interval =
                        migr_parms_new->migr_system_kparms.migr_queue_control_interval;
                update++;
        }  

        if (mpu->migr_queue_capacity_trigger_threshold_update) {
                migr_parms_base.migr_system_kparms.migr_queue_capacity_trigger_threshold =
                        migr_parms_new->migr_system_kparms.migr_queue_capacity_trigger_threshold;
                 update++;
        }
        if (mpu->migr_queue_time_trigger_interval_update) {
                migr_parms_base.migr_system_kparms.migr_queue_time_trigger_interval =
                        migr_parms_new->migr_system_kparms.migr_queue_time_trigger_interval;
                update++;
        }

	if (mpu->migr_vehicle_mode_update) {
		migr_parms_base.migr_system_kparms.migr_vehicle = 
			migr_parms_new->migr_system_kparms.migr_vehicle;
                update++;
	}

        if (mpu->migr_dampening_enabled_update) {
                migr_parms_base.migr_as_kparms.migr_dampening_enabled =
                        migr_parms_new->migr_as_kparms.migr_dampening_enabled;      
                update++;
        }

        if (mpu->migr_dampening_factor_update) {
                migr_parms_base.migr_as_kparms.migr_dampening_factor =
                        migr_parms_new->migr_as_kparms.migr_dampening_factor;      
                update++;
        }

        if (mpu->migr_min_distance_update) {
                migr_parms_base.migr_as_kparms.migr_min_distance =
                        migr_parms_new->migr_as_kparms.migr_min_distance;
                        update++;
        }

        if (mpu->migr_refcnt_mode_update) {
                migr_parms_base.migr_as_kparms.migr_refcnt_mode =
                        migr_parms_new->migr_as_kparms.migr_refcnt_mode;
                update++;
        }


        
        /****
        if (mpu->xxx_update) {
                migr_parms_base.migr_system_kparms.xxx =
                        migr_parms_new->migr_system_kparms.xxx;
                        update++;
        }
        ****/

        if (update) {
                if (syssgi(SGI_NUMA_TUNE, MIGR_PARMS_SET, node, &migr_parms_base) < 0) {
                        perror("syssgi, MIGR_PARMS_SET");
                        exit(1);
                }
        }

        if (showconfig) {

                char* mode;

                printf("\nNuma Management Parameters for Node [%s]\n", node);
                switch (migr_parms_base.migr_as_kparms.migr_base_mode) {
                case MIGRATION_MODE_DIS:
                        mode = "DISABLED";
                        break;
                case MIGRATION_MODE_EN:
                        mode = "ENABLED";
                        break;
                case MIGRATION_MODE_ON:
                        mode = "NORMALLY-ON";
                        break;
                case MIGRATION_MODE_OFF:
                        mode = "NORMALLY-OFF";
                        break;
                default:
                        mode = "UNKNOWN";
                }

                printf("\tMigration: %s Threshold: %d%% MaxAbsThreshold: %d, MinDistance: %d, Vehicle: %s\n",
                       mode,
                       migr_parms_base.migr_as_kparms.migr_base_threshold,
                       migr_parms_base.migr_system_kparms.migr_threshold_reference,
                       migr_parms_base.migr_as_kparms.migr_min_distance,
                       migr_parms_base.migr_system_kparms.migr_vehicle?"TLBsync":"BTE");
                printf("\tFreezing: %s Threshold: %d%% Melting: %s Threshold: %d%%\n",
                       migr_parms_base.migr_as_kparms.migr_freeze_enabled?"ON":"OFF",
                       migr_parms_base.migr_as_kparms.migr_freeze_threshold,
                       migr_parms_base.migr_as_kparms.migr_melt_enabled?"ON":"OFF",
                       migr_parms_base.migr_as_kparms.migr_melt_threshold);
                printf("\tDampening: %s Factor: %d MemoryLowThreshold: %d%%\n",
                       migr_parms_base.migr_as_kparms.migr_dampening_enabled?"ON":"OFF",
                       migr_parms_base.migr_as_kparms.migr_dampening_factor,
                       migr_parms_base.migr_system_kparms.migr_memory_low_threshold);

                printf("\tUnpegging: %s, Threshold: %d, Interval: %d\n",
                       migr_parms_base.migr_system_kparms.migr_unpegging_control_enabled?"ON":"OFF",
                       migr_parms_base.migr_system_kparms.migr_unpegging_control_threshold,
                       migr_parms_base.migr_system_kparms.migr_unpegging_control_interval);

                switch (migr_parms_base.migr_as_kparms.migr_refcnt_mode) {
                case REFCNT_MODE_DIS:
                        mode = "DISABLED";
                        break;
                case REFCNT_MODE_EN:
                        mode = "ENABLED";
                        break;
                case REFCNT_MODE_ON:
                        mode = "NORMALLY-ON";
                        break;
                case REFCNT_MODE_OFF:
                        mode = "NORMALLY-OFF";
                        break;
                default:
                        mode = "UNKNOWN";
                }

                printf("\tRefcnt: %s UpdateAbsThreshold: %d\n",
                       mode,
                       migr_parms_base.migr_system_kparms.migr_effthreshold_refcnt_overflow);
                       
              
        }        
}


void
main(int argc, char** argv)
{
        char* node;
        int errflag = 0;
        int c;
        migr_parms_t migr_parms;
        int showconfig = 1;
        int allnodes = 0;
        migr_parms_update_t mpu = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

        /*
         * We need at least one argument, the nodepath
         */
        if (argc < 2) {
                usage(argv[0]);
                exit(1);
        }

        command = argv[0];

        while ((c = getopt(argc, argv, "havsm:t:u:p:w:b:z:i:f:r:")) != -1) {
                switch (c) {
                case 'h':
                        errflag++;
                        break;
                case 'a':
                        allnodes = 1;
                        break;
                case 'v':
                        showconfig = 1;
                        break;
                case 's':
                        showconfig = 0;
                        break;
                case 'm':
                {
                        if (strcmp(optarg, "foff") == 0) {
                                migr_parms.migr_as_kparms.migr_base_mode = MIGRATION_MODE_DIS;
                        } else if (strcmp(optarg, "fon") == 0) {
                                migr_parms.migr_as_kparms.migr_base_mode = MIGRATION_MODE_EN;
                        } else if (strcmp(optarg, "on") == 0) {
                                migr_parms.migr_as_kparms.migr_base_mode = MIGRATION_MODE_ON;
                        } else if (strcmp(optarg, "off") == 0) {
                                migr_parms.migr_as_kparms.migr_base_mode = MIGRATION_MODE_OFF;
                        } else {
                                errflag++;
                        }
                        mpu.migr_base_mode_update = 1;
                        break;
                }
                case 't':
                {
                        uint migr_threshold;
                        migr_threshold = atoi(optarg);
                        if (migr_threshold > 100) {
                                errflag++;
                        } else {
                                migr_parms.migr_as_kparms.migr_base_threshold = migr_threshold;
                        }
                        mpu.migr_base_threshold_update = 1;
                        break;
                }
                case 'z':
                {
                        uint migr_min_distance;
                        migr_min_distance = atoi(optarg);
                        if (migr_min_distance > 20) {
                                errflag++;
                        } else {
                                migr_parms.migr_as_kparms.migr_min_distance = migr_min_distance;
                        }
                        mpu.migr_min_distance_update = 1;
                        break;
                }
                
                case 'u':
                {
                        if (strcmp(optarg, "off") == 0) {
                                migr_parms.migr_system_kparms.migr_unpegging_control_enabled = 0;
                        } else if (strcmp(optarg, "on") == 0) {
                                migr_parms.migr_system_kparms.migr_unpegging_control_enabled = 1;
                        } else {
                                errflag++;
                        }
                        mpu.migr_unpegging_control_enabled_update = 1;
                        break;
                }
                case 'p':
                {
                        uint unpegger_threshold;
                        unpegger_threshold = atoi(optarg);
                        if (unpegger_threshold > 100) {
                                errflag++;
                        } else {
                                migr_parms.migr_system_kparms.migr_unpegging_control_threshold =
                                        unpegger_threshold;
                        }
                        mpu.migr_unpegging_control_threshold_update = 1;
                        break;
                }
                case 'w':
                {
                        uint unpegger_interval;
                        unpegger_interval = atoi(optarg);
                        migr_parms.migr_system_kparms.migr_unpegging_control_interval = unpegger_interval;
                        mpu.migr_unpegging_control_interval_update = 1;
                        break;
                }
#ifdef NONO                
                case 'c':
                {
                        if (strcmp(optarg, "i") == 0) {
                                migr_parms.migr_system_kparms.migr_auto_migr_mech = MIGR_MECH_IMMEDIATE;
                        } else if (strcmp(optarg, "q") == 0) {
                                migr_parms.migr_system_kparms.migr_auto_migr_mech = MIGR_MECH_ENQUEUE;
                        } else {
                                errflag++;
                        }
                        mpu.migr_auto_migr_mech_update = 1;
                        break;
                }
                case 'd':
                {
                        if (strcmp(optarg, "i") == 0) {
                                migr_parms.migr_system_kparms.migr_user_migr_mech = MIGR_MECH_IMMEDIATE;
                        } else if (strcmp(optarg, "q") == 0) {
                                migr_parms.migr_system_kparms.migr_user_migr_mech = MIGR_MECH_ENQUEUE;
                        } else {
                                errflag++;
                        }
                        mpu.migr_user_migr_mech_update = 1;
                        break;
                }
                case 'q':
                {
                        uint queue_control_interval;
                        queue_control_interval = atoi(optarg);
                        if (queue_control_interval > 255) {
                                errflag++;
                        } else {
                                migr_parms.migr_system_kparms.migr_queue_control_interval =
                                        queue_control_interval;
                        }
                        mpu.migr_queue_control_interval_update = 1;
                        break;
                }
                case 'r':
                {
                        uint migr_queue_capacity_trigger_threshold;
                        migr_queue_capacity_trigger_threshold = atoi(optarg);
                        if (migr_queue_capacity_trigger_threshold > 100) {
                                errflag++;
                        } else {
                                migr_parms.migr_system_kparms.migr_queue_capacity_trigger_threshold =
                                        migr_queue_capacity_trigger_threshold;
                        }
                        mpu.migr_queue_capacity_trigger_threshold_update = 1;
                        break;
                }
                case 'x':
                {
                        uint migr_queue_time_trigger_interval;
                        migr_queue_time_trigger_interval = atoi(optarg);
                        if (migr_queue_time_trigger_interval > 255) {
                                errflag++;
                        } else {
                                migr_parms.migr_system_kparms.migr_queue_time_trigger_interval =
                                        migr_queue_time_trigger_interval;
                        }
                        mpu.migr_queue_time_trigger_interval_update = 1;
                        break;
                }
#endif /*NONO*/               
		case 'b':
		{
                        if (strcmp(optarg, "off") == 0) {
				migr_parms.migr_system_kparms.migr_vehicle = 1;
                        } else if (strcmp(optarg, "on") == 0) {
                                migr_parms.migr_system_kparms.migr_vehicle = 0;
                        } else {
                                errflag++;
                        }
			if (!errflag){
				mpu.migr_vehicle_mode_update = 1;
			}
			
			break;
		}

                case 'i':
                {
                        if (strcmp(optarg, "off") == 0) {
                                migr_parms.migr_as_kparms.migr_dampening_enabled = 0;
                        } else if (strcmp(optarg, "on") == 0) {
                                migr_parms.migr_as_kparms.migr_dampening_enabled = 1;
                        } else {
                                errflag++;
                        }
                        mpu.migr_dampening_enabled_update = 1;
                        break;
                }
                
                case 'f':
                {
                        uint dampening_factor;
                        dampening_factor = atoi(optarg);
                        if ( dampening_factor > 100) {
                                errflag++;
                        } else {
                                migr_parms.migr_as_kparms.migr_dampening_factor =
                                        dampening_factor;
                        }
                        mpu.migr_dampening_factor_update = 1;
                        break;
                }

                case 'r':
                {
                        if (strcmp(optarg, "foff") == 0) {
                                migr_parms.migr_as_kparms.migr_refcnt_mode = REFCNT_MODE_DIS;
                        } else if (strcmp(optarg, "fon") == 0) {
                                migr_parms.migr_as_kparms.migr_refcnt_mode = REFCNT_MODE_EN;
                        } else if (strcmp(optarg, "on") == 0) {
                                migr_parms.migr_as_kparms.migr_refcnt_mode = REFCNT_MODE_ON;
                        } else if (strcmp(optarg, "off") == 0) {
                                migr_parms.migr_as_kparms.migr_refcnt_mode = REFCNT_MODE_OFF;
                        } else {
                                errflag++;
                        }
                        mpu.migr_refcnt_mode_update = 1;
                        break;
                }
                
                                
                case '?':
                        errflag++;
                }
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

                while (fgets(node_buffer, NODE_BUFFER_SIZE, ptr) != NULL) {
                        char* p = strchr(node_buffer, '\n');
                        if (p != NULL) {
                                *p = '\0';
                        }
                        migr_parms_update(node_buffer, &mpu, &migr_parms, showconfig);
                }

                if (pclose(ptr) < 0) {
                        perror("pclose");
                        exit(1);
                }
        } else {
                if (optind >= argc) {
                        usage(command);
                        exit(1);
                }
   
                for ( ; optind < argc; optind++) {
                        migr_parms_update(argv[optind], &mpu, &migr_parms, showconfig);
                }
        }
        
        exit(0);

}

        
                
