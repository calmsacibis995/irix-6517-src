/***********************************************************************\
 *       File:           report.c                                       *
 *                                                                      *
 *       Contains code for outputting standard diagnostic messages      *
 *       during prom testing. This code outputs messages in a manner    *
 *	 conforming to the konatest irsaudit standard.      		*
 *                                                                      *
 \***********************************************************************/

#include <sys/types.h>
#include <sys/SN/agent.h>
#include <sys/SN/router.h>
#include <sys/SN/kldiag.h>
#include <libc.h>
#include <report.h>
#include <prom_msgs.h>

/*
 * result_pass
 *
 *  Used by passing prom diags to report status.If diag_mode is
 *  set to DIAG_MODE_MFG the pass message is printed per the irsaudit
 *  standard. Otherwise, a simpler message is displayed.
 *
 */

void result_pass(char *test_name, int diag_mode)
{
    char  	trunc_test_name[TESTNAME_WIDTH + 1];


    if (GET_DIAG_MODE(diag_mode) == DIAG_MODE_MFG) {
        strncpy(trunc_test_name, test_name, TESTNAME_WIDTH);
	trunc_test_name[TESTNAME_WIDTH] = '\0';
	printf("RSLT %-14s PASS\n", trunc_test_name);
    }
    else 
        DB_PRINTF(("%s: passed.\n", test_name));
}

/*
 * result_fail
 *
 *  Used by failing prom diags to report error status in a way acceptable
 *  to manufacturing test automation tools: 
 *  RSLT abbr_testname  FAIL                diag_rc = xxx <optional short msg>  
 *  1234 <col 6 - 19>   <21-24>             <col 41-53>   <columns 55 to 80> 
 */

void result_fail(char *test_name, int diag_rc, char *comment)
{
    char 	trunc_test_name[TESTNAME_WIDTH + 1], 
		trunc_comment[COMMENT_WIDTH + 1];

    strncpy(trunc_test_name, test_name, TESTNAME_WIDTH);
    trunc_test_name[TESTNAME_WIDTH] = '\0';

    strncpy(trunc_comment, comment, COMMENT_WIDTH);
    trunc_comment[COMMENT_WIDTH] = '\0';
    printf("%s failed: %s\n", test_name, comment);
    printf("RSLT %-14s FAIL                diag_rc = %-3d %-26s\n", 
		    trunc_test_name, diag_rc, trunc_comment); 

} /* result_fail */

/*
 * result_diagnosis
 *
 *  Used by failing prom diags to report additonal isolation information. 
 *  
 *  For router tests, the following format is used: 
 * 
 *  DIAG abbr_testname  LINK OUT_PORT <x> on SLOT <s> LINK IN_PORT <x> on SLOT <s> 
 *
 *  1234 <col 6 - 19>   <21-80>
 *
 *  For hub tests, the following format is used:
 * 
 *  DIAG abbr_testname  HUB on Nodeboard in SLOT n<#> failed test run by CPU <#>
 * 
 *  1234 <col 6 - 19>   <21-80>	
 */

void result_diagnosis(char *test_name, ...)
{
    char  	trunc_test_name[TESTNAME_WIDTH + 1];
    int		from_port,
		from_slot,
		in_port,
		in_slot,
		slot_id,
		cpu_num;
    nic_t	from_nic,
		in_nic; 
    uchar_t	in_module_id,
		from_module_id;
    va_list     argptr;

    va_start(argptr, test_name);

    strncpy(trunc_test_name, test_name, TESTNAME_WIDTH);
    trunc_test_name[TESTNAME_WIDTH] = '\0';

    /* 
     * If a rtr test has failed this message will print out
     * the port and path information to help isolate the link.
     * It should be called like this for rtr diags:
     * 
     * result_diagnosis(char *test_name, int rtr, int from_port, 
     * 			int from_slot, nic_t from_nic, uchar_t from_module_id, 
     * 			int in_port, int in_slot, nic_t in_nic, uchar_t in_module_id)
     * or
     * result_diagnosis(char *test_name, int rtr, int from_slot, nic_t from_nic, 
     *			uchar_t from_module_id, int in_port, int in_slot, nic_t in_nic,
     * 			uchar_t in_module_id)
     */

    if (strncmp(trunc_test_name, "rtr", 3) == 0) {

	if (va_arg(argptr, int) == 1) { /*rtr to rtr test */

	    from_port = va_arg(argptr, int);
	    from_slot = va_arg(argptr, int);
	    from_nic = va_arg(argptr, nic_t);
	    from_module_id = va_arg(argptr, uchar_t);
	    in_port = va_arg(argptr, int);
	    in_slot = va_arg(argptr, int);
	    in_nic = va_arg(argptr, nic_t);
	    in_module_id = va_arg(argptr, uchar_t);
	    printf("DIAG %-14s OUT_BOUND RTR: LINK %d SLOT %d NIC %lx MODULE %lx\n", 
		trunc_test_name, from_port, from_slot, from_nic, from_module_id);
	    printf("DIAG %-14s IN_BOUND RTR: LINK %d SLOT %d NIC %lx MODULE %lx\n", 
		trunc_test_name, in_port, in_slot, in_nic, in_module_id);

	} else { /* hub to rtr test */

	    from_slot = va_arg(argptr, int); 
            from_nic = va_arg(argptr, nic_t);
            from_module_id = va_arg(argptr, uchar_t);
            in_port = va_arg(argptr, int);
            in_slot = va_arg(argptr, int);
            in_nic = va_arg(argptr, nic_t);
            in_module_id = va_arg(argptr, uchar_t);
            printf("DIAG %-14s OUT_BOUND NODE: SLOT %d NIC %lx MODULE %lx\n",
                trunc_test_name, from_slot, from_nic, from_module_id);
            printf("DIAG %-14s IN_BOUND RTR: LINK %d SLOT %d NIC %lx MODULE %lx\n", 
                trunc_test_name, in_port, in_slot, in_nic, in_module_id);
	}
    }

    /*
     * If a hub test has failed this message will print out the slot number
     * and the number of the CPU which was running the test in order to
     * isolate the failing node board.
     *	  
     * This function should be called as follows:
     * 
     * result_diagnosis(char *test_name, int slot_id, int cpu_num) 
     */

    if (strncmp(trunc_test_name, "hub", 3) == 0) {
    	slot_id = va_arg(argptr, int);
        cpu_num = va_arg(argptr, int);
        printf("DIAG %-14s HUB on Nodeboard in SLOT n%d failed test run by CPU %d.\n", 
                trunc_test_name, slot_id, cpu_num);
    }

    va_end(argptr);
}

