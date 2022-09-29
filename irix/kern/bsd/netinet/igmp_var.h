/*
 * Internet Group Management Protocol (IGMP),
 * implementation-specific definitions.
 *
 * Written by Steve Deering, Stanford, May 1988.
 *
 * MULTICAST 1.2
 */
#ifdef __cplusplus
extern "C" {
#endif

/*
 * Statistics are now in sys/tcpipstats.h
 */
#ifdef _KERNEL
#define IGMP_RANDOM_DELAY(X)	((int) random() % (X) + 1 )

struct in_multi;
extern void igmp_joingroup(struct in_multi *);
extern void igmp_leavegroup(struct in_multi *);
#endif
#ifdef __cplusplus
}
#endif
