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
#include <errno.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>
#include <assert.h>

/*
 * mqtest.c
 *
 * 	test program for Posix.1b message queues using multiple procs
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
 *	-n <num>	max. number of messages in queue created
 *	-s <size>	max. size of a message in the queue created
 *	-m <num>	number of messages to be sent/received by each sproc
 *	-t <num>	number of sender/receiver sproc pairs to create
 *	-v		verbose output
 *
 *	<mq_name> 	name of queues on which the operations are
 *			performed
 *	
 */


/*
 * default values for command line parameters
 */
#define MSG_SIZE		4096
#define NUM_MSGS		16
#define NUM_SPROC_PAIRS		1
#define NUM_SND_RCV_MSGS	NUM_MSGS

extern int errno;
/*
 * default values for parameters used during queue creation
 */

#define MIN_MSG_SIZE	80
#define MSG_PERM	(O_RDWR)
#define MSG_MODE	(0777)

#define MQ_NAME_PREFIX "/tmp/mq"

static int verbose_mode = 0;
static int num_sproc_pairs = NUM_SPROC_PAIRS;
static int cr_msg_size = MSG_SIZE;
static int cr_num_msgs = NUM_MSGS;
static int cr_msg_perm = MSG_PERM;
static int cr_msg_mode = MSG_MODE;
static int mq_prio_max;

static char errbuf[80];
static char namebuf[80];
static char *notify_buf = NULL;
static int notify_buf_size = 0;
static void sp_recv_messages(void *arg);
static void sp_send_messages(void *arg);
static void sp_setattr(void *arg);
static void sp_unlink(void *arg);
static void sp_notify(void *arg);
static void msg_notify_handler(int signo, siginfo_t *info, void *context);
static int verify_chksum(char *buf, int size);
static void gen_chksum(char *buf, int size);

int send_recv_msgs = NUM_MSGS;

int notify_succeeded = 0, notify_on = 0;

#define EXIT(val)	exit((val))

#define THREAD_START(func, func_arg, flags, pid, exitval)		\
  	if ((pid = sproc(func, flags, func_arg)) < 0) {			\
		perror( "ERROR - mqtest_sproc: sproc failed");		\
		EXIT(exitval);						\
	}

#define THREAD_EXIT_WAIT(id, rvalp, exitval)			\
	if (waitpid((id), (rvalp), 0) != (id))  {		\
		perror("ERROR - mqtest_sproc: waitpid failed"); \
		EXIT(exitval);					\
	}

#define THREAD_EXIT_OK(retval)						\
	(WIFEXITED(retval) && (WEXITSTATUS(retval) == 0))

static void usage_exit(char *name)
{
	fprintf(stderr,"usage: %s [ -n <max_msgs>]",name);
	fprintf(stderr, " [ -s <max_msg_size> ]\n");
	fprintf(stderr, "\t[ -m <send/recv_msgs> ] [ -t <num_sproc_pairs> ] ");
	fprintf(stderr, " [ <mq_name> ]\n");
	EXIT(1);
}

