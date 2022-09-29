/* Error messages. */

char *err[] = {  

/* Errors in command line, input file, or menu choice */
"usage: irixbtest [ -e elogfile ] [ -s string[ string] ]\n\
                                 [ -f inputfile ]\n\
                                 [ -m ]",		/* CMDLINE */    
"-s must be the last option on the command line.",	/* S-OPTION   */     
"Must give one of the options: -m, -f, -s.",		/* MISSINGOPT */    
"Test not supported:",					/* TESTNOTSUP */    
"Incorrect choice:",					/* MENUCHOICE */

/* Other fatal errors in driver */
"Must be superuser to execute.",			/* UIDNOTSU   */  
"Cannot get a time string for file names.",		/* TIMESTRING */
"Cannot get current directory label.",			/* GETCDLABEL */
"Current directory must be msenlow(or equal)/minthigh(or equal)",
							/* BADCDLABEL */
"Cannot stat current directory.",			/* STATDIR */
"Current directory mode must allow search and write by all.", 
							/* BADCDMODE */
"Stat mode is invalid",					/* BADSTATMO */
"Bad arg passed to queue_multi:",			/* BADQMULTI */

/* Generic errors */
"Cannot open file:",					/* FILEOPEN */       
"Error in function:",					/* FUNCTION */   
"Cannot create file:",					/* F_CREAT */
"Cannot mkdir:",					/* F_MKDIR */
"Cannot write to file:",				/* F_WRITE */
"Cannot get message queue:",				/* F_MSGGET */

/* Label errors */   
"Cannot set process label:",				/* F_SETPLABEL */       
"Cannot create process label:",				/* F_CREPLABEL */       
"Cannot set dir label:",				/* SETLABEL_DIR */
"Cannot set file label:",				/* SETLABEL_FILE */
"Label retrieved does not match object label.",		/* BADLABEL */

/* Capability errors: */   
"Cannot set process capabilities set:",			/* CAPSETPROC */
"Cannot set access control list",			/* CAPSETACL */
"Cannot acquire the needed capabilities.\n",		/* CAPACQUIRE */

/* Child processes */
"Fork call failed:",					/* F_FORK */
"Wait failed:",						/* F_WAIT */
"Child not in stopped state.",				/* C_NOTSTOP */
"Child termination not due to exit call.",		/* C_NOTEXIT */
"Child's exit value was not zero.",			/* C_BADEXIT */
"Cannot kill child process:",				/* C_NOTKILLED */

/* Pipes */
"Pipe call failed:",					/* F_PIPE */
"Read from pipe failed:",				/* PIPEREAD */
"Read from pipe by Object failed:",			/* PIPEREAD_O */
"Read from pipe by Subject failed:",			/* PIPEREAD_S */
"Write to pipe failed:",				/* PIPEWRITE */
"Write to pipe by Object failed:",			/* PIPEWRITE_O */
"Write to pipe by Subject failed:",			/* PIPEWRITE_S */

/* UIDs */
"Setreuid in Object failed:",				/* SETREUID_O */
"Setreuid in Subject failed:",				/* SETREUID_S */
"Setuid in Subject failed:",				/* SETUID_S */
"Setuid in Object failed:",				/* SETUID_O */
"Subject uids not set correctly.",			/* BADUIDS_S */
"Object uids not set correctly.",			/* BADUIDS_O */
"Child has uid or gid from execed file.",		/* SUID_SGID */

/* GIDs */
"Setregid in Object failed:",				/* SETREGID_O */
"Setregid in Subject failed:",				/* SETREGID_S */
"Subject gids not set correctly.",			/* BADGIDS_S */
"Object gids not set correctly.",			/* BADGIDS_O */

/* PGIDs and SIDs */
"PGID not set correctly.",				/* BADPGID */
"BSDsetpgrp failed:",					/* F_BSDSETPGRP */
"setsid failed:",					/* F_SETSID */

/* Test call error */
"Test call fails unexpectedly:",			/* TESTCALL */
"Wrong error on test call:",				/* TEST_ERRNO */
"Test call unexpectedly returns:",			/* TEST_RETVAL */
"Test call unexpectedly succeeds:",			/* TEST_UNEXSUCC */

/* Socket stuff */
"Socket failed:",					/* F_SOCKET */
"Bind failed:",						/* F_BIND */
"Getsockname failed:",					/* F_GETSOCKNAME */
"Listen failed:",					/* F_LISTEN */
"Accept failed:",					/* F_ACCEPT */
"Connect failed:",					/* F_CONNECT */
"Write to socket failed:",				/* F_WRITES0 */
"Read from socket failed:",				/* F_READSO */
"Sendto failed:",					/* F_SENDTO */
"Recvfrom failed:",					/* F_RECVFROM */
"Recv failed:",						/* F_RECV */
"Recvlfrom failed:",					/* F_RECVLFROM */
"Recvl failed:",					/* F_RECVL */
"Select failed:",					/* F_SELECT */ 
"Select timed out.",					/* T_SELECT */ 
"Select unexpectedly detected a descriptor.",		/* U_SELECT */ 
"Ioctl failed:",					/* F_IOCTL */
"Ioctl(. . SIOGETLABEL . .) failed:",			/* F_IOCTLGETLBL */
"Ioctl(. . SIOSETLABEL . .) failed:",			/* F_IOCTLSETLBL */
"System(iflabel . . .) call failed:",			/* F_SYSIFLABEL */
"Socket label does not match label set on process.",	/* BADSOCLABEL */
"Cannot getlabel on UNIX socket:",			/* F_GETLABEL_S */
"Ioctl(. . SIOSETACL . .) failed:",			/* F_IOCTLSETACL */
"SETPSOACL failed:",					/* F_SETPSOACL */
"Ioctl(. . SIOGETACL . .) failed:",			/* F_IOCTLGETACL */
"Ioctl(. . SIOGETUID . .) failed:",			/* F_IOCTLGETUID */
"Socket uid does not match expected value.",		/* BADSOCUID */
"Socket ACL count does not match psoacl count.",	/* BADACLCOUNT */
"Socket ACL not match psoacl.",				/* BADACLVAL */
"Ioctl(. . SIOGIFUID . .) failed:",			/* F_IOCTLGIFUID */
"Ioctl(. . SIOGIFLABEL . .) failed:",			/* F_IOCTLGIFLABEL */
"Interface uid does not match expected value.",		/* BADIFUID */
"Interface label does not match expected value.",	/* BADIFLABEL */

/* Miscellaneous */
"Exec failed:",						/* F_EXEC */
"Ublock values incorrect.",				/* BADRDUBLK */
"Expected name not returned.",				/* BADRDNAME */
"Bad proc size returned.",				/* BADPROCSZ */
"Values written and read by ptrace() do not match.",	/* BADPTRACE */
"RDUBLK failed:",					/* F_RDUBLK */
"Chmod failed:",					/* F_CHMOD */
"Chown failed:",					/* F_CHOWN */
"Symlink failed:",					/* F_SYMLINK */
"Process not blocked:",					/* NOTBLOCKED */

/* Object reuse */
"Stat failed",						/* F_STAT */
"Initial file size not zero.",				/* BADFSIZE */
"Brk failed",						/* F_BRK */
"Lseek failed",						/* F_LSEEK */
"Read failed",						/* F_READ */
"Byte read from empty file is non-null",		/* BADBYTE_EMPTY */
"Unaccessed byte before EOF is non-null",		/* BADBYTE_BEFORE */
"Byte read beyond EOF is non-null",			/* BADBYTE_BEYOND */
"String read doesn't match string written",		/* BADSTRING */
"Opendir failed",					/* F_OPENDIR */
"Readdir failed",					/* F_READDIR */
"Initial directory size > 512.",			/* BADDIRSIZE */
"Read unexpected file name.",				/* BADFILE */
"Mknod failed.",					/* F_MKNOD */
"Fcntl failed.",					/* F_FCNTL */
"Readlink failed.",					/* F_READLINK */
"Unexpected symlink contents.",				/* BADSYM */
"Symlink contents larger than file name.",		/* BADLINKSIZE */
"Msgrcv failed.",					/* F_MSGRCV */
"Message unexpectedly detected.",			/* BADMSGRCV */
"Chdir failed.",					/* F_CHDIR */
"Sproc failed.",					/* F_SPROC */
"Semget failed.",					/* F_SEMGET */
"Semop failed.",					/* F_SEMOP */
"Shmget failed.",					/* F_SHMGET */
"Shmat failed.",					/* F_SHMAT */
"Socketpair failed:",					/* F_SOCKETPAIR */
"Read call unexpectedly succeeded on directory.",	/* READ_DIR_OK */
"Rename failed",					/* F_RENAME */
"Cannot run in enhanced superuser mode.\n",		/* NOESUSER */
};
