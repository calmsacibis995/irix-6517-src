/*
 *	cue.h
 *
 *	Description:
 *		Header file for cue.c
 *
 *	History:
 *      rogerc      11/06/90    Created
 */

void ff_arm( Widget w, CLIENTDATA *client, XmAnyCallbackStruct *call_data );
void ff_disarm( Widget w, CLIENTDATA *client, XmAnyCallbackStruct *call_data );
void ff( CLIENTDATA *client, XtIntervalId *id );
void rew( CLIENTDATA *client, XtIntervalId *id );
void rew_arm( Widget w, CLIENTDATA *client, XmAnyCallbackStruct *call_data );
void rew_disarm( Widget w, CLIENTDATA *client, XmAnyCallbackStruct *call_data );
