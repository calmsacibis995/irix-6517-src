#include "sys/types.h"
#include "sys/time.h"
#include "Client.h"
#include "fam.h"
#include "stdio.h"
#include "string.h"
#include "bstring.h"
#include <sys/param.h>
#include <syslog.h>
#include <errno.h>

//#define DEBUG


// Some libserver stuff
#define FAMNUMBER 391002 // This is the official rpc number for fam.
			 // We used to use getrpcbyname but it added too much
			  // to the size of the executable!  We also got rid
		       // of gethostbyname for the same reason.
#define FAMNAME "sgi_fam"
#define FAMVERS 1
#define LOCALHOSTNUMBER 0x7f000001 // Internet number for loopback.

// Error variables - XXX currently not dealt with
#define FAM_NUM_ERRORS 1
int FAMErrno;
char *FamErrlist[FAM_NUM_ERRORS];

// XXX Place this somewhere
static void FAMInit();
static int FAMFindFreeReqnum();
static void* FAMReqnumToUserData(int reqnum);
static void FAMStoreUserData(int reqnum, void* p);
static void FAMFreeUserData(int reqnum);
static int FAMReqnumToIndex(int reqnum);

// Arrays of user data
static int maxUserData = 200;
static int growBy = 100;
static void** userDataArray;
static int* userDataIndexArray;
static char* userEndExistArray;

/**************************************************************************
* FAMOpen, FAMClose - openning, closing FAM connections
**************************************************************************/

unsigned long _fam_program = FAMNUMBER;
unsigned long _fam_version = FAMVERS;

int FAMOpen2(FAMConnection* fc, const char* appName)
{
    FAMInit();
    Client *client = new Client();
    fc->client = (void *)client;
    fc->fd = client->AddServer(LOCALHOSTNUMBER, _fam_program, _fam_version);
    if (fc->fd < 0) {
	delete client;
	return(-1);
    }

    // Send App name 
    if (appName) {
	char msg[200];
	sprintf(msg, "N0 %d %d %s\n", getuid(), getgid(), appName);
	client->WriteToServer(fc->fd, msg, strlen(msg)+1);
    }

    return(0);
}

int FAMOpen(FAMConnection* fc)
{
    return FAMOpen2(fc, (const char *)NULL);
}

int FAMClose(FAMConnection* fc)
{
    delete (Client *)fc->client;
    return(0);
}



/**************************************************************************
* FAMMonitorDirectory, FAMMonitorFile - monitor directory or file
**************************************************************************/

#define ILLEGAL_REQUEST -1

int FAMMonitor(FAMConnection *fc, const char *filename, FAMRequest* fr,
	       void* userData, int code)
{

    if (!filename || filename[0] != '/')
	return -1;

    int reqnum = fr->reqnum;
    // store user data if necessary
    FAMStoreUserData(reqnum, userData);

    // Create group string
    char groupStr[NGROUPS*8], *p;
    gid_t groups[NGROUPS];
    int ngroups;
    ngroups = getgroups(NGROUPS, groups);
    groupStr[0] = '\0';
    p = groupStr;
    for (int i=1; i<ngroups; i++) {
	if (i == 1) {
	    sprintf(p, " %d", ngroups-1);
	    p += strlen(p);
	}
	sprintf(p, " %d", groups[i]);
	p += strlen(p);
    }

    // Create FAM Str
    char msg[MSGBUFSIZ], *msg2;
    int msgLen;
    if (*groupStr) {

	// First message - standard file message
	sprintf(msg,"%c%d %d %d %s\n", code, reqnum, getuid(), groups[0], 
		filename);
	msg2 = msg + strlen(msg) + 1;

	// second message - list of extra groups
	sprintf(msg2, "%s", groupStr);
	msgLen = strlen(msg) + strlen(msg2) + 2;
    } else {
	sprintf(msg,"%c%d %d %d %s\n", code, reqnum, getuid(), getgid(), 
		filename);
	msgLen = strlen(msg) + 1;
    }

    // Send to FAM
    ((Client *)fc->client)->WriteToServer( fc->fd, msg, msgLen);
    return(0);
}
int FAMMonitorDirectory(FAMConnection *fc,
			const char *filename,
			FAMRequest* fr,
			void* userData)
{
    // Fill out request structure
    int reqnum;
    if ((reqnum = FAMFindFreeReqnum()) == ILLEGAL_REQUEST) {
	return(-1);
    }
    fr->reqnum = reqnum;

	// Check path length of file
	if (strlen(filename) > MAXPATHLEN) {
		syslog(LOG_ALERT, "path too long\n");
		errno = ENAMETOOLONG;
		return (-1);
	}

#ifdef DEBUG
printf("FAMMonitorDirectory filename:%s, reqnum:%d, ud:%d\n", filename, fr->reqnum, (int) userData);
#endif
    return FAMMonitor(fc, filename, fr, userData, 'M');
}
int FAMMonitorDirectory2(FAMConnection *fc,
			 const char *filename,
			 FAMRequest* fr)
{
    return FAMMonitor(fc, filename, fr, (void*) fr->reqnum, 'M');
}

