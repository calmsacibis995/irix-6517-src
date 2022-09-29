/* utime.h - POSIX file time modification */

#ifndef	__INCutimeh
#define	__INCutimeh

#ifdef __cplusplus
extern "C" {
#endif

struct utimbuf
    {
    time_t	actime;		/* set the access time */
    time_t	modtime;	/* set the modification time */
    };

#if defined(__STDC__) || defined(__cplusplus)

extern int utime (char * file, struct utimbuf * newTimes);

#else 

extern int utime ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCdosFsLibh */
