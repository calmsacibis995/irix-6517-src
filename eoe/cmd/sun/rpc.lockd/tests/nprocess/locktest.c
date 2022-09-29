#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>
extern char *sys_errlist[];

void modsw(double *, long *, double *, long *);
void final(double *, long *, double *, long *);

main()
{
   int i;
   double avg_lock_time;
   long lock_requests;
   double avg_openclose_time;
   long openclose_requests;

/* This program tries to reproduce a lockd problem found in PDMS */

   while ( 1 ) {
       printf ("-----------------------------------------\n");
       printf ("Fake 'PDMS', testing 'fcntl'\n");

/* Do some locking */
	   avg_lock_time = (double)0.0;
	   lock_requests = 0;
       for (i=0; i<6; i++) {
          modsw (&avg_lock_time, &lock_requests, &avg_openclose_time,
             &openclose_requests);
          printf ("Done loop %d\n", i);
          fflush (stdout);
       }
       final (&avg_lock_time, &lock_requests, &avg_openclose_time,
          &openclose_requests);
       printf ("All done.\nAverage lock time %lf.  Total requests %d\n",
          avg_lock_time, lock_requests);
       printf ("Average open/close time %lf.  Total requests %d\n",
          avg_openclose_time, openclose_requests);
   }
   exit (0);
}
void modsw (
   double *avg_lock_time,
   long *lock_requests,
   double *avg_openclose_time,
   long *openclose_requests )
{
   int fdS, fd9, i;
   struct flock flk;
   time_t start, end, open_start, open_end;
   time_t locksum = 0;

   open_start = time(NULL);
   fdS = open ("SYSDB", O_CREAT + O_RDWR, 0666); if (fdS == -1) goto ERR;

   fd9 = open ("v99", O_CREAT + O_RDWR, 0666); if (fd9 == -1) goto ERR;

   start = time(NULL);
/* Lock byte 0 to Read on 1st fd */
   flk.l_whence = 0; flk.l_start = 0; flk.l_len = 1; flk.l_type = F_RDLCK;
   i = fcntl (fdS, F_SETLKW, &flk); if (i == -1) goto ERR;

/* Lock byte 2 to Read on 1st fd */
   flk.l_whence = 0; flk.l_start = 2; flk.l_len = 1; flk.l_type = F_RDLCK;
   i = fcntl (fdS, F_SETLKW, &flk); if (i == -1) goto ERR;

/* Unlock byte 0 on 1st fd */
   flk.l_whence = 0; flk.l_start = 0; flk.l_len = 1; flk.l_type = F_UNLCK;
   i = fcntl (fdS, F_SETLK, &flk); if (i == -1) goto ERR;

/* -------------------------------*/

/* Lock byte 0 to Write on 2nd fd */
   flk.l_whence = 0; flk.l_start = 0; flk.l_len = 1; flk.l_type = F_WRLCK;
   i = fcntl (fd9, F_SETLKW, &flk); if (i == -1) goto ERR;

/* Lock byte 2 to Write on 2nd fd */
   flk.l_whence = 0; flk.l_start = 2; flk.l_len = 1; flk.l_type = F_WRLCK;
   i = fcntl (fd9, F_SETLKW, &flk); if (i == -1) goto ERR;

/* Unlock byte 0 on 2nd fd */
   flk.l_whence = 0; flk.l_start = 0; flk.l_len = 1; flk.l_type = F_UNLCK;
   i = fcntl (fd9, F_SETLK, &flk); if (i == -1) goto ERR;
   end = time(NULL);
   *lock_requests = *lock_requests + 6;
   *avg_lock_time = *avg_lock_time +
      ((double)(end - start) - *avg_lock_time) / *lock_requests;
   locksum += end - start;

/* Close 2nd fd */
   close (fd9);

/* Close 1st fd */
   close (fdS);

/* -------------------------------*/

   fdS = open ("SYSDB", O_CREAT + O_RDWR); if (fdS == -1) goto ERR;

   start = time(NULL);
/* Lock byte 0 to Read on 1st fd */
   flk.l_whence = 0; flk.l_start = 0; flk.l_len = 1; flk.l_type = F_RDLCK;
   i = fcntl (fdS, F_SETLKW, &flk); if (i == -1) goto ERR;

/* Lock byte 2 to Read on 1st fd */
   flk.l_whence = 0; flk.l_start = 2; flk.l_len = 1; flk.l_type = F_RDLCK;
   i = fcntl (fdS, F_SETLKW, &flk); if (i == -1) goto ERR;

/* Unlock byte 0 on 1st fd */
   flk.l_whence = 0; flk.l_start = 0; flk.l_len = 1; flk.l_type = F_UNLCK;
   i = fcntl (fdS, F_SETLK, &flk); if (i == -1) goto ERR;
   end = time(NULL);
   *lock_requests = *lock_requests + 3;
   *avg_lock_time = *avg_lock_time +
      ((double)(end - start) - *avg_lock_time) / *lock_requests;
   locksum += end - start;

/* Close 1st fd */
   close (fdS);

/* -------------------------------*/

   fdS = open ("SYSDB", O_CREAT + O_RDWR); if (fdS == -1) goto ERR;

   start = time(NULL);
/* Lock byte 0 to Read on 1st fd */
   flk.l_whence = 0; flk.l_start = 0; flk.l_len = 1; flk.l_type = F_RDLCK;
   i = fcntl (fdS, F_SETLKW, &flk); if (i == -1) goto ERR;

/* Lock byte 2 to Read on 1st fd */
   flk.l_whence = 0; flk.l_start = 2; flk.l_len = 1; flk.l_type = F_RDLCK;
   i = fcntl (fdS, F_SETLKW, &flk); if (i == -1) goto ERR;

/* Unlock byte 0 on 1st fd */
   flk.l_whence = 0; flk.l_start = 0; flk.l_len = 1; flk.l_type = F_UNLCK;
   i = fcntl (fdS, F_SETLK, &flk); if (i == -1) goto ERR;
   end = time(NULL);
   *lock_requests = *lock_requests + 3;
   *avg_lock_time = *avg_lock_time +
      ((double)(end - start) - *avg_lock_time) / *lock_requests;
   locksum += end - start;

/* Close 1st fd */
   close (fdS);
   open_end = time(NULL);
   *openclose_requests = *openclose_requests + 8;
   *avg_openclose_time = *avg_openclose_time +
      ((double)(open_end - open_start - locksum) - *avg_openclose_time) /
      *openclose_requests;
   return;

ERR: 
   printf ("Error %d %s\n", errno, sys_errlist[errno]);
   exit (1);
}
void final (
   double *avg_lock_time,
   long *lock_requests,
   double *avg_openclose_time,
   long *openclose_requests )
{
   int fdS, i;
   struct flock flk;
   time_t start, end, open_start, open_end;

   open_start = time(NULL);
   fdS = open ("SYSDB", O_CREAT + O_RDWR); if (fdS == -1) goto ERR;

   start = time(NULL);
/* Lock byte 0 to Write on 1st fd */
   flk.l_whence = 0; flk.l_start = 0; flk.l_len = 1; flk.l_type = F_WRLCK;
   i = fcntl (fdS, F_SETLKW, &flk); if (i == -1) goto ERR;

/* Lock byte 2 to Write on 1st fd */
   flk.l_whence = 0; flk.l_start = 2; flk.l_len = 1; flk.l_type = F_WRLCK;
   i = fcntl (fdS, F_SETLKW, &flk); if (i == -1) goto ERR;

/* Unlock byte 0 on 1st fd */
   flk.l_whence = 0; flk.l_start = 0; flk.l_len = 1; flk.l_type = F_UNLCK;
   i = fcntl (fdS, F_SETLK, &flk); if (i == -1) goto ERR;
   end = time(NULL);
   *lock_requests = *lock_requests + 3;
   *avg_lock_time = *avg_lock_time +
      ((double)(end - start) - *avg_lock_time) / *lock_requests;

/* Close 1st fd */
   close (fdS);
   open_end = time(NULL);
   *openclose_requests = *openclose_requests + 2;
   *avg_openclose_time = *avg_openclose_time +
      ((double)(open_end - open_start - (end - start)) - *avg_openclose_time) /
	  *openclose_requests;
   return;

ERR: 
   printf ("Error %d %s\n", errno, sys_errlist[errno]);
   exit (1);
}
