#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <mqueue.h>
#include <math.h>
#include <limits.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>
#include <signal.h>
#include <assert.h>
#include <errno.h>

/*
 * mqtest.c
 * 	test program for Posix.1b message queues
 *
 *	If no options/command-line arguments are present, basic operations
 *	of queue creation, message send/receive, queue close and queue
 *	unlink are executed.
 *
 *	if queue names, but no command-line arguments, are listed, the
 *	status of the queues is printed
 *
 *	command line options
 *
 *	-c		create a queue
 *	-n <num>	max. number of messages in queue created
 *	-s <size>	max. size of a message in the queue created
 *	-m <mode>	mode of the queue created
 *	-a [b | n]	if n is specified, set O_NONBLOCK flag
 *	-p [r | w]	open queue with read (r) or write (w) permission
 *	-S <num>	send "num" number of messages to the queue
 *	-R <num>	receive "num" number of messages from the queue
 *	-y		setup async notification request for the queue
 *	-u		unlink the queue
 *	-v		verbose output
 *
 *	<mq_name>
 *			name of queue on which the operations are
 *			performed (if no name is specified, a temp name
 *			is generated)
 *	
 */

/*
 * default values for parameters used during queue creation
 */

#define MSG_SIZE	128
#define MIN_MSG_SIZE	80
#define NUM_MSGS	32
#define MSG_PERM	(O_RDWR)
#define MSG_MODE	(0777)

#define MQ_NAME_PREFIX "/tmp/mq"

static long cr_msg_size = MSG_SIZE;
static long cr_num_msgs = NUM_MSGS;
static int cr_msg_perm = MSG_PERM;
static int cr_msg_mode = MSG_MODE;
static int mq_prio_max;
static int mypid;
static int attr_loop_test = 0;

static char errbuf[80];
static char *notify_buf = NULL;
static long notify_buf_size = 0;
static void msg_notify_handler(int signo, siginfo_t *info, void *context);
static int verbose;

#if _MIPS_SIM == _MIPS_SIM_ABI32
char *abi_type = "MIPS O32";
#endif

#if _MIPS_SIM == _MIPS_SIM_NABI32
char *abi_type = "MIPS N32";
#endif

#if  _MIPS_SIM == _MIPS_SIM_ABI64
char *abi_type = "MIPS N64";
#endif

static void usage_exit(char *name)
{
	fprintf(stderr,"usage: %s [ -c ] [ -n <num_msgs>]",name);
	fprintf(stderr, " [ -s <max_msg_size> [ -m mode]\n");
	fprintf(stderr, "\t[ -a b | -a n] [ -p r | -p w ] [ -S <send_msgs> ]");
	fprintf(stderr, "\n\t[ -R <recv_msgs> ] [ -y ] ");
	fprintf(stderr, " -v -u <mq_name>\n");
	exit(1);
}

