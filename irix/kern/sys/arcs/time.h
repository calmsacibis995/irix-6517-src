/* ARCS time definitions
 */

#ifndef __ARCS_TIME_H__
#define __ARCS_TIME_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.9 $"

#ifndef	SECMIN
#define SECMIN	60
#define SECHOUR	(60 * SECMIN)
#define SECDAY	(24 * SECHOUR)
#define SECYR	(365 * SECDAY)
#endif	/* !SECMIN */

typedef struct timeinfo {
	USHORT Year;
	USHORT Month;
	USHORT Day;
	USHORT Hour;
	USHORT Minutes;
	USHORT Seconds;
	USHORT Milliseconds;
} TIMEINFO;

extern TIMEINFO *GetTime(void);
extern ULONG GetRelativeTime(void);

#ifdef __cplusplus
}
#endif

#endif /* _ARCS_TIME_H */
