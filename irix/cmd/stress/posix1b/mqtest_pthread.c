#include <stdlib.h>
#include <pthread.h>
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
#include <signal.h>
#include <assert.h>

/*
 * mqtest_pthread.c
 *
 * 	test program for Posix.1b message queues using pthreads
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
 *	-m <num>	number of messages to be sent/received by each pthread
 *	-t <num>	number of sender/receiver pthread pairs to create
 *	-d		print additional debug information
 *	-v		verbose output
 *
 *	<mq_name> 	name of queues on which the operations are
 *			performed
 *	
 */


/*
 * default values for command line parameters
 */
#define MSG_SIZE		128
#define NUM_MSGS		32
#define NUM_PTHREAD_PAIRS	1
#define NUM_SND_RCV_MSGS	NUM_MSGS

/*
 * default values for parameters used during queue creation
 */

#define MIN_MSG_SIZE	80
#define MSG_PERM	(O_RDWR)
#define MSG_MODE	(0777)

#define MQ_NAME_PREFIX "/tmp/mq"

static int verbose_mode = 0;
static long num_pthread_pairs = NUM_PTHREAD_PAIRS;
static long cr_msg_size = MSG_SIZE;
static long cr_num_msgs = NUM_MSGS;
static int cr_msg_perm = MSG_PERM;
static int cr_msg_mode = MSG_MODE;
static int mq_prio_max;

static char errbuf[80];
static char namebuf[80];
static void *pt_recv_messages(void *arg);
static void *pt_send_messages(void *arg);
static void *pt_setattr(void *arg);
static void *pt_unlink(void *arg);
static void *pt_notify(void *arg);
static void msg_notify_handler(int signo, siginfo_t *info, void *context);
static int verify_chksum(char *buf, unsigned long size);
static void gen_chksum(char *buf, unsigned long size);

long send_recv_msgs = NUM_MSGS;

int notify_succeeded = 0, notify_on = 0;

static void usage_exit(char *name)
{
	fprintf(stderr,"usage: %s [ -n <max_msgs>]",name);
	fprintf(stderr, " [ -s <max_msg_size> ]\n");
	fprintf(stderr, "\t[ -m <send/recv_msgs> ] [ -t <num_pthread_pairs> ] ");
	fprintf(stderr, " [ <mq_name> ]\n");
	exit(1);
}

