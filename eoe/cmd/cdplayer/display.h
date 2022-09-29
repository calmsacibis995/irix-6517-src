/*
 *	display.h
 *
 *	Description:
 *		Header file for display.c
 *
 *	History:
 *      rogerc      11/06/90    Created
 */

void draw_display( CLIENTDATA *clientp );
Boolean set_playmode_text( int playmode );
void set_repeat( Boolean repeat );
void set_status_bitmap( int state );
void create_display( Widget parent, CLIENTDATA *clientp );
