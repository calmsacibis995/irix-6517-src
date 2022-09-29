/*
 * Library for applications to request exclusive use of a device from
 * mediad.
 * 
 * $Id: libmediad.c,v 1.18 1997/07/11 00:28:11 rogerc Exp $
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <fcntl.h>
#include <signal.h>
#include <strings.h>
#include <bstring.h>
#include <syslog.h>
#include <mntent.h>
#include <errno.h>
#include "mediad.h"


#define NUM_STATIC_ELEMENTS 	10
#define FLAG_INUSE		0x1
#define FLAG_MEDIAD		0x2

static struct sigaction oact;		/* saved signal disposition */

typedef struct sock_ele {
     int flags;
     int sock;
     char *pathname;
} sock_ele_t;

static sock_ele_t static_sock_ele_array[NUM_STATIC_ELEMENTS];
static sock_ele_t *sock_ele_array = NULL;
static int sock_ele_size = 0;
static int mediad_errno = RMED_NOERROR;

#define SOCKELE_ID(x) ((x) - sock_ele_array)

static sock_ele_t *alloc_element(void);
static void free_element(int id);
static sock_ele_t *get_element(int id);
static int setup_socket(sock_ele_t *ptr);
static int send_message(sock_ele_t *ptr, emsg_t *msg);
static int recv_message(sock_ele_t *ptr, rmsg_t *reply_msg);
static int save_signals(void);
static void restore_signals(void);
static void get_mediad_sockname(char *);

/*
 * Tell mediad to suspend monitoring the given device.
 * Open a socket to mediad, send it a message, and keep the socket open.
 * If the program exits, the monitoring is started again.
 *
 * Returns an id equal to or greater than 0 on success, -1 on failure.
 * Returns an id even if mediad is not around, since that means mediad is not 
 * monitoring the device.
 */
int
mediad_get_exclusiveuse(const char *spec_file, const char *prog_name)
{

     emsg_t exclu_msg;
     rmsg_t reply_msg;
     sock_ele_t *sock_ele_ptr;
     int rcode;

     mediad_errno = -1;			/* assume a system error */

     if (spec_file == NULL || prog_name == NULL)
	  return -1;

     sock_ele_ptr = alloc_element();
     if (sock_ele_ptr == NULL)
	  return -1;

     rcode = setup_socket(sock_ele_ptr);
     if (rcode < 0) {
	  free_element(SOCKELE_ID(sock_ele_ptr));
	  return -1;
     }


     /*
      * Send mediad a message and get its reply.
      * If there was a failure, free the sock_ele structure.
      */

     exclu_msg.mtype = MSG_SUSPENDON;

     /*
      * Be careful not to overflow the stack
      */
     strncpy(exclu_msg.filename, spec_file, sizeof exclu_msg.filename);
     strncpy(exclu_msg.progname, prog_name, sizeof exclu_msg.progname);
     exclu_msg.filename[sizeof exclu_msg.filename - 1] = '\0';
     exclu_msg.progname[sizeof exclu_msg.progname - 1] = '\0';

     rcode = send_message(sock_ele_ptr, &exclu_msg);
     if (rcode < 0) {
	  syslog(LOG_INFO, "libmediad: an error occured while trying to send mediad a message.\n");
	  free_element(SOCKELE_ID(sock_ele_ptr));
	  return -1;
     }

     rcode = recv_message(sock_ele_ptr, &reply_msg);
     if (rcode < 0) {
	  syslog(LOG_INFO, "libmediad: an error occured while trying to receive a message from mediad.\n");
	  free_element(SOCKELE_ID(sock_ele_ptr));
	  return -1;
     }

     /*
      *	Ugliness.
      *
      * Copy the reply code to mediad_errno if we really did contact
      *	mediad.  Otherwise, leave mediad_errno set to -1.
      */

     if (sock_ele_ptr->flags & FLAG_MEDIAD)
	 mediad_errno = reply_msg.error;

     if (reply_msg.error == RMED_NOERROR) {
	  sock_ele_ptr->pathname = strdup(spec_file);
	  return (SOCKELE_ID(sock_ele_ptr));
     } else {
	  free_element(SOCKELE_ID(sock_ele_ptr));
	  return -1;
     }
	  
}


/*
 * Tell mediad to resume monitoring the device.
 *
 * I made this to always succeed so that users can just go about their
 * way after this call.
 */