main(int argc, char *argv[])
{
int  option;
long proc_prio;
int index;
mqd_t	mqd;
struct mq_attr attrs;
char	*buf = NULL;
long buf_size = 0;
unsigned int msgprio;
extern char *optarg;
extern int optind;
char mq_name[100] = {{0}};
char *cp;
size_t	msg_size;
int create = 0, cr_mq_flags = O_NONBLOCK;
long send_msgs = 0, recv_msgs = 0;
int errflg = 0, setattr = 0, notify = 0;
int unlink = 0, noargs = 0;
struct mq_attr mqattr, old_mqattr;
struct sigevent notify_sigev;
struct sigaction sa;

	setbuf(stdout,NULL);
	setbuf(stderr,NULL);
	mypid = getpid();
	if (argc == 1)
		noargs = 1;
	while ((option = getopt(argc, argv, "a:cn:s:p:m:yS:R:uv")) != EOF)
		switch (option) {
			case 'c':
				create = 1;
			break;
			case 'v':
				verbose = 1;
			break;
			case 'n':
				cr_num_msgs = strtol(optarg, &cp, 0);
				if (cp == optarg)
					errflg = 1;
			break;
			case 's':
				cr_msg_size = strtol(optarg, &cp, 0);
				if (cp == optarg)
					errflg = 1;
			break;
			case 'p':
				if (optarg[0] == 'r') {
					if (cr_msg_perm == O_WRONLY)
						cr_msg_perm = O_RDWR;
					else
						cr_msg_perm = O_RDONLY;
				} else if (optarg[0] == 'w') {
					if (cr_msg_perm == O_RDONLY)
						cr_msg_perm = O_RDWR;
					else
						cr_msg_perm = O_WRONLY;
				} else
					errflg = 1;
			break;
			case 'm':
				cr_msg_mode = (int) strtol(optarg, &cp, 0);
				if (cp == optarg)
					errflg = 1;
			break;
			case 'S':
				send_msgs = strtol(optarg, &cp, 0);
				if (cp == optarg)
					errflg = 1;
			break;
			case 'R':
				recv_msgs = strtol(optarg, &cp, 0);
				if (cp == optarg)
					errflg = 1;
			break;
			case 'a':
				setattr = 1;
				if (optarg[0] == 'b')
					cr_mq_flags &= ~O_NONBLOCK;
				else if (optarg[0] == 'n')
					cr_mq_flags |= O_NONBLOCK;
				else if (optarg[0] == 'l')
					attr_loop_test = 1;
				else
					errflg = 1;
			break;
			case 'u':
				unlink = 1;
			break;
			case 'y':
				notify = 1;
			break;
			default:
				usage_exit(argv[0]);
		}

	if (errflg) {
		usage_exit(argv[0]);
	}

	if (noargs) {
		/*
		 * no command line arguments specified; for this default
		 * case create a queue, send and receive messages and
		 * unlink the queue
		 */
		create = 1;
		send_msgs = NUM_MSGS;
		recv_msgs = NUM_MSGS;
		unlink = 1;
	}
	if ((optind == argc) || noargs) {
		sprintf(mq_name,"%sXXXXXX",MQ_NAME_PREFIX);
		(void) mktemp(mq_name);
		if (mq_name[0] == 0) {
			fprintf(stderr,
			"mqtest: ERROR - failed to create to a unique mq name\n");
			return(2);
		}
        } else if (optind >= argc) {
                usage_exit(argv[0]);
        } else
                strncpy(mq_name, argv[optind++], 100);

	/*
	 * set a fixed priority, randomly selected, for this process
	 */
	srandom((int) time(NULL));
	proc_prio =  (random() % (NDPNORMMIN - NDPNORMMAX + 1));
	proc_prio = proc_prio + NDPNORMMAX;
	assert((proc_prio >= NDPNORMMAX) && (proc_prio <= NDPNORMMIN));
	if (schedctl(NDPRI, 0, proc_prio) == -1L) {
		if (errno == EPERM) {
			printf("You must be root to run this test\n");
			exit(0);
		}
		perror("mqtest: ERROR - schedctl(NDPRI) failed");
		return(2);
	}

	if ((proc_prio = schedctl(GETNDPRI, 0, proc_prio)) == -1) {
		perror("mqtest: ERROR - schedctl(GETNDPRI) failed");
		return(2);
	}

	if ((mq_prio_max = (int) sysconf(_SC_MQ_PRIO_MAX)) == -1) {
		perror("mqtest: ERROR - sysconf() failed");
		return(2);
	}

	
	if (create) {
		cr_msg_perm |= O_CREAT | O_EXCL;
		attrs.mq_maxmsg = cr_num_msgs;
		attrs.mq_msgsize = cr_msg_size;
		attrs.mq_flags = cr_mq_flags;
		mqd = mq_open(mq_name,cr_msg_perm,cr_msg_mode, &attrs);

		if ((int) mqd != -1)
                	printf("mqtest: %s %-9s %-9d %-10s %s\n",
			       abi_type, "Proc", getpid(), "created", mq_name);
	} else
		mqd = mq_open(mq_name, cr_msg_perm);

	if ((int) mqd < 0) {
		fprintf(stderr, "mqtest: FAILED mq_open of %s failed",
			abi_type, mq_name);
		perror(errbuf);
		exit(2);
	}

	if (mq_getattr(mqd, &mqattr) < 0) {
		perror("mqtest: ERROR - mq_getattr failed");
		exit(2);
	}
	if (setattr) {
	 	if (attr_loop_test) {
			int z = 20;
			int set_to = O_NONBLOCK;
			while (z--) {
				mqattr.mq_flags = set_to;
				if (mq_setattr(mqd, &mqattr, &old_mqattr) < 0) {
				  fprintf(stderr,
					  "mqtest: FAILED %s mq_setattr failed\n",
					  abi_type);
				  exit(2);
				}			
				if (mq_getattr(mqd, &mqattr) < 0) {
				  fprintf(stderr,
					  "mqtest: FAILED %s mq_getattr failed\n",
					  abi_type);
				  exit(2);
				}
				if (mqattr.mq_flags != set_to) {
				  fprintf(stderr,
					  "mqtest: FAILED %s can't set mq attrs\n",
					  abi_type);
				  exit(1);
				}
				if (set_to)
					set_to = 0;
				else
					set_to = O_NONBLOCK;
			}
		}

		mqattr.mq_flags = cr_mq_flags;
		if (mq_setattr(mqd, &mqattr, &old_mqattr) < 0) {
			fprintf(stderr, "mqtest: FAILED %s mq_setattr failed",
				abi_type);
			exit(2);
		}
		if (verbose) {
			printf("%6s: %6s flags = 0x%2.2x size = 0x%x ",
					"old_mq", mq_name, old_mqattr.mq_flags,
					old_mqattr.mq_msgsize);
			printf("maxmsgs = %d curmsgs = %d\n",
				old_mqattr.mq_maxmsg, old_mqattr.mq_curmsgs);
			printf("%6s: %6s flags = 0x%2.2x size = 0x%x ",
				"mq", mq_name, mqattr.mq_flags,
							mqattr.mq_msgsize);
			printf("maxmsgs = %d curmsgs = %d\n",
					mqattr.mq_maxmsg, mqattr.mq_curmsgs);
		}
	}
	if (notify) {
		if ((notify_buf == NULL)  ||
				(notify_buf_size < mqattr.mq_msgsize)){
			if ((notify_buf) && 
				(notify_buf_size < mqattr.mq_msgsize)){
				free(notify_buf);
			}
			if ((notify_buf = 
				(char *) malloc(mqattr.mq_msgsize)) 
							== NULL) {
				fprintf(stderr,
				"mqtest: ERROR - failed to malloc %d bytes\n",
						mqattr.mq_msgsize);
				return(2);
			}
			notify_buf_size = mqattr.mq_msgsize;
		}
		/*
		 * signal handler for async notification
		 */
		sa.sa_sigaction = msg_notify_handler;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_SIGINFO;
		if (sigaction(SIGUSR1, &sa, NULL) < 0) {
			perror("mqtest: ERROR - sigaction failed");
			return(2);
		}
		
		notify_sigev.sigev_notify = SIGEV_SIGNAL;
		notify_sigev.sigev_signo =  SIGUSR1;
		notify_sigev.sigev_value.sival_ptr =  (void *) mqd;
		if (mq_notify(mqd, &notify_sigev)) {
			perror("mqtest: ERROR - mq_notify failed");
			return(2);
		}
/*
		printf("mq_notify: signo = %2d sival.ptr = 0x%x\n",
				notify_sigev.sigev_signo,
				notify_sigev.sigev_value.sival_ptr);
*/
	}

	if (verbose) {
		printf("%10s %10s %14s %8s %4s\n\n","Pid(pri)","Op",
				"Name/Bytes", "Msg_Pri","Msg");
		printf("mq_name = %6s flags = 0x%x size = 0x%x ",
		       mq_name, mqattr.mq_flags, mqattr.mq_msgsize);
		printf("maxmsgs = %d curmsgs = %d\n\n",
				mqattr.mq_maxmsg, mqattr.mq_curmsgs);
	}

	if ((buf == NULL)  || (buf_size < mqattr.mq_msgsize)) {
		if ((buf) && (buf_size < mqattr.mq_msgsize)) {
			free(buf);
		}
		if ((buf = (char *) malloc(mqattr.mq_msgsize))== NULL){
			fprintf(stderr,"failed to malloc %d bytes\n",
						mqattr.mq_msgsize);
			return(2);
		}
		buf_size = mqattr.mq_msgsize;
	}
	if ((!notify) && send_msgs) {
		srandom((int) time(NULL));
		for(index = 1; index <= send_msgs; index++) {
			sprintf(buf,"Process %d msg %d",mypid,index);
			msg_size = (int) (random() % mqattr.mq_msgsize);
			if (msg_size < MIN_MSG_SIZE)
				msg_size = MIN_MSG_SIZE;
			msgprio = (int) (random() % mq_prio_max);
			if (msgprio == 0)
				msgprio = 1;
			if (mq_send(mqd, buf, msg_size, msgprio)) {
				perror("mqtest: ERROR - mq_send failed");
				exit(2);
			}
			if (verbose)
				printf("mqtest: %s %-5d(%3d) %-10s %-14d %-8d %-32s\n",
				       abi_type, mypid, proc_prio, "sent", msg_size,
				       msgprio, buf);
/*
			printf(
		"Proc %5d(%3d) %8s %14d %2d Msg: %s \n",
		mypid, proc_prio, "sent", msg_size, msgprio, buf);
*/
		}
                printf("mqtest: %s %-9s %-9d %-10s %-10d %-12s\n",
		       abi_type, "Proc",getpid(), "sent", send_msgs, "messages");
	}
	if ((!notify) && recv_msgs) {
		msgprio = 0;
		for(index = 1; index <= recv_msgs; index++) {
			if ((msg_size = 
				mq_receive(mqd, buf, mqattr.mq_msgsize,
						&msgprio)) == -1) {
				perror("mq_recv failed");
				exit(2);
			}
			if (verbose)
				printf("mqtest: %s %5d(%3d) %10s %14d %8d %-32s\n",
				       abi_type, mypid, proc_prio, "received",
				       msg_size, msgprio, buf);
/*
			printf("Proc %5d(%3d) %8s %7d bytes ",mypid,
				proc_prio,"received", msg_size);
			printf( "pri %2d Msg: %s \n", msgprio, buf);
*/
		}
                printf("mqtest: %s %-9s %-9d %-10s %-10d %-12s\n",
		       abi_type, "Proc", getpid(), "received", recv_msgs, "messages");
	}
	if (!notify)
		mq_close(mqd);
	if (unlink) {
		if (mq_unlink(mq_name)) {
			fprintf(stderr, "mqtest: FAILED %s mq_unlink failed",
				abi_type);
			return(2);
		} else
                	printf("mqtest: %s %-9s %-9d %-10s %s\n",
			       abi_type, "Proc", getpid(), "unlinked", mq_name);
	}
	if (notify) {
		pause();
	}
	if (noargs) {
		fprintf(stderr,"\n%-15s %-s \n\n"," ",
				"posix mq functions executed", "mq_open");
		fprintf(stderr,"%-8s %-15s "," ","mq_open");
		fprintf(stderr,"%-15s %-15s\n", "mq_getattr", "mq_setattr");
		fprintf(stderr,"%-8s %-15s "," ","mq_send");
		fprintf(stderr,"%-15s %-15s\n", "mq_receive", "mq_notify");
		fprintf(stderr,"%-8s %-15s %-15s\n", " ","mq_close", "mq_unlink");
	}

	return(0);
}

/* ARGSUSED */
void
msg_notify_handler(int signo, siginfo_t *info, void *context)
{
mqd_t mqd;
size_t	msg_size;
unsigned int msgprio;

	mqd = (mqd_t) info->si_value.sival_ptr;
/*
	printf("msg_notify_handler: sig = %2d code = 0x%x i = 0x%x p = 0x%x\n",
				info->si_signo, info->si_code,
			info->si_value.sival_int, info->si_value.sival_ptr);
	printf("msg_notify_handler: signo = %2d mqd = 0x%x\n", signo, mqd);
*/
	if ((msg_size = mq_receive(mqd, notify_buf, notify_buf_size,
				&msgprio)) == -1) {
		perror("mqtest: ERROR - mq_notify_handler: mq_recv failed");
		exit(2);
	}
	printf("Proc %5d %8s %7d bytes ",mypid, "received", msg_size);
	printf("pri %2d [notify] Msg: %s \n", msgprio, notify_buf);
}
