
/* Power-Up, Shutdown and Bad Graphics tunes.
 * Conversion from aiff file to "tune" file by adpcm.
 */

#ident "$Revision: 1.5 $"

#if ENETBOOT || !defined(NOBOOTTUNE)
#ifndef	DEBUG
#if IP24
#include "IP24start.tune"
#include "IP24stop.tune"
#include "IP24badgfx.tune"
#else
#include "IP22start.tune"
#include "IP22stop.tune"
#include "IP22badgfx.tune"
#endif
#endif
#endif