void
mediad_release_exclusiveuse(int exclusiveuse_id)
{
     emsg_t resume_msg;
     sock_ele_t *sock_ele_ptr;

     sock_ele_ptr = get_element(exclusiveuse_id);
     if (sock_ele_ptr == NULL)
	  return;

     resume_msg.mtype = MSG_SUSPENDOFF;
     if (sock_ele_ptr->pathname)
	 strncpy(resume_msg.filename,
		 sock_ele_ptr->pathname, sizeof resume_msg.filename);

     send_message(sock_ele_ptr, &resume_msg);

     free_element(SOCKELE_ID(sock_ele_ptr));

     return;
}

extern int
mediad_last_error()
{
    return mediad_errno;
}

/*
 * ======================================================================
 * These are internal functions.
 * ======================================================================
 */


/*
 * Make a socket and connect it to mediad.
 * Returns greater than or equal to 0 on success, -1 on failure.
 * If I can't find the sockname of mediad in FSDTAB, then it is not
 * running and success is returned - the MEDIAD flag bit is not set so
 * that the layer below will hide the fact that mediad is not running
 * and just return success.
 */

static int 
setup_socket(sock_ele_t *ptr)
{
     struct sockaddr_un addr;
     char sockname[MNTMAXSTR];

     ptr->sock = socket(AF_UNIX, SOCK_STREAM, 0);
     if (ptr->sock < 0)
	  return -1;

     sockname[0] = '\0';
     get_mediad_sockname(sockname);
     if (sockname[0] == '\0') {
	  return 0;  
     } else {
	  ptr->flags |= FLAG_MEDIAD;
     }

     /*
      * If mediad is killed ungracefully, it will leave an entry in fsd.tab
      * which get_mediad_sockname will find.  An attempt to connect to it
      * will return an error, even though this should fall into the no
      * mediad running case.  Turn off the mediad flag and return 0.
      * Treat timeouts in the same way.
      */
     addr.sun_family = AF_UNIX;
     strcpy(addr.sun_path, sockname);
     if (-1 == connect(ptr->sock, (struct sockaddr *) &addr, 
		       strlen(addr.sun_path) + sizeof(addr.sun_family))) {
	  if (errno == ECONNREFUSED || errno == ENOENT ||
	      errno == ETIMEDOUT) {
	       ptr->flags &= ~FLAG_MEDIAD;
	  } else {
	       return -1;
	  }
     }

     return 1;
}


/*
 * Returns 1 on success, -1 on failure.  If no mediad is running, then
 * success is returned.
 */

static int
send_message(sock_ele_t *ptr, emsg_t *sock_msg)
{

     int rcode;

     if (!(ptr->flags & FLAG_MEDIAD))
	  return 1;

     /*
      * A write to a disconnected socket causes a SIGPIPE, so save
      * the original handler and put in a SIG_IGN for ourselves.
      */
     if (save_signals() != 0)
	  return -1;
     rcode = write(ptr->sock, sock_msg, sizeof(emsg_t));
     restore_signals();
     if (rcode < 0) {
	  return -1;
     }

     return 1;
}

/*
 * Gets a message on the socket.  Returns 1 if successful, -1 otherwise.
 * If no mediad is running, then its successful (and also munge the 
 * message to this effect.)
 */
static int
recv_message(sock_ele_t *ptr, rmsg_t *reply_msg)
{
     fd_set readfds;
     struct timeval timeout_val;
     int rcode;

     if (!(ptr->flags & FLAG_MEDIAD)) {
	  reply_msg->error = RMED_NOERROR;
	  return 1;
     }

     /*
      * Get a reply.  Use select to wait for at least EJECT_TIMEOUT seconds.
      * (mediad might be in the middle of an eject, and wont' be able
      * to respond in time.)
      */

     FD_ZERO(&readfds);
     FD_SET(ptr->sock, &readfds);
     timeout_val.tv_sec = MEDIAD_EXCLUSIVEUSE_TIMEOUT;
     timeout_val.tv_usec = 0;
     rcode = select(ptr->sock + 1, &readfds, NULL, NULL, &timeout_val);
     if ((rcode == 1) && (FD_ISSET(ptr->sock, &readfds)))
	  rcode = read(ptr->sock, reply_msg, sizeof(*reply_msg));
     else
	  rcode = -1;

     if (rcode < 0)
	  return -1;

     return 1;
}


/*
 * Return value of string mount option
 */
