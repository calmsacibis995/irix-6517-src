/*
 * Copyright 1991, Silicon Graphics, Inc. 
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or 
 * duplicated in any form, in whole or in part, without the prior written 
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions 
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or 
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished - 
 * rights reserved under the Copyright Laws of the United States.
 */



#ifdef BOGUS
#include "irixbtest.h"
#include "queuetests.h"
#include "menus.h"
#include <stdlib.h>          /* For system(). */

extern void queue_multi(short index);

/*
 * displaymenu() is used to display all the menus.  It
 * clears the screen and prints several lines of text.
 * The argument "menu_num" determines which menu is displayed.
 */
void
displaymenu(short menu_num)
{
    /*
     * Clear the screen.
     */
    system("tput clear");

    /*
     * Print the strings common to all the menus.
     */
    printf("\n\n\nIndicate your choice by typing the corresponding number,\n");
    printf("followed by a carriage return.\n\n\n\n\n\n\n");
    

    /*
     * Print the strings specific to this menu.
     */
    switch (menu_num) {
    case (0):          /* The main menu. */
	printf("     Queue every test in the suite for execution.   1\n\n\n");
	printf("     Select specific tests by Security Policy.      2\n\n\n");
	printf("     Select specific tests by System Call.          3\n\n\n");
	printf("     Ready to execute queued tests now.             4\n\n\n");
	printf("     Terminate this program without \n");
	printf("     executing any tests.                           5\n\n\n");
	break;
	
    case (1):          /* Security policy menu. */
	printf("     Select a Mandatory Access Control (MAC) Policy       1\n\n\n");
	printf("     Select a Discretionary Access Control (DAC) Policy   2\n\n\n");
	printf("     Exit this menu                                       3\n\n\n");
	break;
	
    case (2):          /* System call menu menu. */
	printf("     open               1\n\n\n");
	printf("     stat               2\n\n\n");
	printf("     Exit this menu     3\n\n\n");
	break;
	
    case (3):          /* MAC policy menu menu. */
	printf("     Pathname Data Read MAC Policy        1\n\n\n");
	printf("     Pathname Data Write MAC Policy       2\n\n\n");
	printf("     Pathname Attribute Read MAC Policy   3\n\n\n");
	printf("     Exit this menu                       4\n\n\n");
	break;
	
    case (4):          /* DAC policy menu menu. */
	printf("     Pathname Data Read DAC Policy        1\n\n\n");
	printf("     Pathname Data Write DAC Policy       2\n\n\n");
	printf("     Pathname Attribute Read DAC Policy   3\n\n\n");
	printf("     Exit this menu                       4\n\n\n");
	break;
	
    default:
	break;
    }
    return;
}

/* 
 * The following functions are all similar. The function's
 * action is determined by the input value.  One or more
 * of the input values cause a function to be called.
 * Another input value causes the function to return.
 * Any other value is a user error.
 */

void
domenu_policy(void)
{
    register short i = 0;
    register int c;
    short inpt;
    char str[5];
    
    for ( ; ; ) {
	c = '\0';
	i = 0;
	inpt = 0;
	displaymenu(1);
	
	while ( (c = getchar()) != '\n') {
	    str[i++] = c;
	}
	str[i] = '\0';
	inpt = atoi(str);
	switch (inpt) {
	case (1):
	    domenu_mac();
	    break;
	case (2):
	    domenu_dac();
	    break;
	case (3):   
	    return;
	default:      /* User error. */
	    w_error(GENERAL, err[MENUCHOICE], str[i], (char *)0, 0);
	    break;
	}
    }
}

void
domenu_syscall(void)
{
    register short i = 0;
    register int c;
    short inpt;
    char str[5];
    
    for ( ; ; ) {
	c = '\0';
	i = 0;
	inpt = 0;
	displaymenu(2);
	
	while ( (c = getchar()) != '\n') {
	    str[i++] = c;
	}
	str[i] = '\0';
	inpt = atoi(str);
	switch (inpt) {
	case (1):
	    queue_multi(OPEN);
	    break;
	case (2):
	    queue_multi(STAT);
	    break;
	case (3):     /* Return to calling function. */
	    return;
	default:      /* User error. */
	    w_error(GENERAL, err[MENUCHOICE], str[i], (char *)0, 0);
	    break;
	}
    }
}

void
domenu_mac(void)
{
    register short i = 0;
    register int c;
    short inpt;
    char str[5];
    
    for ( ; ; ) {
	c = '\0';
	i = 0;
	inpt = 0;
	displaymenu(3);
	
	while ( (c = getchar()) != '\n') {
	    str[i++] = c;
	}
	str[i] = '\0';
	inpt = atoi(str);
	switch (inpt) {
	case (1):
	    queue_multi(PNDR_MAC);
	    break;
	case (2):
	    queue_multi(PNDW_MAC);
	    break;
	case (3):
	    queue_multi(PNAR_MAC);
	    break;
	case (4):     /* Return to calling function. */
	    return;
	default:      /* User error. */
	    w_error(GENERAL, err[MENUCHOICE], str[i], (char *)0, 0);
	    break;
	}
    }
}

void
domenu_dac(void)
{
    register short i = 0;
    register int c;
    short inpt;
    char str[5];
    
    for ( ; ; ) {
	c = '\0';
	i = 0;
	inpt = 0;
	displaymenu(4);
	
	while ( (c = getchar()) != '\n') {
	    str[i++] = c;
	}
	str[i] = '\0';
	inpt = atoi(str);
	switch (inpt) {
	case (1):
	    queue_multi(PNDR_DAC);
	    break;
	case (2):
	    queue_multi(PNDW_DAC);
	    break;
	case (3):
	    queue_multi(PNAR_DAC);
	    break;
	case (4):     /* Return to calling function. */
	    return;
	default:      /* User error. */
	    w_error(GENERAL, err[MENUCHOICE], str[i], (char *)0, 0);
	    break;
	}
    }
}

#endif BOGUS