main(int argc, char *argv[])
{
int  option;
int index, i;
mqd_t	mqd;
struct mq_attr attrs;
unsigned int msgprio;
extern char *optarg;
extern int optind;
char *mq_name = NULL, *cp;
size_t	msg_size;
int cr_mq_flags = O_NONBLOCK;
int errflg = 0, setattr = 0, notify = 0;
int unlink = 0, default_case = 0;
struct mq_attr mqattr, old_mqattr;
struct sigevent notify_sigev;
struct sigaction sa;
int	spret;
sigset_t sigset;
int tmp_cnt;
pid_t *pids;

	setbuf(stdout,NULL);
	setbuf(stderr,NULL);
	while ((option = getopt(argc, argv, "n:s:m:t:v")) != EOF)
		switch (option) {
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
			case 'm':
				send_recv_msgs = strtol(optarg, &cp, 0);
				if (cp == optarg)
					errflg = 1;
			break;
			case 't':
				num_sproc_pairs = strtol(optarg, &cp, 0);
				if (cp == optarg)
					errflg = 1;
			break;
			case 'v':
				verbose_mode = 1;
			break;
			default:
				usage_exit(argv[0]);
		}

	if (errflg) {
		usage_exit(argv[0]);
	}
	if (argc == optind)
		default_case = 1;

	if (default_case) {
		/*
		 * default case; generate a queue name
		 */
		sprintf(namebuf,"%sXXXXXX",MQ_NAME_PREFIX);
		(void) mktemp(namebuf);
		if (namebuf[0] == 0) {
			fprintf(stderr,
			"mqtest:failed to create to a unique mq name\n");
			return(2);
		}
		mq_name = namebuf;
	} else if (optind >= argc) {
		usage_exit(argv[0]);
	} else
		mq_name = argv[optind++];
	
	if ((mq_prio_max = sysconf(_SC_MQ_PRIO_MAX)) == -1) {
		perror("ERROR - mqtest_sproc: sysconf() failed");
		return(2);
	}
	/*
	 * set random number seed
	 */
	srandom((int) time(NULL));

	/*
	 * create queue
	 */

	fprintf(stderr,"\n%-20s %-15s\n\n"," ","mqtest_sproc");

	cr_msg_perm |= O_CREAT | O_EXCL;
	attrs.mq_maxmsg = cr_num_msgs;
	if (cr_msg_size < MIN_MSG_SIZE)
		cr_msg_size = MIN_MSG_SIZE;
	attrs.mq_msgsize = cr_msg_size;
	attrs.mq_flags = cr_mq_flags;
	mqd = mq_open(mq_name,cr_msg_perm,cr_msg_mode, &attrs);

	if ((int) mqd < 0) {
		sprintf(errbuf,
		"ERROR - mqtest_sproc: mq_open(%.20s) failed",mq_name);
		perror(errbuf);
		EXIT(2);
	}
	if (verbose_mode) {
		printf("Created Message Queue\n");
	}
	if (mq_getattr(mqd, &mqattr) < 0) {
		perror("mq_getattr failed");
		EXIT(2);
	}
	if (verbose_mode) {
		printf("%12s %10s %14s %8s %4s\n\n","Pid(pri)","Op",
					"Name", "Msg_Pri","Msg");
		printf("mq_name = %6s flags = 0x%x size = 0x%x ",
			mq_name, mqattr.mq_flags, mqattr.mq_msgsize);
		printf(" maxmsgs = %d curmsgs = %d\n",
					mqattr.mq_maxmsg, mqattr.mq_curmsgs);
	}
	/*
	 * create sprocs to execute the following mq operations
	 * mq_open, mq_send, mq_receive, mq_getattr, mq_setattr, mq_notify
	 * mq_close and mq_unlink
	 */

	/*
	 * create sender and receiver sprocs
	 */
	if ((pids = (pid_t *) malloc(num_sproc_pairs * 2 * sizeof(pid_t))) ==
								NULL) {
		fprintf(stderr,"ERROR - mqtest_sproc: malloc( %d bytes)\n",
				num_sproc_pairs * sizeof(pid_t));
		EXIT(2);
	}
	for (i = 0; i < num_sproc_pairs; i++) {
		THREAD_START(sp_recv_messages, mq_name, PR_SALL, pids[i*2], 2);
		THREAD_START(sp_send_messages, mq_name,PR_SALL,pids[i*2 + 1], 2);
	}
	/*
	 * wait for all sender and receiver sprocs to exit
	 */
	for (i = 0; i < num_sproc_pairs; i++) {
		THREAD_EXIT_WAIT(pids[2*i], (&spret),2);

		if (!THREAD_EXIT_OK(spret)) {
			fprintf(stderr,
		"ERROR - mqtest_sproc: sproc(mq_recv) %d exited with error\n",
					pids[2*i]);
			EXIT(2);
		}
		fprintf(stderr,"%-9s %-9d %-10s %-10d %-12s\n","Sproc",pids[2*i],
					"received", send_recv_msgs, "messages");
		THREAD_EXIT_WAIT(pids[2*i + 1], (&spret),2);
		if (!THREAD_EXIT_OK(spret)) {
			fprintf(stderr,
		"ERROR - mqtest_sproc: sproc(mq_send) %d exited with error\n",
					pids[2*i + 1]);
			EXIT(2);
		}
		fprintf(stderr,"%-9s %-9d %-10s %-10d %-12s\n", "Sproc",
			pids[2*i + 1], "sent", send_recv_msgs, "messages");
	}
										
	/*
	 * test mq_setattr() with sproc
	 */
	THREAD_START(sp_setattr, mq_name, PR_SALL, pids[0], 2);
	THREAD_EXIT_WAIT(pids[0], (&spret),2);
	if (!THREAD_EXIT_OK(spret)) {
		fprintf(stderr,
		"ERROR - mqtest_sproc: sproc(mq_setattr) %d exited with error\n",
				pids[0]);
		EXIT(2);
	}
	/*
	 * test mq_notify() with sproc
	 * start a sproc that calls mq_notify() and pauses() for a message.
	 * start a sender thead to send a message to the queue; the 
	 * waiting sproc should receive notification
	 */
	/*
	 * block SIGUSR1; only sp_notify sets up a handler for this signal
	 */
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGUSR1);
	if (sigprocmask(SIG_BLOCK, &sigset, NULL)) {
		perror( "ERROR - mqtest_sproc: sigprocmask failed");
		EXIT(2);
	}
	THREAD_START(sp_notify, mq_name, PR_SALL, pids[0], 2);
	/*
	 * wait till the notify request is set up
	 */
	while (notify_on == 0)
		sleep(1);

	/*
	 * start a sender sproc to send one message
	 */
	send_recv_msgs = 1;

	THREAD_START(sp_send_messages, mq_name, PR_SALL, pids[1], 2);
	THREAD_EXIT_WAIT(pids[1], (&spret),2);
	if (!THREAD_EXIT_OK(spret)) {
		fprintf(stderr,
		"ERROR - mqtest_sproc: sproc(mq_notify) %d exited with error\n",
				pids[1]);
		EXIT(2);
	}

	/*
	 * check status of the mq_notify sproc
	 */
	THREAD_EXIT_WAIT(pids[0], (&spret),2);
	if (!THREAD_EXIT_OK(spret)) {
		fprintf(stderr,
		"ERROR - mqtest_sproc: sproc(mq_notify) %d exited with error\n",
				pids[0]);
		EXIT(2);
	}

	/*
	 * start a sproc to test mq_unlink
	 */
	THREAD_START(sp_unlink, mq_name, PR_SALL, pids[0], 2);
	THREAD_EXIT_WAIT(pids[0], (&spret),2);
	if (!THREAD_EXIT_OK(spret)) {
		fprintf(stderr,
		"ERROR - mqtest_sproc: sproc(mq_unlink) %d exited with error\n",
				pids[0]);
		EXIT(2);
	}
	if (mq_close(mqd)) {
		perror("ERROR - mqtest_sproc: mq_close failed");
		EXIT(2);
	}
	fprintf(stderr,"\n%-15s %-s \n\n"," ","posix mq functions executed",
								"mq_open");
	fprintf(stderr,"%-8s %-15s "," ","mq_open");
	fprintf(stderr,"%-15s %-15s\n", "mq_getattr", "mq_setattr");
	fprintf(stderr,"%-8s %-15s "," ","mq_send");
	fprintf(stderr,"%-15s %-15s\n", "mq_receive", "mq_notify");
	fprintf(stderr,"%-8s %-15s %-15s\n", " ","mq_close", "mq_unlink");
	return(0);
}

