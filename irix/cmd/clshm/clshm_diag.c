/* clshm_diag:
 * -----------
 * Make sure that all partitions are up and have their daemons up as well
 * for clshm driver.  
 */

#include <stdio.h>
#include <sys/types.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/syssgi.h>
#include <sys/partition.h>
#include <sys/errno.h>
#include <stdlib.h>

#ifdef CLSHM_USR_LIB_LINKED
#include <sys/SN/clshm.h>
#include "../../lib/libclshm/src/xp_shm.h"

extern int errno;

#define HOSTNAME_LENGTH 256
#endif /* CLSHM_USR_LIB_LINKED */

main()
{
#ifdef CLSHM_USR_LIB_LINKED
    partid_t	own_part;
    int		num_parts, num_bad_parts, i;
    partid_t 	part_list[MAX_PARTITIONS];
    char	hostname[HOSTNAME_LENGTH];

    num_bad_parts = 0;
    own_part = part_getid();
    num_parts = part_getcount();
    printf("%d partitions were discovered.\n", num_parts);
    printf("\tLocal partition number is 0x%02x.\n", own_part);


    /* get the list of all partitions known */
    if((num_parts = part_getlist(part_list, MAX_PARTITIONS)) < 1) {
	printf("\tCan't even access local partition information!\n");
	exit(1);
    } 

    /* get our own hostname to make sure at least the local daemon is up 
     * before going on */
    if(part_gethostname(own_part, hostname, HOSTNAME_LENGTH) < 0) {
	printf("\tCan't communicated with local partition's clshm "
	       "daemon\n");
	exit(1);
    } 
    printf("\tLocal partition name is %s\n", hostname);

    /* get partition names for all partitions */
    for(i = 0; i < num_parts; i++)  {
	/* don't check if the list gave us a bad partition number */
	if(part_list[i] < 0) {
	    printf("\tOne partition number unavailable at this time\n");
	    num_bad_parts++;
	} else if(part_gethostname(part_list[i], hostname, 
				   HOSTNAME_LENGTH) < 0) {
	    printf("\tCan't communicate with daemon on Partition "
		   "0x%02x\n", part_list[i]);
	    num_bad_parts++;
	} else {
	    printf("\tPartition 0x%02x has hostname %s and is ok.\n", 
		   part_list[i], hostname);
	}
    }

    if(num_bad_parts > 0)  {
    	printf("clshm_diag found problems with clshm on %d partition(s).\n",
	       num_bad_parts);
    } else {
    	printf("All %d available partitions are set up with craylink "
	       "shared memory\n", num_parts);
    }
    return(0);
#else /* !CLSHM_USR_LIB_LINKED */
    printf("clshm (the craylink shared memory driver)\n");
    printf("\tis not supported at this time!!!!!\n");
    return(0);
#endif /* CLSHM_USR_LIB_LINKED */
}
