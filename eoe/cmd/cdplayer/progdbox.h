/*
 *	progdbox.h
 *
 *	Description:
 *		Header file for progdbox.c
 *
 *	History:
 *      rogerc      11/26/90    Created
 */

#define program_playwarn program_nodisc

Widget create_program_dialog( Widget parent, CLIENTDATA *clientp );
void modify_program( CLIENTDATA *clientp, int prog_num );
void create_program( CLIENTDATA *clientp );
void program_nodisc( CLIENTDATA *clientp );