/*
 * sp_recv_messages
 *	sproc function opens mq, gets attributes, receives messages and closes
 *	queue.
 */
void
sp_recv_messages(void *mq_name)
{
mqd_t mqd;
size_t	msg_size;
struct mq_attr mqattr;
char	*buf = NULL;
int buf_size = 0;
int index;
unsigned int msgprio;

	mqd = mq_open(mq_name, MSG_PERM);
	if ((int) mqd < 0) {
		sprintf(errbuf,
			"ERROR - mqtest_sproc: mq_open(%.20s) failed",mq_name);
		perror(errbuf);
		EXIT(2);
	}
	if (mq_getattr(mqd, &mqattr) < 0) {
		perror("mq_getattr failed");
		EXIT(2);
	}
	if (verbose_mode) {
		printf("%-12s %-10s %-8s %-8s %-4s\n\n","Pid","Op",
					"Bytes", "Msg_Pri","Msg");
		printf("mq_name = %6s flags = 0x%x size = 0x%x ",
			mq_name, mqattr.mq_flags, mqattr.mq_msgsize);
		printf(" maxmsgs = %d curmsgs = %d\n",
					mqattr.mq_maxmsg, mqattr.mq_curmsgs);
	}
	if ((buf = (char *) malloc(mqattr.mq_msgsize)) == NULL){
		fprintf(stderr,"ERROR - mqtest_sproc: malloc( %d bytes)\n",
					mqattr.mq_msgsize);
		EXIT(2);
	}
	buf_size = mqattr.mq_msgsize;

	for(index = 1; index <= send_recv_msgs; index++) {
		msgprio = 0;
		if ((msg_size = mq_receive(mqd, buf, mqattr.mq_msgsize,
					&msgprio)) == -1) {
			perror("ERROR - mqtest_sproc: mq_recv failed");
			EXIT(2);
		}
		if (verify_chksum(buf,msg_size)) {
			printf("%-12d %-10s %-8d %-8d %-30s\n", getpid(),
					"received", msg_size, msgprio, buf + 4);
			/*
			 * msg corrupted
			 */
			fprintf(stderr,
			"ERROR - mqtest_sproc: mq_recv - msg corruption\n");
			EXIT(2);
		}
		if (verbose_mode)
			printf("%-12d %-10s %-8d %-8d %-30s\n", getpid(),
					"received", msg_size, msgprio, buf + 4);
	}
	if (mq_close(mqd)) {
		perror("ERROR - mqtest_sproc: mq_close failed");
		EXIT(2);
	}
	EXIT(0);
}

