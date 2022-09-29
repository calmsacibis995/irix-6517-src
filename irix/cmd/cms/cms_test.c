#include "cms_base.h"
#include "cms_kernel.h"
#include "cms_cell_status.h"
#include "sys/mman.h"


extern int	cms_num_cells; 

#define	POLL_TIME	2

void	test_case1(void);
void	test_case2(void);
void	test_case3(void);
void	test_case4(void);
void	test_case5(void);
void	cms_monitor(int);
void 	wait_for_stable_membership(void);


extern cms_cell_status_t	*cms_cell_status;

typedef	 void (*func_ptr_t)();

func_ptr_t test_funcs[] = {
	test_case1,
	test_case2,
	test_case3,
	test_case4,
	test_case5
};
	
int	num_test_funcs = (sizeof(test_funcs)/sizeof(func_ptr_t));


void
wait_for_stable_membership(void)
{
	boolean_t stable;
	int	i;

	do {
		stable = B_TRUE;
		sleep(POLL_TIME);
		for (i = 0; i < cms_num_cells; i++) {
			if (cms_get_cell_status(i) && 
				(cms_cell_status->cms_info[i].cms_state 
							!= CMS_STABLE)) {
				stable = B_FALSE;
				break;
			}
		}
	} while (!stable);
}

void
cms_monitor(int test_num)
{
	func_ptr_t func;
	int	i;

	if (test_num < 0) {
		for (i = 0; i <num_test_funcs; i++) {
			func = test_funcs[i]; 
			func();
		}
	} else {
		func = test_funcs[test_num];
		func();
	}
}

/*
 * Set a node dead.
 */
void
test_case1(void)
{
	wait_for_stable_membership();
	printf("Test case 1: Setting node %d to dead\n", cms_num_cells - 1);
	cms_set_cell_status( cms_num_cells - 1, B_FALSE);
	wait_for_stable_membership();
	printf("Test case 1: Bringing up node %d \n", cms_num_cells - 1);
	cms_set_cell_status(cms_num_cells - 1, B_TRUE);
}

/*
 * Assumes that node 0 is the leader.
 * Brings down all but one cell and brings them up again.
 */
void
test_case2(void)
{
	int	i;

	wait_for_stable_membership();
	for (i = 1; i < cms_num_cells; i++) {
		printf("Test case 2: Setting node %d to dead\n", i);
		cms_set_cell_status(i, B_FALSE);
		wait_for_stable_membership();
	}

	/*
	 * Bringing up the dead cells. This includes 0
	 */
	cms_set_cell_status(0, B_TRUE);
	for (i = 1; i < cms_num_cells; i++) {
		printf("Test case 2: Setting node %d to alive\n", i);
		cms_set_cell_status(i, B_TRUE);
		wait_for_stable_membership();
	}
}

void
test_case3(void)
{
	/*
	 * Bring down the leader.
 	 */

	cell_t	leader, cell;
	int	leader_age;

	leader = 0;
	leader_age = cms_cell_status->cms_info[0].cms_age[0];

	for (cell = 1; cell < cms_num_cells; cell++) {
		if (cms_cell_status->cms_info[cell].cms_age[cell] 
								> leader_age) {
			leader = cell;
			leader_age = cms_cell_status->cms_info[cell].
							cms_age[cell];
		}
	}
	printf("Test case 3: Kill the leader %d\n", leader);
	cms_set_cell_status(leader, B_FALSE);
	wait_for_stable_membership();
	printf("Test case 3: Bring up the leader %d\n", leader);
	cms_set_cell_status(leader, B_TRUE);
}

/*
 * Test link failure.
 */
void
test_case4(void)
{
	wait_for_stable_membership();
	printf("Breaking link between 1 and 2\n");
	cms_set_link_status(1,2, B_FALSE);
	wait_for_stable_membership();
}

/*
 * Cause an indirect link. This should cause forwarding.
 */
void
test_case5(void)
{
	printf("Breaking link between 3 and 2\n");
	wait_for_stable_membership();
	printf("setting link to false\n");
	cms_set_link_status(3,2, B_FALSE);
	wait_for_stable_membership();
	cms_set_cell_status(3, B_TRUE);
	cms_set_link_status(3,2, B_TRUE);
	printf("Setting 3 to be alive. "
		"Also reconnecting link between 3 and 2\n");
	wait_for_stable_membership();
}
