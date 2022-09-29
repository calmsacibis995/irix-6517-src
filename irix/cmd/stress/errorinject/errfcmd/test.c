#include <stdio.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <syslog.h>



main(int argc, char *argv[])
{
	char msg[256];
	sprintf(msg, "This is a test");
	openlog("test", LOG_PID|LOG_NDELAY, LOG_USER);	    
	syslog(LOG_INFO, msg);
	closelog();
	printf("Message %s \n", msg);
}
