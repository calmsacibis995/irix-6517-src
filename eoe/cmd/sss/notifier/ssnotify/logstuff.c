#include "logstuff.h"
#include "globals.h"

int logstuff(int errnum, char *message)
{
    char  xxmybuffer[BUFFER];
    int   nobytes = 0;

    if ( !message ) return(0);

    if ( errnum ) {
	nobytes = snprintf(xxmybuffer, sizeof(xxmybuffer), "%s: %s", message, strerror(errnum));
    } else {
	nobytes = snprintf(xxmybuffer, sizeof(xxmybuffer), "%s", message);
    }

    if ( isatty(fileno(stderr)))  {
	fprintf(stderr, "%s\n", xxmybuffer);
    } else {
	openlog("espnotify", LOG_PID, LOG_LOCAL0);
	syslog(LOG_ERR, xxmybuffer);
	closelog();
    }

    return(0);
}

#if 0
int logstuff(int errnum, char *message)
{
	int fd = 0;
	
	short status = OK;
	pid_t   stu_pid;

        char error_string[BUFFER];

	char *test;
	char newtime[16];
	
        time_t the_time;

	stu_pid = getpid();

	memset(newtime, 0, 16);

        if((fd = open(logfileloc, O_WRONLY|O_CREAT|O_APPEND, 0644)) == -1) {
		status = NOTOK;
		fprintf(stderr, "Can't open %s for writing\n", logfileloc);
        }

	else {
		time(&the_time);
		
		cftime(newtime, "%b %d %r", &the_time);	
	
	        if(errnum) {
	                char *errno_text;
	
	                errno_text = strerror(errnum);
	
	                snprintf(error_string, BUFFER, "[%d] %s %s: %s\n", stu_pid, newtime, message, errno_text);
	
	                if(write(fd, error_string, strlen(error_string)) == -1) {
				status = NOTOK;
	                }

	        }
	
		else {
	                snprintf(error_string, BUFFER, "[%d] %s %s\n", stu_pid, newtime, message);
	
	                if(write(fd, error_string, strlen(error_string)) == -1) {
				status = NOTOK;
	                }
	        }

		close(fd);
	}

	return status;
}	
#endif