main(int argc, char *argv[])
{
int  option;
int i;
mqd_t	mqd;
struct mq_attr attrs;
extern char *optarg;
extern int optind;
char *mq_name = NULL, *cp;
int cr_mq_flags = O_NONBLOCK;
int errflg = 0;
int  default_case = 0;
struct mq_attr mqattr;
pthread_t *pts;
void	*ptret;
sigset_t sigset;
long tmp_cnt;

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
				num_pthread_pairs = strtol(optarg, &cp, 0);
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
	
	if ((mq_prio_max = (int) sysconf(_SC_MQ_PRIO_MAX)) == -1) {
		perror("ERROR - mqtest_pthread: sysconf() failed");
		return(2);
	}
	/*
	 * create queue
	 */

	printf("\n%-20s %-15s\n\n"," ","mqtest_pthread");

	cr_msg_perm |= O_CREAT | O_EXCL;
	attrs.mq_maxmsg = cr_num_msgs;
	if (cr_msg_size < MIN_MSG_SIZE)
		cr_msg_size = MIN_MSG_SIZE;
	attrs.mq_msgsize = cr_msg_size;
	attrs.mq_flags = cr_mq_flags;
	mqd = mq_open(mq_name,cr_msg_perm,cr_msg_mode, &attrs);

	if ((int) mqd < 0) {
		sprintf(errbuf,
		"ERROR - mqtest_pthread: mq_open(%.20s) failed",mq_name);
		perror(errbuf);
		exit(2);
	}
	if (verbose_mode) {
		printf("Created Message Queue\n");
	}
	if (mq_getattr(mqd, &mqattr) < 0) {
		perror("mq_getattr failed");
		exit(2);
	}
	printf("mq_name = %6s flags = 0x%x size = 0x%x ",
		mq_name, mqattr.mq_flags, mqattr.mq_msgsize);
	printf(" maxmsgs = %d curmsgs = %d\n\n",
				mqattr.mq_maxmsg, mqattr.mq_curmsgs);
	if (verbose_mode) {
		printf("%12s %10s %14s %8s %4s\n\n","Pid(pri)","Op",
					"Name", "Msg_Pri","Msg");
	}
	/*
	 * create pthreads to execute the following mq operations
	 * mq_open, mq_send, mq_receive, mq_getattr, mq_setattr, mq_notify
	 * mq_close and mq_unlink
	 *
	 * also create a receiver thread, cancel it, start a pair of
	 * sender/receiver threads to make sure cancellation did not
	 * corrupt the state of the queue
	 *
	 */
	/*
	 * create sender and receiver threads
	 */
	if ((pts =
	(pthread_t *) malloc(num_pthread_pairs * 2 * sizeof(pthread_t))) ==
								NULL) {
		fprintf(stderr,"ERROR - mqtest_pthread: malloc( %d bytes)\n",
				num_pthread_pairs * sizeof(pthread_t));
		exit(2);
	}
	for (i = 0; i < num_pthread_pairs; i++) {
		if ((pthread_create(pts + i*2, NULL, pt_recv_messages,
				     (void *) mq_name)) != 0)  {
			perror(
			"ERROR - mqtest_pthread: pthread_create failed");
			exit(2);
		}
		if ((pthread_create(pts + (i*2 + 1), NULL, pt_send_messages,
				     (void *) mq_name)) != 0)  {
			perror(
			"ERROR - mqtest_pthread: pthread_create failed");
			exit(2);
		}
	}
	/*
	 * wait for all sender and receiver threads to exit
	 */
	for (i = 0; i < num_pthread_pairs; i++) {
		if (pthread_join(pts[2*i], &ptret) != 0)  {
			perror("ERROR - mqtest_pthread: pthread_join failed");
			exit(2);
		}

		if (ptret != 0) {
			fprintf(stderr,
		"ERROR - mqtest_pthread: pthread(mq_recv) %d exited with error\n",
					pts[2*i]);
			exit(2);
		}
		fprintf(stderr,"%-9s %-9d %-10s %-10d %-12s\n", "Pthread", pts[2*i],
					"received", send_recv_msgs, "messages");
		if (pthread_join(pts[2*i + 1], &ptret) != 0) {
			perror("ERROR - mqtest_pthread: pthread_join failed");
			exit(2);
		}
		if (ptret != 0) {
			fprintf(stderr,
		"ERROR - mqtest_pthread: pthread(mq_send) %d exited with error\n",
					pts[2*i + 1]);
			exit(2);
		}
		fprintf(stderr,"%-9s %-9d %-10s %-10d %-12s\n", "Pthread",
			pts[2*i + 1], "sent", send_recv_msgs, "messages");
	}
	/*
	 * test mq_setattr() with pthread
	 */
	if ((pthread_create(pts, NULL, pt_setattr, (void *) mq_name)) != 0)  {
		perror( "ERROR - mqtest_pthread: pthread_create failed");
		exit(2);
	}
	if (pthread_join(pts[0], &ptret) != 0)  {
		perror("ERROR - mqtest_pthread: pthread_join failed");
		exit(2);
	}
	if (ptret != 0) {
		fprintf(stderr,
		"ERROR - mqtest_pthread: pthread(mq_setattr) %d exited with error\n",
				pts[0]);
		exit(2);
	}
	/*
	 * test mq_notify() with pthread
	 * start a thread that call mq_notify() and pauses() for a message.
	 * start a sender thead to send a message to the queue; the 
	 * thread should receive notification
	 */
	/*
	 * block SIGUSR1; only pt_notify sets up a handler for this signal
	 */
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGUSR1);
	if (pthread_sigmask(SIG_BLOCK, &sigset, NULL)) {
		perror( "ERROR - mqtest_pthread: pthread_sigmask failed");
		exit(2);
	}
	if ((pthread_create(pts, NULL, pt_notify, (void *) mq_name)) != 0)  {
		perror( "ERROR - mqtest_pthread: pthread_create failed");
		exit(2);
	}
	/*
	 * wait till the notify request is set up
	 */
	while (notify_on == 0)
		sleep(1);

	/*
	 * start a sender thread to send one message
	 */
	send_recv_msgs = 1;

	if ((pthread_create(pts + 1, NULL, pt_send_messages,(void *) mq_name)) != 0) {
		perror( "ERROR - mqtest_pthread: pthread_create failed");
		exit(2);
	}
	if (pthread_join(pts[1], &ptret) != 0)  {
		perror("ERROR - mqtest_pthread: pthread_join failed");
		exit(2);
	}
	if (ptret != 0) {
		fprintf(stderr,
		"ERROR - mqtest_pthread: pthread(mq_mq_notify) %d exited with error\n",
				pts[1]);
		exit(2);
	}

	/*
	 * check status of the mq_notify thread
	 */
	if (pthread_join(pts[0], &ptret) != 0)  {
		perror("ERROR - mqtest_pthread: pthread_join failed");
		exit(2);
	}
	if (ptret != 0) {
		fprintf(stderr,
		"ERROR - mqtest_pthread: pthread(mq_mq_notify) %d exited with error\n",
				pts[0]);
		exit(2);
	}
	/*
	 * test cancellation of mq_send/mq_receive
	 */

	send_recv_msgs = 1;
	if ((pthread_create(pts, NULL, pt_recv_messages,(void *) mq_name)) != 0) {
		perror( "ERROR - mqtest_pthread: pthread_create failed");
		exit(2);
	}
	sleep(1);
	/*
	 * cancel the receiver
	 */
	if (pthread_cancel(pts[0]) != 0) {
		perror("ERROR: mqtest_pthread: pthread_cancel failed");
		exit(2);
	}
	if (pthread_join(pts[0], &ptret) != 0)  {
		perror("ERROR - mqtest_pthread: pthread_join failed");
		exit(2);
	}
	if (ptret != PTHREAD_CANCELED) {
		fprintf(stderr,
		"ERROR - mqtest_pthread: cancellation of pthread %d failed\n",
				pts[0]);
		exit(2);
	} else {
		fprintf(stderr,"%-9s %-9d %-10s\n", "Pthread", pts[0],
					"cancelled");
	}
	/*
	 * now send and receive a few messages to test for queue state being
	 * ok after thread cancellation
	 */
	/*
	 * create upto 4 (arbitrary) sender and receiver threads
	 */
	if (num_pthread_pairs < 4)
		tmp_cnt = num_pthread_pairs;
	else
		tmp_cnt = 4;

	for (i = 0; i < tmp_cnt; i++) {
		if ((pthread_create(pts + i*2, NULL, pt_recv_messages,
				     (void *) mq_name)) != 0)  {
			perror(
			"ERROR - mqtest_pthread: pthread_create failed");
			exit(2);
		}
		if ((pthread_create(pts + (i*2 + 1), NULL, pt_send_messages,
				     (void *) mq_name)) != 0)  {
			perror(
			"ERROR - mqtest_pthread: pthread_create failed");
			exit(2);
		}
	}
	/*
	 * wait for all sender and receiver threads to exit
	 */
	for (i = 0; i < tmp_cnt; i++) {
		if (pthread_join(pts[2*i], &ptret) != 0)  {
			perror("ERROR - mqtest_pthread: pthread_join failed");
			exit(2);
		}

		if (ptret != 0) {
			fprintf(stderr,
		"ERROR - mqtest_pthread: pthread(mq_recv) %d exited with error\n",
					pts[2*i]);
			exit(2);
		}
		if (pthread_join(pts[2*i + 1], &ptret) != 0) {
			perror("ERROR - mqtest_pthread: pthread_join failed");
			exit(2);
		}
		if (ptret != 0) {
			fprintf(stderr,
		"ERROR - mqtest_pthread: pthread(mq_send) %d exited with error\n",
					pts[2*i + 1]);
			exit(2);
		}
	}


	/*
	 * start a pthread to test mq_unlink
	 */
	if ((pthread_create(pts, NULL, pt_unlink, (void *) mq_name)) != 0)  {
		perror( "ERROR - mqtest_pthread: pthread_create failed");
		exit(2);
	}
	if (pthread_join(pts[0], &ptret) != 0)  {
		perror("ERROR - mqtest_pthread: pthread_join failed");
		exit(2);
	}
	if (ptret != 0) {
		fprintf(stderr,
		"ERROR - mqtest_pthread: pthread(mq_unlink) %d exited with error\n",
				pts[0]);
		exit(2);
	}
	if (mq_close(mqd)) {
		perror("ERROR - mqtest_pthread: mq_close failed");
		exit(2);
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
 * pt_recv_messages
 *	thread function opens mq, gets attributes, receives messages and closes queue.
 */
void *
pt_recv_messages(void *mq_name)
{
mqd_t mqd;
size_t	msg_size;
struct mq_attr mqattr;
char	*buf = NULL;
int index;
unsigned int msgprio;

	mqd = mq_open(mq_name, MSG_PERM);
	if ((int) mqd < 0) {
		sprintf(errbuf,
			"ERROR - mqtest_pthread: mq_open(%.20s) failed",mq_name);
		perror(errbuf);
		pthread_exit((void *) 2L);
	}
	if (mq_getattr(mqd, &mqattr) < 0) {
		perror("mq_getattr failed");
		pthread_exit((void *) 2L);
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
		fprintf(stderr,"ERROR - mqtest_pthread: malloc( %d bytes)\n",
					mqattr.mq_msgsize);
		pthread_exit((void *) 2L);
	}

	for(index = 1; index <= send_recv_msgs; index++) {
		msgprio = 0;
		if ((msg_size = mq_receive(mqd, buf, mqattr.mq_msgsize,
					&msgprio)) == -1) {
			perror("ERROR - mqtest_pthread: mq_recv failed");
			pthread_exit((void *) 2L);
		}
		if (verify_chksum(buf,msg_size)) {
			printf("%-12d %-10s %-8d %-8d %-30s\n", pthread_self(),
					"received", msg_size, msgprio, buf + 4);
			/*
			 * msg corrupted
			 */
			fprintf(stderr,
				"ERROR - mqtest_pthread: mq_recv - msg corruption\n");
			pthread_exit((void *) 2L);
		}
		if (verbose_mode)
			printf("%-12d %-10s %-8d %-8d %-30s\n", pthread_self(),
					"received", msg_size, msgprio, buf + 4);
	}
	if (mq_close(mqd)) {
		perror("ERROR - mqtest_pthread: mq_close failed");
		pthread_exit((void *) 2L);
	}
	pthread_exit((void *) 0);
	return(0);
}

/*
 * pt_send_messages
 *	thread function opens mq, gets attributes, sends messages and closes queue.
 */
void *
pt_send_messages(void *mq_name)
{
mqd_t mqd;
size_t	msg_size;
struct mq_attr mqattr;
char	*buf = NULL;
int index;
unsigned int msgprio;
unsigned long tmp_size;
unsigned int rand_seed;

	mqd = mq_open(mq_name, MSG_PERM);
	if ((int) mqd < 0) {
		sprintf(errbuf,
			"ERROR - mqtest_pthread: mq_open(%.20s) failed",mq_name);
		perror(errbuf);
		pthread_exit((void *) 2L);
	}
	if (mq_getattr(mqd, &mqattr) < 0) {
		perror("ERROR - mqtest_pthread: mq_getattr failed");
		pthread_exit((void *) 2L);
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
		fprintf(stderr,"ERROR - mqtest_pthread: malloc( %d bytes)\n",
					mqattr.mq_msgsize);
		pthread_exit((void *) 2L);
	}

	/*
	 * seed for random number
	 */
	rand_seed = time(NULL);
	msgprio = 0;
	for(index = 1; index <= send_recv_msgs; index++) {
		/*
		 * write pid and msg index in to the message; fill rest of
		 * the message buffer with random numbers
		 * not be greater than MIN_MSG_SIZE
		 * the first four bytes of the message are use to save the checksum
		 * for the message
		 */
		sprintf(buf + 4,"Pthread %9d msg %4d",pthread_self(),index);
		msg_size = (rand_r(&rand_seed) % mqattr.mq_msgsize);
		if (msg_size < MIN_MSG_SIZE)
			msg_size = MIN_MSG_SIZE;
		tmp_size = msg_size;
		while (tmp_size > (4 + (strlen(buf + 4) + 1))) {
			buf[tmp_size - 1] = (char) rand_r(&rand_seed);
			tmp_size--;
		}
		gen_chksum(buf,msg_size);
		msgprio = (int) (rand_r(&rand_seed) % mq_prio_max);
		if (msgprio == 0)
			msgprio = 1;
		if (mq_send(mqd, buf, msg_size, msgprio)) {
			perror("ERROR - mqtest_pthread: mq_send failed");
			pthread_exit((void *) 2L);
		}
		if (verbose_mode)
			printf("%-12d %-10s %-8d %-8d %-30s\n", pthread_self(),
					"sent", msg_size, msgprio, buf);
	}
	if (mq_close(mqd)) {
		perror("ERROR - mqtest_pthread: mq_close failed");
		pthread_exit((void *) 2L);
	}
	pthread_exit((void *) 0L);
	return(0);
}

/*
 * pt_setattr
 *	thread function opens mq, sets attributes and closes queue.
 */
void *
pt_setattr(void *mq_name)
{
mqd_t mqd;
struct mq_attr mqattr, old_mqattr;

	mqd = mq_open(mq_name, MSG_PERM);
	if ((int) mqd < 0) {
		sprintf(errbuf,
			"ERROR - mqtest_pthread: mq_open(%.20s) failed",mq_name);
		perror(errbuf);
		pthread_exit((void *) 2L);
	}
	/*
	 * set the O_NONBLOCK flag, then clear it to make sure mq_setattr
	 * works
	 */
	mqattr.mq_flags = O_NONBLOCK;
	if (mq_setattr(mqd, &mqattr, NULL) < 0) {
		perror("ERROR - mqtest_pthread: mq_setattr failed");
		pthread_exit((void *) 2L);
	}
	mqattr.mq_flags = 0;
	if (mq_setattr(mqd, &mqattr, &old_mqattr) < 0) {
		perror("ERROR - mqtest_pthread: mq_setattr failed");
		pthread_exit((void *) 2L);
	}
	(void) memset(&mqattr, 0, sizeof(mqattr));
	if (mq_getattr(mqd, &mqattr) < 0) {
		perror("ERROR - mqtest_pthread: mq_getattr failed");
		pthread_exit((void *) 2L);
	}
	if (verbose_mode) {
		printf("%14s flags = 0x%2.2x size = 0x%x ",
			"old attributes", old_mqattr.mq_flags, old_mqattr.mq_msgsize);
		printf("maxmsgs = %d curmsgs = %d\n", old_mqattr.mq_maxmsg,
					old_mqattr.mq_curmsgs);
		printf("%14s flags = 0x%2.2x size = 0x%x ",
			"new attributes", mqattr.mq_flags,
						mqattr.mq_msgsize);
		printf("maxmsgs = %d curmsgs = %d\n",
				mqattr.mq_maxmsg, mqattr.mq_curmsgs);
	}
	if (mq_close(mqd)) {
		perror("ERROR - mqtest_pthread: mq_close failed");
		pthread_exit((void *) 2L);
	}
	if (((mqattr.mq_flags & O_NONBLOCK) == 0) &&
				((old_mqattr.mq_flags & O_NONBLOCK) != 0))
		fprintf(stderr,"%-9s %-9d %-s\n", "Pthread", pthread_self(),
				"set mq attributes");
	else
		fprintf(stderr,"ERROR - mqtest_pthread: %-9s %-9d %-s\n",
			"Pthread", pthread_self(), "failed to set mq attributes");
	pthread_exit((void *) 0L);
	return(0);
}

/*
 * pt_unlink
 *	thread function unlinks mq
 */
void *
pt_unlink(void *mq_name)
{

	if (mq_unlink(mq_name)) {
		perror("ERROR - mqtest_pthread: mq_unlink failed");
		pthread_exit((void *) 2L);
	} else if ((mq_open(mq_name, MSG_PERM) >= 0) || (errno != ENOENT)) {
		fprintf(stderr,"ERROR - mqtest_pthread: %-9s %-5d %-s\n","Pthread",
				pthread_self(),
			"failed to unlink mq: mq_open succeeded after mq_unlink\n");
		pthread_exit((void *) 2L);
	}
	fprintf(stderr,"%-9s %-9d %-s\n", "Pthread", pthread_self(),
				"unlinked mq");
	pthread_exit((void *) 0L);
	return(0);
}

/*
 * pt_notify
 *	thread function calls mq_notify, waits for notification, reads message
 */
void *
pt_notify(void *mq_name)
{
struct sigaction sa;
struct sigevent notify_sigev;
mqd_t mqd;
sigset_t sigset;

	mqd = mq_open(mq_name, MSG_PERM);
	if ((int) mqd < 0) {
		sprintf(errbuf,
			"ERROR - mqtest_pthread: mq_open(%.20s) failed",mq_name);
		perror(errbuf);
		pthread_exit((void *) 2L);
	}
	sa.sa_sigaction = msg_notify_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGUSR1, &sa, NULL) < 0) {
		perror("ERROR - mqtest_pthread: sigaction failed");
		pthread_exit((void *) 2L);
	}
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGUSR1);
	if (pthread_sigmask(SIG_UNBLOCK, &sigset, NULL)) {
		perror( "ERROR - mqtest_pthread: pthread_sigmask failed");
		exit(2);
	}
	notify_sigev.sigev_notify = SIGEV_SIGNAL;
	notify_sigev.sigev_signo =  SIGUSR1;
	notify_sigev.sigev_value.sival_ptr =  (void *) mqd;
	if (mq_notify(mqd, &notify_sigev)) {
		perror("ERROR - mqtest_pthread: mq_notify failed");
		pthread_exit((void *) 2L);
	}
	notify_on = 1;
	pause();
	if (mq_close(mqd)) {
		perror("ERROR - mqtest_pthread: mq_close failed");
		pthread_exit((void *) 2L);
	}
	if (notify_succeeded)
		pthread_exit((void *) 0L);
	else
		pthread_exit((void *) 2L);
	return(0);
}

