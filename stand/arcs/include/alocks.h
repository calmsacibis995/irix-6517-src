#ifndef __ALOCKS_H__
#define __ALOCKS_H__

/*
 * alocks.h - define and declare locks used by arcs routines
 *
 * $Revision: 1.3 $
 */

#ident	"arcs/include/alocks.h:  $Revision: 1.3 $"

#include <sys/types.h>
#include <sys/systm.h>

extern int arcs_ui_spl;
extern lock_t arcs_ui_lock;
extern int malloc_spl;
extern lock_t malloc_lock;


#define LOCK_ARCS_UI()							\
		if (arcs_ui_lock) {					\
			arcs_ui_spl = splockspl(arcs_ui_lock, splhi);	\
		}
#define UNLOCK_ARCS_UI()						\
		if (arcs_ui_lock) {					\
			spunlockspl(arcs_ui_lock, arcs_ui_spl);		\
		}

#define LOCK_MALLOC()							\
		if (malloc_lock) {					\
			malloc_spl= (lock_t) splockspl(malloc_lock,splhi);	\
		}
#define UNLOCK_MALLOC()							\
		if (malloc_lock) {					\
			spunlockspl(malloc_lock,malloc_spl);		\
		}

#endif /* __ALOCKS_H__ */
