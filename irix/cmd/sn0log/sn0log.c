#define SN0 1

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/SN/arch.h>
#include <sys/SN/router.h>
#include <sys/SN/SN0/sn0drv.h>
#include <sys/errno.h>
#include <syslog.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/SN/SN0/ip27log.h>

#include <capability.h>
#include <sys/capability.h>

#define MAX_DEV_PATH	255

void usage(char *cmd);
int get_stats(char *vertex_name, int fds, int dev_type, void *buf,
	      int error_mode, int verbose_mode);
int find_devs(char **vertices);
void open_devs(char **vertices, int *fds, int num_devs);
void print_hublog(char *vertex_name, int fd, int entire_log, int syslog_mode);
void clear_hublog(int fd, int init_log);

static int insufficient_privilege(void) ;

int
main(int argc, char *argv[])
{
	int i, c;
	int show_all = 0;
	int entire_log = 0;
	int syslog_mode = 0;
	int clear_log = 0;
	int init_log = 0;
	char vertex_name[MAX_DEV_PATH];
	char *vertices[MAX_NASIDS];
	int fds[MAX_NASIDS];
	
	int num_devs = 0;

	extern char *optarg;
	extern int optind;

	vertex_name[0] = '\0';

	while((c = getopt(argc, argv, "awscih")) != -1) {
		switch(c) {
			case 'a':
				show_all = 1;
				break;
			case 'w':
				entire_log = 1;
				break;
			case 's':
				syslog_mode = 1;
				break;
			case 'c':
				clear_log = 1;
				break;
			case 'i':
				init_log = 1;
				break;
			case 'h':
				usage(argv[0]);
		}
	}

        /* Check for su access */

        if ((init_log || clear_log) && (insufficient_privilege())) {
		fprintf(stderr, 
		"sn0log: Only privileged users can clear or init promlog\n") ;
                exit(1);
        }

	/* Check for specific file name */

	if (argc == optind + 1) {
		strcpy(vertex_name, argv[optind]);
		vertices[0] = vertex_name;
		vertices[1] = (char *)NULL;
		num_devs = 1;
	} else {
		if (!show_all)
			usage(argv[0]);
	}

	if ((clear_log || init_log) && (syslog_mode || entire_log)) {
		fprintf(stderr, "sn0log: Can't use syslog/entire log mode "
			"while clearing/initing log\n");
		exit(1);
	}

	if (show_all) {
		num_devs = find_devs(vertices);
	}

	if (!num_devs) {
		fprintf(stderr, "sn0log: Can't find any devices.\n");
		exit(1);
	}

	open_devs(vertices, fds, num_devs);

	if (syslog_mode) {
		openlog("sn0log", LOG_NDELAY, LOG_DAEMON);
		syslog(LOG_INFO, "The following are messages stored in the flashlog from a previous system boot.");
	}

	for (i = 0; i < num_devs; i++)
		if (clear_log || init_log)
			clear_hublog(fds[i], init_log);
		else
			print_hublog(vertices[i], fds[i], entire_log,
				syslog_mode);

	if (syslog_mode)
		syslog(LOG_INFO, "End of flashlog messages.");
	exit(0);
}

int
find_devs(char **vertices)
{
	char *cmd;
	char buf[MAX_DEV_PATH];
	FILE *ptr;
	int lines = 0;

	cmd = "ls -1 /hw/module/*/slot/*/node/hub/mon";
		
	if ((ptr = popen(cmd, "r")) != NULL) {
		while (fgets(buf, BUFSIZ, ptr) != NULL) {
			vertices[lines] = (char *)malloc(strlen(buf) + 1);
			if (buf[strlen(buf) - 1] == '\n')
				buf[strlen(buf) - 1] = '\0';
			strcpy(vertices[lines], buf);
			lines++;
		}
		pclose(ptr);
	} else {
		printf("sn0log: Command failed: %s\n", cmd);
	}
	return lines;
}

void
open_devs(char **vertices, int *fds, int num_devs)
{
	int i;
	int size;
	int maxsize = 0;

	for (i = 0; i < num_devs; i++) {
		fds[i] = open(vertices[i], O_RDONLY);

#ifdef DEBUG
		printf("Opened %s, fd == %d\n", vertices[i], fds[i]);
#endif

		if (fds[i] < 0) {
			printf("sn0log: Error opening %s\n", vertices[i]);
			perror("sn0log: open_devs");
			exit(1);
		}

	}
	
}

void format_output(char *vertex_name, char *flashbuf, int bytes,
		   int syslog_mode)
{
	cpuid_t cpu;
	int count;
	int prev;
	char temp;
	int lines = 1;

	if (syslog_mode)
		syslog(LOG_INFO, "Flashlog for %s",
		       vertex_name);
	else
		printf("Flashlog messages for %s\n", vertex_name);

/*
	for (cpu = 0; cpu < CPUS_PER_NODE; cpu++) {
*/

		count = prev = 0;

		while (count < bytes) {
#if 0
			/* If you need a cleaner display from a
			 * raw flash dump file, you can include this
			 */
			if (flashbuf[count] == 0xff) {
				count++;
				continue ;
			}
			if (!isascii(flashbuf[count])) {
				printf("0x%x ", flashbuf[count++]) ;
				continue ;
			}
#endif
			for (; (count < bytes) && (flashbuf[count] != '\n');
			     count++);

			/* Put in a temporary NULL */
			temp = flashbuf[count];
			flashbuf[count] = '\0';	

			if (!strstr(&flashbuf[prev], IP27LOG_DUP_DLM)) {
				if (syslog_mode)
					syslog(LOG_NOTICE, "%s\n", 
						&flashbuf[prev]);
				else
					printf("%s\n", &flashbuf[prev]);
			}

			flashbuf[count] = temp;

			lines++;
			prev = ++count;
		}
/*
	}
*/
	if (syslog_mode)
		syslog(LOG_INFO, "End of flashlog for %s\n", vertex_name);
	else
		printf("End of flashlog for %s\n", vertex_name);
}