static char *
copt(struct mntent *mnt, char *opt, char *coptstr)
{
	char *str, *equal, *val = 0, *comma;

	if (str = hasmntopt(mnt, opt)) {
		strcpy(coptstr, str);
		if (equal = index(coptstr, '=')) {
			val = equal + 1;
			comma = strchr(val, ',');
			if (comma) {
				*comma = '\0';
			}
		}
	}
	return val;
}


static void
get_mediad_sockname(char *sockname)
{
#if 0
     FILE *fsdtab;
     struct mntent *mntp;
     char *name;
     char coptstr[MNTMAXSTR];

     fsdtab = setmntent(FSDTAB, "r");
     while (mntp = getmntent(fsdtab)) {
	  if (name = copt(mntp, SOCK, coptstr)) {
	       if (!strncmp(name, SOCK_TEMPLATE, MEDIAD_SOCK_LEN)) {
		    strcpy(sockname, name);
		    break;
	       }
	  }
     }
     endmntent(fsdtab);
#else
     strcpy(sockname, "/tmp/.mediad_socket");
#endif
}


/*
 * Save the old SIGPIPE signal handler in case the user had one set up.
 * Returns 0 on success.
 */
static int
save_signals(void)
{
     struct sigaction act;
     int rcode;

     act.sa_handler = SIG_IGN;
     act.sa_flags = 0;
     sigemptyset(&act.sa_mask);

     rcode = sigaction(SIGPIPE, &act, &oact);
     if (rcode != 0) {
	  return -1;
     }

     return 0;
}


static void
restore_signals(void)
{
     sigaction(SIGPIPE, &oact, NULL);
}

static void
initialize_array(void)
{
     int i=0;

     sock_ele_array = static_sock_ele_array;
     sock_ele_size = NUM_STATIC_ELEMENTS;

     while (i < NUM_STATIC_ELEMENTS) {
	  sock_ele_array[i].flags = 0;
	  sock_ele_array[i].sock = -1;
	  sock_ele_array[i].pathname = NULL;
	  i++;
     }
}


static sock_ele_t *
alloc_element(void)
{
     int i=0;
     int newbyte;
     sock_ele_t *old_array, *new_ele;

     if (sock_ele_size == 0)
	  initialize_array();

     while (i < sock_ele_size) {
	  if (!sock_ele_array[i].flags) {
	       sock_ele_array[i].flags = FLAG_INUSE;
	       return (&sock_ele_array[i]);
	  }
	  i++;
     }

     /*
      * All array elements are busy! Save the old pointer, and alloc a new
      * one.  Copy over the old info and initialize the rest of the array.
      * Free the old one if it is not the static one I started with.
      * Return the first new one.
      */
     old_array = sock_ele_array;
     newbyte = (sock_ele_size + NUM_STATIC_ELEMENTS) * sizeof(sock_ele_t);
     sock_ele_array = (sock_ele_t *) malloc(newbyte);
     if (sock_ele_array == NULL) {
	  syslog(LOG_WARNING, "libmediad: malloc of %d bytes failed, %m\n",
		 newbyte);
	  sock_ele_array = old_array;
	  return NULL;
     }

     for (i=0; i < sock_ele_size; i++) {
	  sock_ele_array[i] = old_array[i];
     }

     if (sock_ele_size > NUM_STATIC_ELEMENTS)
	  free(old_array);

     new_ele = &sock_ele_array[i];
     sock_ele_size += NUM_STATIC_ELEMENTS;
     while (i < sock_ele_size) {
	  sock_ele_array[i].flags = 0;
	  sock_ele_array[i].sock = 0;
	  i++;
     }

     new_ele->flags = FLAG_INUSE;
     return new_ele;
}


static void
free_element(int id)
{

     if (sock_ele_size == 0)
	  initialize_array();

     if (id < 0 || id >= sock_ele_size)
	  return;

     if (sock_ele_array[id].sock >= 0) {
	  close(sock_ele_array[id].sock);
	  sock_ele_array[id].sock = -1;
     }
     if (sock_ele_array[id].pathname) {
	  free(sock_ele_array[id].pathname);
	  sock_ele_array[id].pathname = NULL;
     }
     sock_ele_array[id].flags = 0;
}


static sock_ele_t *
get_element(int id)
{

     if (sock_ele_size == 0)
	  initialize_array();

     if (id < 0 || id >= sock_ele_size)
	  return NULL;

     if (sock_ele_array[id].flags & FLAG_INUSE)
	  return (&sock_ele_array[id]);
     else
	  return NULL;

}