int FAMMonitorFile(FAMConnection *fc,
		   const char *filename,
		   FAMRequest* fr,
		   void* userData)
{
    // Fill out request structure
    int reqnum;
    if ((reqnum = FAMFindFreeReqnum()) == ILLEGAL_REQUEST) {
	return(-1);
    }
    fr->reqnum = reqnum;

	// Check path length of file
	if (strlen(filename) > MAXPATHLEN) {
		syslog(LOG_ALERT, "path too long\n");
		errno = ENAMETOOLONG;
		return (-1);
	}

#ifdef DEBUG
printf("FAMMonitorFile filename:%s, reqnum:%d, ud:%d\n", filename, fr->reqnum, (int) userData);
#endif
    return FAMMonitor(fc, filename, fr, userData, 'W');
}
int FAMMonitorFile2(FAMConnection *fc,
		    const char *filename,
		    FAMRequest* fr)
{
    return FAMMonitor(fc, filename, fr, (void*) fr->reqnum, 'W');
}


int 
FAMMonitorCollection(FAMConnection* fc,
		     const char* filename,
		     FAMRequest* fr,
		     void* userData,
		     int depth,
		     const char* mask)
{
    // Fill out request structure
    int reqnum;
    if ((reqnum = FAMFindFreeReqnum()) == ILLEGAL_REQUEST) {
	return(-1);
    }
    fr->reqnum = reqnum;

	// Check path length of file
	if (strlen(filename) > MAXPATHLEN) {
		syslog(LOG_ALERT, "path too long\n");
		errno = ENAMETOOLONG;
		return (-1);
	}

    // store user data if necessary
    if (userData) FAMStoreUserData(reqnum, userData);

    // Create group string
    char groupStr[NGROUPS*8], *p;
    gid_t groups[NGROUPS];
    int ngroups;
    ngroups = getgroups(NGROUPS, groups);
    groupStr[0] = '\0';
    p = groupStr;
    for (int i=1; i<ngroups; i++) {
	if (i == 1) {
	    sprintf(p, " %d", ngroups-1);
	    p += strlen(p);
	}
	sprintf(p, " %d", groups[i]);
	p += strlen(p);
    }

    // Create FAM Str
    char msg[MSGBUFSIZ], *msg2;
    int msgLen;
    if (groupStr[0] == '\0') {

        // First Message
	sprintf(msg,"F%d %d %d %s\n", reqnum, getuid(), 
		groups[0], filename);
	msg2 = msg + strlen(msg) + 1;

	// second message - list of extra groups
	sprintf(msg2, "0 %d %s\n", depth, mask);

    } else {

	// First Message
	sprintf(msg,"F%d %d %d %s\n", reqnum, getuid(), groups[0], filename);
	msg2 = msg + strlen(msg) + 1;

	// Second message
	sprintf(msg2, "%s %d %s\n", groupStr, depth, mask);
    }
    msgLen = strlen(msg) + strlen(msg2) + 2;

    // Send to FAM
    ((Client *)fc->client)->WriteToServer( fc->fd, msg, msgLen);
    return(0);
}