/* ARGSUSED */
void
msg_notify_handler(int signo, siginfo_t *info, void *context)
{
mqd_t mqd;
size_t	msg_size;
struct mq_attr mqattr;
unsigned int msgprio;
char	*buf = NULL;
long buf_size = 0;

	mqd = (mqd_t) info->si_value.sival_ptr;
	if (mq_getattr(mqd, &mqattr) < 0) {
		perror("ERROR - mqtest_pthread: mq_getattr failed");
		pthread_exit((void *) 2L);
	}
	if ((buf = (char *) malloc(mqattr.mq_msgsize))== NULL){
		fprintf(stderr,"ERROR - mqtest_pthread: malloc( %d bytes)\n",
					mqattr.mq_msgsize);
		pthread_exit((void *) 2L);
	}
	buf_size = mqattr.mq_msgsize;
	if ((msg_size = mq_receive(mqd, buf, buf_size, &msgprio)) == -1) {
		perror("ERROR - mqtest_pthread: msg_notify_handler(mq_recv) failed");
		pthread_exit((void *) 2L);
	}
	if (verbose_mode)
		printf("%-9d %-10s %-14d %-8d %-30s\n", pthread_self(),
				"received [notify]", msg_size, msgprio, buf);
	fprintf(stderr,"%-9s %-9d %-s\n", "Pthread", pthread_self(),
				"received message by async notification");
	notify_succeeded = 1;
}

