#include <sys/SN/klconfig.h>
#include <sys/SN/gda.h>
#include <promgraph.h>
#include <kllibc.h>
#include <libsc.h>

#define MAX_BRDS_PER_MODULE		16

int num_modules ;

struct mod_tab module_table[MAX_MODULES] ;

void
init_module_table(void)
{
	int 		i, j, found ;
        cnodeid_t 	cnode ;
	nasid_t		nasid ;
        lboard_t 	*lbptr ;
    	gda_t *gdap;

    	gdap = (gda_t *)GDA_ADDR(get_nasid());
	if (gdap->g_magic != GDA_MAGIC) {
		printf("init_module_table: Invalid GDA MAGIC\n") ;
		return ;
	}

	bzero(&module_table[0], sizeof(module_table)) ;

	/* element 0 is not used and should be filled with 0 */

	module_table[0].module_id = 0 ; 
	module_table[0].module_num = 0 ;

        for (cnode = 0, i = 1; cnode < MAX_COMPACT_NODES; cnode ++) {
                nasid = gdap->g_nasidtable[cnode];
                if (nasid == INVALID_NASID)
                        continue;
                lbptr = find_lboard((lboard_t *)KL_CONFIG_INFO(nasid),
                                  KLTYPE_IP27);
                if (!lbptr) {
                    printf("init_module_table: Error - No IP27 on nasid %d!\n",
                                nasid);
                        continue;
                }

		found = 0 ;

		for(j=1;j<=i;j++)
			if (module_table[j].module_id == lbptr->brd_module) {
				found = 1 ;
				break ;
			}

		if (found) {
			found = 0 ;
			continue ;
		}

		module_table[i++].module_id = lbptr->brd_module ;
	}
	num_modules = i-1 ;

	module_table[1].module_num = MAX_BRDS_PER_MODULE ;

}