/**************************************************************************
* FAMMonitorRemoteDirectory, FAMMonitorRemoteFile - monitor directory or file
**************************************************************************/
#if 0
FAMRequest *FAMMonitorRemoteDirectory(FAMConnection *fc, const char *hostname,
				      const char *filename, FAMCallback func)
{
}

FAMRequest *FAMMonitorRemoteFile(FAMConnection *fc, const char *hostname, 
				 const char *filename, FAMCallback func)
{
}
#endif


/**************************************************************************
* FAMSuspendMonitor, FAMSuspendMonitor - suspend FAM monitoring
**************************************************************************/

int FAMSuspendMonitor(FAMConnection *fc, const FAMRequest *fr)
{
    // Create FAM String
    char msg[MSGBUFSIZ];
    sprintf(msg,"S%d %d %d\n", fr->reqnum, getuid(), getgid());

    // Send to FAM
    ((Client *)fc->client)->WriteToServer(fc->fd, msg, strlen(msg)+1);
    return(0);
}

int FAMResumeMonitor(FAMConnection *fc, const FAMRequest *fr)
{
    // Create FAM String
    char msg[MSGBUFSIZ];
    sprintf(msg,"U%d %d %d\n", fr->reqnum, getuid(), getgid());

    // Send to FAM
    ((Client *)fc->client)->WriteToServer(fc->fd, msg, strlen(msg)+1);
    return(0);
}



/**************************************************************************
* FAMCancelMonitor, FAMCancelMonitor - cancel FAM monitoring
**************************************************************************/

int FAMCancelMonitor(FAMConnection *fc, const FAMRequest* fr)
{
#ifdef DEBUG
printf("FAMCancelMonitor reqnum:%d\n", fr->reqnum);
#endif
    // Remove from user Data array
    // Actually, we will do this when we receive the ack back from fam
    // FAMFreeUserData(fr->reqnum);

    // Create FAM String
    char msg[MSGBUFSIZ];
    sprintf(msg,"C%d %d %d\n", fr->reqnum, getuid(), getgid());

    // Send to FAM
    ((Client *)fc->client)->WriteToServer(fc->fd, msg, strlen(msg)+1);
    return (0);
}


/**************************************************************************
* FAMNextEvent() - find the next fam event
* FAMPending() - return if events are ready yet
**************************************************************************/

