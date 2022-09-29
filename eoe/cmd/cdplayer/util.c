/*
 *	util.c
 *
 *	Description:
 *		Utility functions for cdplayer application
 *
 *	History:
 *      rogerc      11/27/90    Created
 */

#include <X11/Intrinsic.h>
#include <Xm/ToggleBG.h>
#include "cdaudio.h"
#include "program.h"
#include "client.h"
#include "util.h"

/*
 *  void set_playmode_buttons( CLIENTDATA *clientp, int state )
 *
 *  Description:
 *		Set the play mode buttons to match state
 *
 *  Parameters:
 *      clientp		client data
 *      state		the play mode to match
 */

void set_playmode_buttons( CLIENTDATA *clientp, int state )
{
	switch (state) {
	default:
	case PLAYMODE_NORMAL:
		XmToggleButtonGadgetSetState( clientp->normalButton, TRUE, TRUE );
		break;

	case PLAYMODE_PROGRAM:
		XmToggleButtonGadgetSetState( clientp->programButton, TRUE, TRUE );
		break;

	case PLAYMODE_SHUFFLE:
		XmToggleButtonGadgetSetState( clientp->shuffleButton, TRUE, TRUE );
		break;
	}
}
