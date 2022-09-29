/*
 *	seldbox.h
 *
 *	Description:
 *		Header file for seldbox.c
 *
 *	History:
 *      rogerc      11/26/90    Created
 */

#define select_playwarn select_nodisc

int create_program_select_dialog( Widget parent, CLIENTDATA *clientp );
void select_program( CLIENTDATA *clientp );
void select_nodisc( CLIENTDATA *clientp );