/*
 * sp_send_messages
 *	sproc function opens mq, gets attributes, sends messages and closes
 *	queue.
 */
void
sp_send_messages(void *mq_name)
{
mqd_t mqd;
size_t	msg_size;
struct mq_attr mqattr;
char	*buf = NULL;
int buf_size = 0;
int index;
unsigned int msgprio;
int tmp_size;

	mqd = mq_open(mq_name, MSG_PERM);
	if ((int) mqd < 0) {
		sprintf(errbuf,
			"ERROR - mqtest_sproc: mq_open(%.20s) failed",mq_name);
		perror(errbuf);
		EXIT(2);
	}
	if (mq_getattr(mqd, &mqattr) < 0) {
		perror("ERROR - mqtest_sproc: mq_getattr failed");
		EXIT(2);
	}
	if (verbose_mode) {
		printf("%-12s %-10s %-8s %-8s %-4s\n\n","Pid","Op",
					"Name", "Msg_Pri","Msg");
		printf("mq_name = %6s flags = 0x%x size = 0x%x ",
			mq_name, mqattr.mq_flags, mqattr.mq_msgsize);
		printf(" maxmsgs = %d curmsgs = %d\n",
					mqattr.mq_maxmsg, mqattr.mq_curmsgs);
	}
	if ((buf = (char *) malloc(mqattr.mq_msgsize))== NULL){
		fprintf(stderr,"ERROR - mqtest_sproc: malloc( %d bytes)\n",
					mqattr.mq_msgsize);
		EXIT(2);
	}
	buf_size = mqattr.mq_msgsize;

	msgprio = 0;
	for(index = 1; index <= send_recv_msgs; index++) {
		/*
		 * write pid and msg index in to the message; fill rest of
		 * the message buffer with random numbers
		 * not be greater than MIN_MSG_SIZE
		 * the first four bytes of the message are use to save the
		 * checksum for the message
		 */
		sprintf(buf + 4,"Sproc %9d msg %4d",getpid(),index);
		msg_size = (random() % mqattr.mq_msgsize);
		if (msg_size < MIN_MSG_SIZE)
			msg_size = MIN_MSG_SIZE;
		tmp_size = msg_size;
		while (tmp_size > (4 + (strlen(buf + 4) + 1))) {
			buf[tmp_size - 1] = random();
			tmp_size--;
		}
		gen_chksum(buf,msg_size);
		msgprio = (random() % mq_prio_max);
		if (msgprio == 0)
			msgprio = 1;
		if (mq_send(mqd, buf, msg_size, msgprio)) {
			perror("ERROR - mqtest_sproc: mq_send failed");
			EXIT(2);
		}
		if (verbose_mode)
			printf("%-12d %-10s %-8d %-8d %-30s\n", getpid(),
					"sent", msg_size, msgprio, buf);
	}
	if (mq_close(mqd)) {
		perror("ERROR - mqtest_sproc: mq_close failed");
		EXIT(2);
	}
	EXIT(0);
}

