/*
 * IP6 Audio controls
 *
 * $Revision: 1.5 $
 *
 */

#define AUDIOC	('a' << 8)

#define AUDIOCGETINGAIN		(AUDIOC|1)	/* Get Input Gain */
#define AUDIOCSETINGAIN		(AUDIOC|2)	/* Set Input Gain (8 bits) */
#define AUDIOCGETOUTGAIN	(AUDIOC|3)	/* Get Output Gain (8 bits) */
#define AUDIOCSETOUTGAIN	(AUDIOC|4)	/* Set Output Gain (8 bits) */
#define AUDIOCSETRATE		(AUDIOC|5)	/* Sampling rate:
					   1=32khz, 2=16khz, 3=8khz, 0=0khz */
#define AUDIOCDURATION		(AUDIOC|6)	/* Duration in ticks (loop mode) */
