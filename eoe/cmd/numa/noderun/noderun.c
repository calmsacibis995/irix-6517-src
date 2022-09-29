/*
 *
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
 */

#include <sys/types.h>
#include <sys/pmo.h>
#include <sys/prctl.h>
#include <sys/syssgi.h>
#include <sys/wait.h>
#include <string.h>
#include <malloc.h>
#include <fmtmsg.h>
#include <unistd.h>


/*
 * noderunon - run a process on a specific node
 */

pmo_handle_t
mld_create_and_place(char* source_node)
{
        pmo_handle_t mld;
        pmo_handle_t mldset;
        raff_info_t rafflist;

        /*
         * The mld, radius = 0 (from one node only),
         *          size = 0 (for process placement only)
         */
        
        if ((mld = mld_create(0, 0)) < 0) {
                perror("mld_create");
                return (-1);
        }

        /*
         * The mldset
         */

        if ((mldset = mldset_create(&mld, 1)) < 0) {
                perror("mldset_create");
                return (-1);
        }

        /*
         * Placing the mldset with the one mld
         */

        rafflist.resource = source_node;
        rafflist.restype = RAFFIDT_NAME;
        rafflist.reslen = strlen(source_node);
        rafflist.radius = 0;
        rafflist.attr = RAFFATTR_ATTRACTION;

        if (mldset_place(mldset,
                         TOPOLOGY_PHYSNODES,
                         &rafflist, 1,
                         RQMODE_ADVISORY) < 0) {
                perror("mldset_placeee");
                return (-1);
        }

        return (mld);
}



main(int argc, char** argv)
{
        pmo_handle_t mld;
        int waitstate;
        pid_t pid;
        
        if (argc < 3) {
                fprintf(stderr, "Usage: %s <hwgraph device path> program args\n",
                        argv[0]);
                exit(1);
        }

        mld = mld_create_and_place(argv[1]);

        /*
         * Get rid of program name and device name
         */
        argv += 2;

        if (mld < 0) {
                exit(1);
        }

        switch (pid = fork()) {
        case 0:
                /*
                 * This is the child 
                 */
                if (process_mldlink(0, mld, RQMODE_MANDATORY) < 0) {
                        perror("process_mldlink");
                        exit(1);
                }
                
                if (execvp(argv[0], argv) < 0) {
                        perror("exec");
                        exit(1);
                }
                exit(0);
        case -1:
                perror("fork");
                exit(1);
        default:
                wait(&waitstate);
        }

        exit(0);
}


        