static void
FAMParseMessage(FAMConnection* fc, char* msg, FAMEvent* fe)
{
    // 
    char last, code, changeInfo[100];
    int reqnum;
    int index;

    sscanf( msg, "%c", &code );
// can't use straight scanf here, because some filenames have spaces in them
/*
    if (code == 'c') {
	sscanf( msg, "%c%d %s %s%c", &code, &reqnum, changeInfo, fe->filename, 
				     &last );
    } else if (code == 'C') {
	sscanf( msg, "%c%d %s", &code, &reqnum, fe->filename );
    } else {
	sscanf( msg, "%c%d %s", &code, &reqnum, fe->filename );
    }
*/
    char *c;
    int i=0;
    if (code == 'c') {
	sscanf( msg, "%c%d %s", &code, &reqnum, changeInfo);
	char *g = strchr(msg, ' ');
	g++;
	c = strchr(g, ' ');
	if ( c ) {
	    c++;
	    while (*c != '\0') {
		fe->filename[i++] = *c++;
	    }
	    last = fe->filename[i-1];
	    fe->filename[i-1] = '\0';
	}
	
    } else {
	sscanf( msg, "%c%d", &code, &reqnum);
	c = strchr(msg, ' ');
	if ( c ) {
	    c++;
	    while (*c != '\0') {
		fe->filename[i++] = *c++;
	    }
	    fe->filename[i-1] = '\0';
	}
    }
    fe->fc = fc;
    fe->fr.reqnum = reqnum;

    // Find user Data
    fe->userdata = FAMReqnumToUserData(reqnum);

    switch (code) {
	case 'e': // new - XXX what about exists ?
	    index = FAMReqnumToIndex(reqnum);
	    if (userEndExistArray[index]) {
		fe->code = FAMCreated;
	    } else {
		fe->code = FAMExists;
	    }
	    break;
	// NOTE: just changed these next two to make sure that an
	// end exist has been sent - if not, we are just going to
	// give a exist message
	case 'F':
	    index = FAMReqnumToIndex(reqnum);
	    if (userEndExistArray[index]) {
		fe->code = FAMCreated;
	    } else {
		fe->code = FAMExists;
	    }
	    break;
	case 'D':
	    index = FAMReqnumToIndex(reqnum);
	    if (userEndExistArray[index]) {
		fe->code = FAMChanged;
	    } else {
		fe->code = FAMExists;
	    }
	    break;
	case 'c': // change
	case 'C':
	    fe->code = FAMChanged;
	    break;
	case 'A':
	case 'n': // delete
	    fe->code = FAMDeleted;
	    break;
	case 'x': // start execute
	case 'X':
	    fe->code = FAMStartExecuting;
	    break;
	case 'q': // quit execute
	case 'Q':
	    fe->code = FAMStopExecuting;
	    break;
	case 'P':
	    fe->code = FAMEndExist;
	    index = FAMReqnumToIndex(reqnum);
	    userEndExistArray[index] = 1;
	    break;
	case 'G':
	    // XXX we should be able to free the user data here
	    FAMFreeUserData(reqnum);
	    fe->code = FAMAcknowledge;
	    break;
	default:
	    break;
    }
#ifdef DEBUG
printf("\nFAM received %s  ", msg);
printf("translated to event code:%d, reqnum:%d, ud:%d, filename:<%s>\n",
	fe->code, reqnum, fe->userdata, fe->filename);
#endif

}

//  FAMNextEvent tries to read one complete message into the input buffer.
//  If it's successful, it parses the message and returns 1.
//  If it reads an EOF before it has a complete message, it returns 0.
//  If it gets an error, it returns -1.

int FAMNextEvent(FAMConnection* fc, FAMEvent* fe)
{
    Client *client = (Client *)fc->client;
    ServerEntry *server = client->Slist;
    int nb = server->inend - server->instart;
    if (nb < sizeof (__uint32_t))
    {   int rc = read(fc->fd, server->inend, sizeof (__uint32_t) - nb);
	if (rc <= 0)
	    return -1;
	nb += rc;
	server->inend += rc;
	if (nb < sizeof (__uint32_t))
	    return 0;
    }
    nb -= sizeof (__uint32_t);
    __uint32_t msglen;
    getword(server->instart, &msglen);
    int rc = read(fc->fd, server->inend, msglen - nb);
    if (rc <= 0)
	return rc;
    FAMParseMessage(fc, server->instart + sizeof (__uint32_t), fe);
    server->instart = server->inend = server->inbuf;
    return 1;
}

//  FAMPending tries to read all but the last byte of a message.
//  If it's successful, and select() says there's still more to
//  read, then it returns true.  Also, if it reads EOF, it
//  returns true.
//
//  The input buffer never holds more than one message.

