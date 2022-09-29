/*  Standalone hardware configuration.
 */

#ident "$Revision: 1.7 $"

struct component;
struct htp_fncs;

#if IP30
#define SLOTCONFIG 1
#include <sys/slot_drvr.h>
#else
struct slotinfo_s;
#endif

struct standcfg {
	/* driver install routine -- slot is optional */
	int (*install)(struct component *, struct slotinfo_s *);
	struct standcfg *parent;		/* parent of this node */
	void *(*gfxprobe)(struct htp_fncs **);	/* graphics probe */
#ifdef SLOTCONFIG
	struct drvrinfo_s *drvrinfo;		/* driver info */
#endif
};

#define	SCFG_NOPARENT	0			/* driver only here for link */
#define SCFG_ROOTPARENT	1			/* parent is "root" */
