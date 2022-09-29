/*
**
**	Does all diagnostics test for slot board
*/

#include"sys/sbd.h"
#include "uif.h"
#include "saioctl.h"
#include "sys/IP20.h"


/*
 * HPC ID register
 */
#define HPC_0_ID_ADDR	0x1fb80000	/* HPC 0 exists (primary HPC) */
#define HPC_1_ID_ADDR	0x1fb00000	/* HPC 1 exists */
#define HPC_2_ID_ADDR	0x1f980000	/* HPC 2 exists */
#define HPC_3_ID_ADDR	0x1f900000	/* HPC 3 exists */

extern int badaddr();

slot_test(argc,argv)
int argc;
char *argv[];
{
	int error_count = 0;
	unsigned int i;
	int slot_no ;
	if ( argc < 2 ){
		printf("Usage:- %s <slot number>\n",argv[0]);
		printf("slot number : 0 - slot 0(Connector P6), 1 - slot 1(Connector P1), 2 - both\n");
		return(-1);
	}

	if(atoi(argv[1]) == 1 ) slot_no = 1 ;
	else if(atoi(argv[1]) == 2 ) slot_no = 2 ;
	else slot_no = 0;

	/* These tests will result in exception if slot boards are not physically present.
	** One way to check whether slot boards are present would be to probe for 
	** corresponding hpc 
	*/

	switch(slot_no) {

		case 0: if(!new_hpc_probe(2)) {
					msg_printf(VRB,"Warning: Slot board 0 (Connector P6) not present \n");
					return -1;
				}
				break ;
		case 1: if(!new_hpc_probe(3)) {
					msg_printf(VRB,"Warning: Slot board 1 (Connector P1) not present \n");
					return -1;
				}
				break ;
		case 2: if(!new_hpc_probe(2) || !new_hpc_probe(3)) {
					msg_printf(VRB,"Warning: Not Both Slot Boards Present \n");
					return -1;
				}
				break ;
	}


	msg_printf(VRB,"\n------SLOT BOARD DIAGNOSTICS TEST ------------\n");

/*
	** This was tried out to make the type = 0 (longburst device)
	** instead of usual type = 1 ( I/0 device)
	** Refer: page 21 of PIC specs

	Config_slot() ;  
*/

	error_count = 
	/*				gio_master_logic_test(argc,argv) + */ 
	   				gio_interrupt_test(argc,argv) + 
	  				pbus_logic_test(argc,argv) + 
	   				pbus_interrupt_test(argc,argv);  


	if(error_count)
		sum_error("******Slot board diagnostics******");
	else okydoky();
  
	return error_count;
}

/*
 * hpc_probe - return 1 if HPC number hpc exists, 0 otherwise
 */
new_hpc_probe(hpc)
int hpc ;
{
    int *testaddr;

    switch (hpc) {
    case 0: testaddr = (int *)PHYS_TO_K1(HPC_0_ID_ADDR); break;
/*
#ifdef LATER	* Read bus errors aren't always happening for hpc123 *
*/
    case 1: testaddr = (int *)PHYS_TO_K1(HPC_1_ID_ADDR); break;
    case 2: testaddr = (int *)PHYS_TO_K1(HPC_2_ID_ADDR); break;
    case 3: testaddr = (int *)PHYS_TO_K1(HPC_3_ID_ADDR); break;
/*
#endif * LATER *
*/
    default: return 0;
    }

    if (badaddr (testaddr, sizeof(int)))
		return 0;
    else
		return 1;
}

Config_slot()
{

#define	SLOT0_CONFIG_REGR	0x1fa20000	
#define	SLOT1_CONFIG_REGR	0x1fa20004	

	volatile unsigned int value ;

	/* make type = 0 , (type is bit 1) */
	value = *((volatile unsigned int *)(PHYS_TO_K1(SLOT0_CONFIG_REGR)));
	*(volatile unsigned int *)(PHYS_TO_K1(SLOT0_CONFIG_REGR)) = value & ~2 ; 	

	value = *((volatile unsigned int *)(PHYS_TO_K1(SLOT1_CONFIG_REGR))); 	
	*(volatile unsigned int *)(PHYS_TO_K1(SLOT1_CONFIG_REGR)) = value & ~2 ; 	
}