/*
 * sp_setattr
 *	sproc function opens mq, sets attributes and closes queue.
 */
void
sp_setattr(void *mq_name)
{
mqd_t mqd;
size_t	msg_size;
struct mq_attr mqattr, old_mqattr;
char	*buf = NULL;
int buf_size = 0;
int index;
unsigned int msgprio;

	mqd = mq_open(mq_name, MSG_PERM);
	if ((int) mqd < 0) {
		sprintf(errbuf,
			"ERROR - mqtest_sproc: mq_open(%.20s) failed",mq_name);
		perror(errbuf);
		EXIT(2);
	}
	/*
	 * set the O_NONBLOCK flag, then clear it to make sure mq_setattr
	 * works
	 */
	mqattr.mq_flags = O_NONBLOCK;
	if (mq_setattr(mqd, &mqattr, &old_mqattr) < 0) {
		perror("ERROR - mqtest_sproc: mq_setattr failed");
		EXIT(2);
	}
	mqattr.mq_flags = 0;
	if (mq_setattr(mqd, &mqattr, &old_mqattr) < 0) {
		perror("ERROR - mqtest_sproc: mq_setattr failed");
		EXIT(2);
	}
	(void) memset(&mqattr, 0, sizeof(mqattr));
	if (mq_getattr(mqd, &mqattr) < 0) {
		perror("ERROR - mqtest_sproc: mq_getattr failed");
		EXIT(2);
	}
	if (verbose_mode) {
		printf("%14s flags = 0x%2.2x size = 0x%x ",
			"old attributes", old_mqattr.mq_flags,
						old_mqattr.mq_msgsize);
		printf("maxmsgs = %d curmsgs = %d\n", old_mqattr.mq_maxmsg,
					old_mqattr.mq_curmsgs);
		printf("%14s flags = 0x%2.2x size = 0x%x ",
			"new attributes", mqattr.mq_flags,
						mqattr.mq_msgsize);
		printf("maxmsgs = %d curmsgs = %d\n",
				mqattr.mq_maxmsg, mqattr.mq_curmsgs);
	}
	if (mq_close(mqd)) {
		perror("ERROR - mqtest_sproc: mq_close failed");
		EXIT(2);
	}
	if (((mqattr.mq_flags & O_NONBLOCK) == 0) &&
				((old_mqattr.mq_flags & O_NONBLOCK) != 0))
		fprintf(stderr,"%-9s %-9d %-s\n", "Sproc", getpid(),
				"set mq attributes");
	else
		fprintf(stderr,"ERROR - mqtest_sproc: %-9s %-9d %-s\n",
			"Sproc", getpid(), "failed to set mq attributes");
	EXIT(0);
}

/*
 * sp_unlink
 *	thread function unlinks mq
 */
void
sp_unlink(void *mq_name)
{

	if (mq_unlink(mq_name)) {
		perror("ERROR - mqtest_sproc: mq_unlink failed");
		EXIT(2);
	} else if ((mq_open(mq_name, MSG_PERM) >= 0) || (errno != ENOENT)) {
		fprintf(stderr,"ERROR - mqtest_sproc: %-9s %-5d %-s\n","Sproc",
				getpid(),
		"failed to unlink mq: mq_open succeeded after mq_unlink\n");
		EXIT(2);
	}
	fprintf(stderr,"%-9s %-9d %-s\n", "Sproc", getpid(),
				"unlinked mq");
	EXIT(0);
}

