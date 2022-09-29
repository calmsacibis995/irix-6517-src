#ident  "$Header: /proj/irix6.5.7m/isms/eoe/cmd/sss/configmon/RCS/configmon_event.h,v 1.3 1999/06/16 17:56:04 tjm Exp $"

/* ConfigMon Types
 */
#define CME_CONFIGMON_INIT      0x00200100
#define CME_SYSINFO_CHANGE      0x00200101
#define CME_HW_INSTALLED        0x00200102
#define CME_HW_DEINSTALLED      0x00200103
#define CME_SW_INSTALLED        0x00200104
#define CME_SW_DEINSTALLED      0x00200105
#define CME_SYSTEM_CHANGE   	0x00200106
#define CME_UPDATE_FAILED   	0x00200107

/* Function prototypes
 */
int send_cm_event(
	time_t 		/* time of event(s) */,
	int 		/* event type (see configmon_api.h) */);
