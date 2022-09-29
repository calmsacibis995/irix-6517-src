
/* unixperf - an x11perf-style Unix performance benchmark */

/* test_stdio.c measures the C library's stdio speed. */

#include "unixperf.h"

#ifndef EX_OK /* if new EX_OK does not exist... */
#define EX_OK X_OK 
#endif

Bool
InitPopenEchoHello(Version version)
{
    if(access("/bin/echo", EX_OK) == 0) {
	return TRUE;
    } else {
        printf("/bin/echo not executable or not found\n");
        return FALSE;
    }
}

unsigned int
DoPopenEchoHello(void)
{
   FILE *echohello;
   char buffer[255];
   int i;

   for(i=10;i>0;i--) {
      echohello = popen("/bin/echo hello", "r"); 
      if(echohello == NULL) DOSYSERROR("popen");
      do {
	 ptr = fgets(buffer, sizeof(buffer), echohello);
      } while(ptr != NULL);
      pclose(echohello);
   }
   return 10;
}