/*
 * sp_notify
 *	thread function calls mq_notify, waits for notification, reads message
 */
void
sp_notify(void *mq_name)
{
struct sigaction sa;
struct sigevent notify_sigev;
mqd_t mqd;
sigset_t sigset;

	mqd = mq_open(mq_name, MSG_PERM);
	if ((int) mqd < 0) {
		sprintf(errbuf,
			"ERROR - mqtest_sproc: mq_open(%.20s) failed",mq_name);
		perror(errbuf);
		EXIT(2);
	}
	sa.sa_sigaction = msg_notify_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGUSR1, &sa, NULL) < 0) {
		perror("ERROR - mqtest_sproc: sigaction failed");
		EXIT(2);
	}
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGUSR1);
	if (sigprocmask(SIG_UNBLOCK, &sigset, NULL)) {
		perror( "ERROR - mqtest_sproc: pthread_sigmask failed");
		EXIT(2);
	}
	notify_sigev.sigev_notify = SIGEV_SIGNAL;
	notify_sigev.sigev_signo =  SIGUSR1;
	notify_sigev.sigev_value.sival_ptr =  (void *) mqd;
	if (mq_notify(mqd, &notify_sigev)) {
		perror("ERROR - mqtest_sproc: mq_notify failed");
		EXIT(2);
	}
	notify_on = 1;
	pause();
	if (mq_close(mqd)) {
		perror("ERROR - mqtest_sproc: mq_close failed");
		EXIT(2);
	}
	if (notify_succeeded)
		EXIT(0);
	else
		EXIT(2);
}

void
msg_notify_handler(int signo, siginfo_t *info, void *context)
{
mqd_t mqd;
size_t	msg_size;
struct mq_attr mqattr;
unsigned int msgprio;
char	*buf = NULL;
int buf_size = 0;

	mqd = (mqd_t) info->si_value.sival_ptr;
	if (mq_getattr(mqd, &mqattr) < 0) {
		perror("ERROR - mqtest_sproc: mq_getattr failed");
		EXIT(2);
	}
	if ((buf = (char *) malloc(mqattr.mq_msgsize))== NULL){
		fprintf(stderr,"ERROR - mqtest_sproc: malloc( %d bytes)\n",
					mqattr.mq_msgsize);
		EXIT(2);
	}
	buf_size = mqattr.mq_msgsize;
	if ((msg_size = mq_receive(mqd, buf, buf_size, &msgprio)) == -1) {
		perror(
		"ERROR - mqtest_sproc: msg_notify_handler(mq_recv) failed");
		EXIT(2);
	}
	if (verbose_mode)
		printf("%-9d %-10s %-14d %-8d %-30s\n", getpid(),
				"received [notify]", msg_size, msgprio, buf);
	fprintf(stderr,"%-9s %-9d %-s\n", "Sproc", getpid(),
				"received message by async notification");
	notify_succeeded = 1;
}

/*
 * verify_chksum
 *	verify the checksum in the first four bytes of the buffer
 */
int
verify_chksum(char *buf, int size)
{
uint cksum;
uint i, *pi;
int shift;

	/*
	 * the buffer should be at least 5 bytes long
	 */
	assert(size > 4);
	cksum = 0;
	while (size > 4) {
		shift = (8 * (3 - (size % 4)));
		cksum = cksum ^ (buf[size - 1]  << (shift));
		/*
		printf("ver: buf[%d] = 0x%x\n",size - 1,buf[size - 1]);
		*/
		size--;
	}
	if (cksum == (*((unsigned int *) buf)))
		return 0;
	else
		return 1;
}
/*
 * gen_chksum
 *	save the checksum in the first four bytes of the buffer
 */
void
gen_chksum(char *buf, int size)
{
uint cksum;
uint i, *pi;
int shift;

	/*
	 * the buffer should be at least 5 bytes long
	 */
	assert(size > 4);
	cksum = 0;
	while (size > 4) {
		shift = (8 * (3 - (size % 4)));
		cksum = cksum ^ (buf[size - 1]  << shift);
		size--;
	}
	pi = (unsigned int *) buf;
	*pi = cksum;
}