int FAMPending(FAMConnection* fc)
{
    fd_set tfds;
    struct timeval tv = { 0, 0 };
    FD_ZERO(&tfds);
    FD_SET(fc->fd, &tfds);
    if (select(fc->fd + 1, &tfds, NULL, NULL, &tv) == 0)
	return 0;
    Client *client = (Client *)fc->client;
    ServerEntry *server = client->Slist;
    int nb = server->inend - server->instart;
    if (nb < sizeof (__uint32_t))
    {   int rc = read(fc->fd, server->inend, sizeof (__uint32_t) - nb);
	if (rc <= 0)
	    return 1;			// EOF now
	nb += rc;
	server->inend += rc;
	if (nb < sizeof (__uint32_t))
	    return 0;
    }
    nb -= sizeof (__uint32_t);
    __uint32_t msglen;
    getword(server->instart, &msglen);
    if (nb >= msglen - 1)
	return 1;
    int rc = read(fc->fd, server->inend, msglen - nb - 1);
    if (rc <= 0)
	return 1;			// EOF now
    server->inend += rc;
    nb += rc;
    if (select(fc->fd + 1, &tfds, NULL, NULL, &tv) > 0)
	return 1;
    return 0;
}



/**************************************************************************
* FAMDebugLevel() - sets the fam debugging level
**************************************************************************/
int FAMDebugLevel(FAMConnection* fc, int debugLevel)
{
    char msg[200];
    char opcode;
    switch (debugLevel) {
	case FAM_DEBUG_OFF: opcode = 'E'; break;
	case FAM_DEBUG_ON: opcode = 'D'; break;
	case FAM_DEBUG_VERBOSE: opcode = 'V'; break;
	default: return(-1);
    }
    sprintf(msg, "%c\n", opcode);
    ((Client *)fc->client)->WriteToServer( fc->fd, msg, strlen(msg)+1);
    return(1);
}



/**************************************************************************
* Support Functions
**************************************************************************/

void  FAMInit()
{
    static int firstTime = 1;

    // allocate arrays
    if (firstTime) {
	userDataArray = (void**) malloc(maxUserData * sizeof(void*));
	userDataIndexArray = (int*) malloc(maxUserData * sizeof(int*));
	userEndExistArray = (char*) malloc(maxUserData * sizeof(char*));
	for (int i=0; i<maxUserData; i++) {
	    userDataIndexArray[i] = -1;
	}
	firstTime = 0;
    }
}


int FAMFindFreeReqnum()
{
    static int reqnum=1;
    return(reqnum++);
}

void* FAMReqnumToUserData(int reqnum)
{
    for (int i=0; i<maxUserData; i++) {
	if (userDataIndexArray[i] == reqnum) return(userDataArray[i]);
    }
    return(0);
}

int FAMReqnumToIndex(int reqnum)
{
    for (int i=0; i<maxUserData; i++) {
	if (userDataIndexArray[i] == reqnum) return(i);
    }
    return(-1);
}


void FAMStoreUserData(int reqnum, void* p)
{
    int i;
    for (i=0; i<maxUserData; i++) {
	if (userDataIndexArray[i] == -1) {
	    userDataIndexArray[i] = reqnum;
	    userDataArray[i] = p;
	    userEndExistArray[i] = 0;
	    return;
	}
    }

    // out of space - realloc
    maxUserData += growBy;
    growBy *= 1.5;
    userDataArray = (void**) realloc(userDataArray, 
				     maxUserData * sizeof(void*));
    userDataIndexArray = (int*) realloc(userDataIndexArray, 
				       maxUserData * sizeof(int*));
    userEndExistArray = (char*) realloc(userEndExistArray, 
				       maxUserData * sizeof(char*));

    // set up fields in first space available (old max user data)
    userDataIndexArray[i] = reqnum;
    userDataArray[i] = p;
    userEndExistArray[i] = 0;

    // -1 out new items
    for (++i; i<maxUserData; i++) {
	userDataIndexArray[i] = -1;
    }
}

void FAMFreeUserData(int reqnum)
{
    for (int i=0; i<maxUserData; i++) {
	if (userDataIndexArray[i] == reqnum) {
	    userDataIndexArray[i] = -1;
	    return;
	}
    }
    return;
}