/*
 * verify_chksum
 *	verify the checksum in the first four bytes of the buffer
 */
int
verify_chksum(char *buf, unsigned long size)
{
uint cksum;
int shift;

	/*
	 * the buffer should be at least 5 bytes long
	 */
	assert(size > 4);
	cksum = 0;
	while (size > 4) {
		shift = (int) (8 * (3 - (size % 4)));
		cksum = cksum ^ (buf[size - 1]  << (shift));
		/*
		printf("ver: buf[%d] = 0x%x\n",size - 1,buf[size - 1]);
		*/
		size--;
	}
	/*
	printf("verify_chksum: cksum = 0x%x buf[0] = 0x%x\n",cksum,
						(*((unsigned int *) buf)));
	*/
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
gen_chksum(char *buf, unsigned long size)
{
uint cksum;
uint  *pi;
int shift;

	/*
	 * the buffer should be at least 5 bytes long
	 */
	assert(size > 4);
	cksum = 0;
	while (size > 4) {
		shift = (int) (8 * (3 - (size % 4)));
		/*
		printf("gen: buf[%d] = 0x%x\n",size - 1,buf[size - 1]);
		*/
		cksum = cksum ^ (buf[size - 1]  << shift);
		size--;
	}
	pi = (unsigned int *) buf;
	*pi = cksum;
	/*
	printf("gen_chksum: 0x%x: %s\n",*pi,buf + 4);
	*/
}