/*
 * result_code
 *
 *  Used by failing prom diags to report additional failure information 
 *  to manufacturing test automation tools using the irsaudit standard
 *  CODE message.
 *
 */

void result_code(char *test_name, char *code_type, ...)
{
    char        trunc_test_name[TESTNAME_WIDTH + 1],
		trunc_code_type[CODE_TYPE_WIDTH + 1];
    char   	board_status[STATUS_WIDTH + 1];
    char   	part_num[PART_NUM_WIDTH + 1];
    char   	serial_num[SNUM_WIDTH + 1];
    char   	nic_num[NIC_NUM_WIDTH + 1];
    char 	error_status[ERRSTAT_WIDTH + 1];
    char 	chip[CHIP_WIDTH + 1];
    int		r,y,g;
		
    va_list     argptr;

    va_start(argptr, code_type);

    strncpy(trunc_test_name, test_name, TESTNAME_WIDTH);
    trunc_test_name[TESTNAME_WIDTH] = '\0';
    strncpy(trunc_code_type, code_type, CODE_TYPE_WIDTH);
    trunc_code_type[CODE_TYPE_WIDTH] = '\0';

    if (strcmp(trunc_code_type, "BF") == 0) {
	
    /*
     *  To call result_code for "BF"-type messages:
     *      result_code(char *test_name, char *code_type, char *board_status,
     *                      char * part_num, char *serial_num, char *nic_num)
     */
        strcpy(board_status, va_arg(argptr, char *));
        strcpy(part_num, va_arg(argptr, char *));
        strcpy(serial_num, va_arg(argptr, char *));
        strcpy(nic_num, va_arg(argptr, char *));

	/* Columns are as follows:  			*/
	/*      1    6     21  25   27			*/
	printf("CODE %-14s BF  %-1s %s:%s(%s)\n",
                trunc_test_name, board_status, part_num, 
					serial_num, nic_num);

    } else if (strcmp(trunc_code_type, "SUM") == 0) {

    /*
     *  To call result_code for "SUM"-type messages:
     *      result_code(char *test_name, char *code_type, char *error_status)
     */
  
	strcpy(error_status, va_arg(argptr, char *));

        /* Columns are as follows:                    */
        /*      1    6     21  25                     */
        printf("CODE %-14s SUM %-15s\n", trunc_test_name, error_status);

    } else if (strcmp(trunc_code_type, "BSUM") == 0) {

    /*
     *  To call result_code for "BSUM"-type messages:
     *      result_code(char *test_name, char *code_type, int r_num,
     *                      int y_num, int g_num, char *part_num,
     *                      char *serial_num, char *nic_num)
     */
  
	r = va_arg(argptr, int);
	y = va_arg(argptr, int);
	g = va_arg(argptr, int);
        strcpy(part_num, va_arg(argptr, char *));
        strcpy(serial_num, va_arg(argptr, char *));
        strcpy(nic_num, va_arg(argptr, char *));

        /* Columns are as follows:                     	        */
        /*      1    6     21   26  30   35    41               */
        printf("CODE %-14s BSUM %3dR%3dY %3dG  %s:%s(%s)\n",
                trunc_test_name, r, y, g, part_num, serial_num, nic_num); 

    } else if (strcmp(trunc_code_type, "CSUM") == 0) {

    /*
     *  To call result_code for "CSUM"-type messages:
     *      result_code(char *test_name, char *code_type, int r_num,
     *                      int y_num, int g_num, char *chip)
     */

        r = va_arg(argptr, int);
        y = va_arg(argptr, int);
        g = va_arg(argptr, int);
        strcpy(chip, va_arg(argptr, char *));

        /* Columns are as follows:                      	*/
        /*      1    6     21   26  30   35    41               */
        printf("CODE %-14s CSUM %3dR%3dY %3dG  %s\n",
                trunc_test_name, r, y, g, chip);
    } else {

	printf("CODE %-14s Illegal or unsupported CODE message\n", 
					trunc_test_name);
    }

    va_end(argptr);

} /* result_code */

int make_diag_rc(short diag_parm, short diag_val)
{
    int diag_return_code;

    diag_return_code = ((diag_parm << 16) | diag_val);
    return diag_return_code;
}


