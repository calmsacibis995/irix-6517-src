/*
 *	cddata.h
 *
 *	Description:
 *		Header file for cddata.c
 *
 *	History:
 *      rogerc      11/28/90    Created
 */

void popup_data_dbox( Widget w, CLIENTDATA *clientp,
 XmAnyCallbackStruct *call_data);
Widget create_data_dbox( Widget parent, CLIENTDATA *clientp );
void data_nodisc( CLIENTDATA *clientp );