void
clear_hublog(int fd, int init_log)
{
	char	errbuf[128];

	if (ioctl(fd, (init_log ? SN0DRV_INIT_LOG : SN0DRV_CLEAR_LOG), 0) < 0) {
		sprintf(errbuf, "sn0log: %s failed.", init_log ?
			"SN0DRV_INIT_LOG" : "SN0DRV_CLEAR_LOG");
		perror(errbuf);
		exit(1);
	}
}

/*
 * print_hublog_from_file
 *
 */
#include <sys/stat.h>
#include <sys/SN/fprom.h>

void
print_hublog_from_file(char *vertex_name, int fd, int syslog_mode)
{
	char 	*logbuf ;
	int	nbytes = 0 ;

	logbuf = (char *)malloc(2 * FPROM_SECTOR_SIZE) ;
	if (!logbuf) {
		perror("malloc") ;
		return ;
	}

	if (lseek64(fd, 14 * FPROM_SECTOR_SIZE, SEEK_SET) < 0) {
		perror("seek") ;
		return ;
	}

	if ((nbytes = read(fd, logbuf, (2 * FPROM_SECTOR_SIZE))) !=
					(2 * FPROM_SECTOR_SIZE)) {
		perror("read") ;
		return ;
	}

        format_output(vertex_name, logbuf, nbytes, syslog_mode);

	free(logbuf) ;
}


void
print_hublog(char *vertex_name, int fd, int entire_log, int syslog_mode)
{
	char *flashbuf;
	int bytes = 0;
	int log_ioctl;

	int size;

	struct stat	stat_buf ;

	/*
 	 * Check if the file is a char special or regular file.
 	 */

        if (	(stat(vertex_name, &stat_buf) >= 0) &&
		(S_ISREG(stat_buf.st_mode))) {

		/* Read flash log buf from file and call format_output */

		print_hublog_from_file(vertex_name, fd, syslog_mode) ;

		return ;
        }

	if ((size = ioctl(fd, SN0DRV_GET_FLASHLOGSIZE, 0)) < 0) {
		perror("sn0log: SN0DRV_GET_FLASHLOGSIZE failed.");
		exit(1);
	}

	flashbuf = (char *)malloc(size);

	if (entire_log)
		log_ioctl = SN0DRV_GET_FLASHLOGALL;
	else
		log_ioctl = SN0DRV_GET_FLASHLOGDATA;

	if ((bytes = ioctl(fd, log_ioctl, flashbuf)) < 0) {
		perror("sn0log: get_stats(HUBINFO_GETINFO)");
		exit(1);
	} else {
		if (bytes > 1)
			format_output(vertex_name, flashbuf, bytes,
				      syslog_mode);
	}

	/*
	 * Now mark the log as having been read.
	 */
	if (syslog_mode && (bytes > 1))
		ioctl(fd, SN0DRV_SET_FLASHSYNC, 0);

	free(flashbuf);
}

void
usage(char *cmd)
{
	fprintf(stderr, "Usage: %s [-wscih] { -a | hub_mon_dev }\n", cmd);
	exit(1);
}

/* -------------- Capability checks ------------------------ */

static int      cap_enabled ;

/*
 * cap_permitted
 *
 *      Read the user's capability set value. If he has the required
 *      effective permissions, return TRUE, else return FALSE.
 */

static int
cap_permitted(cap_value_t cap)
{
        cap_t           cap_state;
        int             result = 0 ;

        if (!cap_enabled)
                return (1);

        if ((cap_state = cap_get_proc()) == NULL) {
                return (0);
        }

        result = CAP_ID_ISSET(cap, cap_state->cap_effective) ;

        return result ;
}

/*
 * Procedure:   insufficient_privilege
 *
 * Job:         Checks user privilege. If Capability Mgmt
 *              is enabled (Trusted Irix, CMW, POSIX Capabilities)
 *              check if we have
 *
 *                      - CAP_SHUTDOWN
 *                      - CAP_SYSINFO_MGT
 *                      - CAP_DEVICE_MGT
 *
 *              The current functionality of mkpart seems to need
 *              only these 2 now. More can be added if needed later.
 *
 * Returns:     TRUE if user DOES NOT have sufficient privilege.
 *              FALSE if user DOES have sufficient privilege.
 *
 * Note:        Most of this code is derived from a similar
 *              routine in su.c
 *
 */

static int
insufficient_privilege (void)
{

        /*
         * Find out a few things about the system's configuration
         */
        if ((cap_enabled = sysconf(_SC_CAP)) < 0)
                cap_enabled = 0;

        /*
         * If it's a strict SuperUser environment the effective uid tells all.
         */
        if (cap_enabled == CAP_SYS_DISABLED)
                 return (geteuid());

        /*
         * If it's an augmented SuperUser environment an effective uid
         * of root (0) is sufficient.
         */
        if ((cap_enabled == CAP_SYS_SUPERUSER) && (geteuid() == 0))
                 return (0);

        /*
         * If it's a No SuperUser environment, or the euid is not root,
         * check the capabilities.
         */

        if (!cap_permitted(CAP_SHUTDOWN | CAP_SYSINFO_MGT | CAP_DEVICE_MGT))
                return (1);

        return (0);
}


