#ifndef _FAM_
#define _FAM_
#ifdef __cplusplus
extern "C" {
#endif


/*****************************************************************************
*
*  MODULE:  Fam.h - File Alteration Monitor 
*
*   Fam allows applications to monitor files - receiving events when they
*   change.  The scheme is simple:  Applications register which
*   files/directories they are interested, and fam will notify the
*   application when any of these files are changed.
*
*****************************************************************************/

/* For NAME_MAX - maximum # of chars in a filename */
#include "limits.h"





/*****************************************************************************
*  DATA STRUCTURES
*
*
*  FAMConnection Structure:
*
*  The FAMConnection Data structure is created when opening a connection
*  to fam.  After that, it is passed into all fam procedures.  This
*  structure has all the information in it to talk to fam (Currently,
*  the only data inside this structure is a file descriptor for
*  the socket to fam).  
*****************************************************************************/

typedef struct FAMConnection {
    int fd;
    void *client;
} FAMConnection;


/*****************************************************************************
*  To access the fd inside this structure (so the application writer can
*  use select for fam events), the following macro is defined (this will 
*  allow us to change the implementation without users having to change 
*  their code):
*****************************************************************************/
#define FAMCONNECTION_GETFD(fc)      ((fc)->fd)





/*****************************************************************************
*  FAMRequest Structure:
*
*  Every time fam is called on to monitor a file, it passes back a
*  FAMRequest structure.  The main piece of data that this structure 
*  contans is a reqnum.  This reqnum is guaranteed to be unique.  When
*  cancelling or suspending a monitor request, you must pass this
*  data structure into FAMCancelMonitor or FAMSuspendMonitor to
*  make sure that you are cancelling/suspending the correct request
*  (You can monitor the same file/directory more than once).
*****************************************************************************/

typedef struct FAMRequest {
    int reqnum;
} FAMRequest;


/*****************************************************************************
*  Once again, to access the reqnum inside this structure, use the
*  following macro:
*****************************************************************************/
#define FAMREQUEST_GETREQNUM(fc)     ((fc)->reqnum)



/*****************************************************************************
*  FAMEvent Structure:
*
*  When a file/directory has been modified/deleted, fam will pass
*  a FAMEvent structure to the application (through the callback
*  in the FAMMonitorFile/Directory routines).  This structure will
*  describe what event happened to the file.  The different events
*  that can happen to a file are listed in the FAMCodes enum.
*****************************************************************************/
enum FAMCodes { FAMChanged=1, FAMDeleted=2, FAMStartExecuting=3, 
		FAMStopExecuting=4, FAMCreated=5, FAMMoved=6, 
		FAMAcknowledge=7, FAMExists=8, FAMEndExist=9 };

typedef struct  FAMEvent {
    FAMConnection* fc;         /* The fam connection that event occurred on */
    FAMRequest fr;             /* Corresponds to the FamRequest from monitor */
    char *hostname;            /* host and filename - pointer to which */
    char filename[PATH_MAX];   /* file changed */
    void *userdata;            /* userdata associated with this monitor req. */
    enum FAMCodes code;        /* What happened to file - see above */
} FAMEvent;





/*****************************************************************************
*  ERROR HANDLING
*
*  If an error occurs inside of libfam, a global named FAMErrno is set
*  to a non-zero value and the routine that the error occurred in will 
*  return an error value (usually NULL).  FAMErrlist is a global
*  string array (indexed by FAMErrno) that describes the last error 
*  that happened;
*
*  NOTE: currently FAMErrno and FamErrList are unused
*****************************************************************************/

extern int FAMErrno;
extern char *FamErrlist[];

/*
*[NOTE: Eventually, there will be a bunch of defines right here defining what
* errors can happen in using libfam ]
*/






/*****************************************************************************
*  PROCEDURES:
*
*  FAMOpen, FAMClose
*
*  The first step that an application has to do is open a connection to
*  fam.  This is done through the FAMOpen.  FAMOpen returns a FAMConnection
*  data structure which is passed to all fam procedures.  FAMClose closes
*  a fam connection.  
*
*  On error, FAMOpen will return NULL and FAMClose will return -1 (and
*  FAMErrno will be set to the value of the error).  
*****************************************************************************/

extern int FAMOpen(FAMConnection* fc);
extern int FAMOpen2(FAMConnection* fc, const char* appName);
extern int FAMClose(FAMConnection* fc);



/*****************************************************************************
*  FAMMonitorDirectory, FAMMonitorFile
*
*  These routines tell fam to start monitoring a file/directory.  The
*  parameters to this function are a FAMConnection (received from FAMOpen),
*  a filename and a user data ptr.  A FAMRequest structure is returned.
*  When the file/directory changes, a fam event structure will be 
*  generated.  The application can retrieve this structure by calling
*  FAMNextEvent (see description under FAMNextEvent).
*
*  The difference between these two routines is that FAMMonitorDirectory
*  monitors any changes that happens to the contents of the directory
*  (as well as the directory file itself) and FAMMonitorFile monitors
*  only what happens to a particular file.
*
*  On error FAMMonitorDirectory/File will return NULL (and FAMErrno will
*  be set to the value of the error).  
*****************************************************************************/

extern int FAMMonitorDirectory(FAMConnection *fc,
			       const char *filename,
			       FAMRequest* fr,
			       void* userData);
extern int FAMMonitorFile(FAMConnection *fc,
			  const char *filename, 
			  FAMRequest* fr,
			  void* userData);
extern int FAMMonitorCollection(FAMConnection *fc,
				const char *filename, 
				FAMRequest* fr,
				void* userData,
				int depth,
				const char* mask);

extern int FAMMonitorDirectory2(FAMConnection *fc,
				const char *filename,
				FAMRequest* fr);
extern int FAMMonitorFile2(FAMConnection *fc,
			   const char *filename,
			   FAMRequest* fr);


/*****************************************************************************
*  FAMMonitorRemoteDirectory, FAMMonitorRemoteFile
*
*  These routines do the same as FAMMonitorDirectory and FAMMonitorFile
*  except they will do it on any machine in the network [NOTE: depending
*  on the capability of fam - which is another topic].
*****************************************************************************/

extern int FAMMonitorRemoteDirectory(FAMConnection *fc, 
				     const char *hostname,
				     const char *filename,
				     FAMRequest* fr,
				     void* userdata);
extern int FAMMonitorRemoteFile(FAMConnection *fc,
				const char *hostname, 
				const char *filename,
				FAMRequest* fr, 
				void* userdata);





/*****************************************************************************
*  FAMSuspendMonitor, FAMResumeMonitor
*
*  FAMSuspendMonitor enables applications to suspend
*  receive events about files/directories that are changing.  This can
*  be useful in situations when an application is stowed and does not
*  want to receive events until it is unstowed.  FAMResumeMonitor
*  signals fam to start monitoring the file/directory again.  
*
*  Both of these routines take a FAMConnection and a FAMRequest structure.
*  The FAMRequest Structure is returned from the FAMMonitorFile/Directory
*  routines.
*
*  On error, FAMResume/SuspendMonitor will return -1 (and the global
*  FAMErrno will be set to the value of the error).
*****************************************************************************/

int FAMSuspendMonitor(FAMConnection *fc, const FAMRequest *fr);
int FAMResumeMonitor(FAMConnection *fc, const FAMRequest *fr);






/*****************************************************************************
*  FAMCancelMonitor
*
*  When an application is done monitoring a file/directory, it should
*  call FAMCancelMonitor.  This routine will signal fam not to watch
*  this directory anymore. Once again, the FAMRequest structure is
*  returned from the FAMMonitorFile/Directory routines.
*
*  On error, FAMCancelMonitor will return -1 (and the global
*  FAMErrno will be set to the value of the error).  This routine will free
*  the FAMRequest structure that is passed in.
*****************************************************************************/

int FAMCancelMonitor(FAMConnection *fc, const FAMRequest *fr);




/*****************************************************************************
*  FAMNextEvent, FAMPending
*  
*  FAMNextEvent will get the next fam event (file/directory change).  If 
*  there are no fam events waiting, then FAMNextEvent will wait 
*  until a fam event has been received (from fam).
*
*  FAMPending will return the number of fam events that are waiting.
*  This routine always returns immediately to the caller.
*
*  Essentially, there are two ways to for applications to receive fam
*  events:
*
*  1.  The "Polling" approach - Just call FAMPending periodically;
*      like when the system is waiting for input.  If FAMPending returns
*      with a positive return value, then there are fam events waiting.
*      Call FAMNextEvent to receive the events.
*
*  2.  The "Select" approach - Use the fd returned from famOpen (in the
*      FAMConnection structure) as one of the fds that the application
*      selects on.  When select returns saying that data is on the fam
*      fd, call FAMNextEvent.
*
*  FAMNextEvent reads any information that is on the fam socket,
*  and returns it to the application (in the form of a FAMEvent).
*
*  On error, FAMNextEvent and FAMPendingEvent will return -1 (and the global
*  FAMErrno will be set to the value of the error).
*****************************************************************************/

int FAMNextEvent(FAMConnection *fc, FAMEvent *fe);
int FAMPending(FAMConnection* fc);




/*****************************************************************************
*  FAMDebugLevel
*  
*  Sets the fam debugging level.  Currently, there are three levels:
*
*      FAM_DEBUG_OFF      -  no debugging information output - default
*      FAM_DEBUG_ON       -  fam outputs various debugging information
*      FAM_DEBUG_VERBOSE  -  even more information than FAM_DEBUG_ON
*
*  All output is written to the syslog.
*
*****************************************************************************/

#define FAM_DEBUG_OFF 0
#define FAM_DEBUG_ON  1
#define FAM_DEBUG_VERBOSE 2

int FAMDebugLevel(FAMConnection *fc, int debugLevel);

#ifdef __cplusplus
}
#endif
#endif  /* _FAM_ */
