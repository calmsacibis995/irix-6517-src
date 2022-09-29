/* 	$Modified: Thu Apr 10 17:05:45 1997 by cwilson $ */

static char rcsversion[] = "$Revision: 2.11 $"; 
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h> 
#include <string.h>
#include <stdlib.h>
#define _BSD_SIGNALS
#include <signal.h>
#include <search.h>
#include <rpcsvc/ypclnt.h>

#define SERV_TCP_PORT 79
#define MAXLINE 512
#define LINE_SIZE	128

#define err_sys(X) fprintf(stderr, X);
#define WHO "\n"

#define MAX_HOSTS	500			/* number of potential machines hooked up to annex boxes */
#define MAX_KEYS	900			/* number of keys that the ktools map can have, for use with -noyp flag */
#define CONFIG_FILE	"/usr/annex/ktools.map"
#define DELAY 2
#define TRUE 1
#define FALSE 0
#define YPMAP "ktools"

#define TABLESIZE 50
#define ELSIZE    20

int  Debug = 0;
int  tablecounter = 0;
char addressTable[TABLESIZE][ELSIZE];

/* The hosts table has 75 characters now. It's an important number
   for qsort()*/
char Hosts[MAX_HOSTS][75];				
int  Hostcounter = 0;

/* this section used with hsearch(), when -noyp flag is given */
char	key_space[MAX_KEYS*30];
char	value_space[MAX_KEYS][MAX_KEYS];
char	*key_ptr = key_space;
char	*value_ptr = value_space[0];
int	key_counter = 0;
ENTRY item, *found_item;

char *ypdomain = NULL;
char default_domain_name[64];

char	*commandlineclass;
char	*theirclass;
char	classes[10][20];			/* user can belong to 10 different classes */

char	*machine = NULL;			/* lookup an individual machine */

struct 	ktools_classes {
  char	*name;
  char	*description;
  char	*owner;
  char	*owner_uname;
  char	*owner_ext;
  char	*annexes[20];
} Class[20];
int	classcounter = 0;

int	NoYP;

/* ----------------------------------------------------------------------
 * m_strcpy:
 * malloc space for string, then strcpy into it.
 */
void m_strcpy(char *dest, char *source) {
  int temp;

  temp = strlen(source);
  if ( (dest = (char *)malloc(temp)) == NULL) {
    fprintf(stderr, "kwho: m_strcpy: unable to malloc space for string.\n");
    exit(1);
  }
  
  strcpy(dest, source);
}




void PrintClasses() {
  int	n;

  printf ("\tClass name    Class description     Class owner, email, extension\n");
  printf ("\t----------    -----------------     -----------------------------\n");

  for (n = 0; n < classcounter; n++) {
    printf("\t%-10s   %-17s   %s,%s,%s\n", Class[n].name, Class[n].description,
	   Class[n].owner, Class[n].owner_uname, Class[n].owner_ext);
  }

}

/* -----------------------------------------------------------------------
 * 
 */
void slurpclasses() {
    int 	err;
    char 	*val;
    int	len;
    char	*scratch;
    char	class[30];
    char  *temp;
    int	a, b;

    err = ypmatch("classes", &val, &len);
    if (err) {
	fprintf(stderr, "kwho: unable to locate classes in ktools NIS map.\n");
	exit(1);
    }

    if ((scratch = strtok(val, "`")) == NULL) {
	fprintf(stderr, "Hmm, nothing in the ktools map under the classes key..\n");
	exit(1);
    }
    
    /* extract class names, put in struct */
    while (scratch) {
	Class[classcounter++].name = scratch;
	scratch = strtok(NULL, "`");
    }
  
    /* remove trailing newline from last entry */
    /*Class[classcounter].name[strlen(Class[--classcounter]) - 1] = NULL; */
    Class[classcounter].name[strlen(Class[--classcounter].name) -1] = NULL;

    /* null out the last name pointer as end of table marker */
    Class[classcounter+1].name = NULL;

    /* cycle through all classes, retrieving the def from the yp map. */
    for (a = 0; Class[a].name; a++) {
	strcpy(class, Class[a].name);
	strcat(class, "_config");
	err = ypmatch(class, &val, &len);
	if (err) {
	    fprintf(stderr, "kwho: No class definition for %s\n", Class[a].name);
	    exit(1);
	}
    
	if ((scratch = strtok(val, "`")) == NULL) {
	    fprintf(stderr, "kwho: Empty class definition for %s\n", Class[a].name);
	    exit(1);
	}
	Class[a].description = scratch;

	if ((scratch = strtok(NULL, "`")) == NULL) {
	    fprintf(stderr, "Eeek. Incomplete class definition in map.\n");
	    exit(1);
	}
	Class[a].owner = scratch;
    
	if ((scratch = strtok(NULL, "`")) == NULL) {
	    fprintf(stderr, "Eeek. Incomplete class definition in map.\n");
	    exit(1);
	}
	Class[a].owner_uname = scratch;

	if ((scratch = strtok(NULL, "`")) == NULL) {
	    fprintf(stderr, "Eeek. Incomplete class definition in map.\n");
	    exit(1);
	}
	Class[a].owner_ext = scratch;
	
	if ((scratch = strtok(NULL, "`")) == NULL) {
	    fprintf(stderr, "Eeek. Incomplete class definition in map.\n");
	    exit(1);
	}
	
	b = 0;
	if (strchr(scratch, ',')) {		/* multiple annexes */
	    scratch = strtok(scratch, ",");
	    while (scratch) {
		Class[a].annexes[b++] =  scratch;
		scratch = strtok(NULL, ",");
	    }
	} else {
	    Class[a].annexes[b++] = scratch;
	}
	Class[a].annexes[b][ strlen( Class[a].annexes[--b] ) - 1] = NULL; /* nuke trailing newline */
    } 
}


int writen(int fd, char *ptr, int nbytes) {
  int nleft, nwritten;
  
  nleft = nbytes;
  while (nleft > 0) {
    nwritten = write(fd, ptr, nleft);
    if (nwritten <= 0)
      return(nwritten);
    nleft -= nwritten;
    ptr += nwritten;
  }
  return (nbytes-nleft);
}


int readline(int fd, char *ptr, int maxlen) {
  int n, rc;
  char c;
  
  for (n = 1; n < maxlen; n++) {
    if ( (rc = read(fd, &c, 1)) == 1) {
      *ptr++ = c;
      if (c == '\n') 
	break;
    } else if (rc == 0) {
      if (n == 1)
	return (0);				/* EOF, no data read */
      else 
	break;					/* EOF, some data was read */
    } else 
      return (-1);				/* error */
  }
  
  *ptr = 0;
  return(n);
}


void doclasses() {
  char class[30];
  char ypclass[30];
  char *val;
  char *scratch;
  int len;
  int error;
  int n = 0;
  int z = 0;
  int a = 0;

  if (commandlineclass != NULL)
    theirclass = commandlineclass;
  else if ((theirclass = getenv("KTOOLS_CLASS")) == NULL) 
    classes[0][0] = NULL;				/* all classes */

  if (theirclass != NULL) {
    /* multiple classes? */
    if (strchr(theirclass, ':')) {
      strcpy(class, theirclass);
      scratch = strtok(class, ":");
      strcpy(classes[n++], scratch);
      while (scratch = strtok(NULL, ":")) {
	strcpy(classes[n++], scratch);
      }
    } else 
      strcpy(classes[n++], theirclass);
    
    /* verify classes */
    classes[n][0] = NULL;			
    n = 0;
    while (classes[n][0]) {
      a = 0;
      z = 0;
      while (Class[z].name) {
	if ( strcmp(Class[z++].name, classes[n] ) ) 
	  a++;
      }    
      if (a == z) {
	fprintf(stderr, "kwho: invalid or unknown class of machines: %s.\n", classes[n]);
	fprintf(stderr, "      (check your KTOOLS_CLASS environment variable)\n");
	fprintf(stderr, "Valid classes are:\n");
	PrintClasses();
	exit(1);
      }
      n++;
    }
  }
}


/* 
 * checkversion
 * 
 * check the compiled-in rcsversion string w/ the current NIS one.
 * if different, tell the user so.
 *
 */
void checkversion() {
  int 	error;
  char 	*val;
  char  *scratch;
  int 	len;
  char	foo[20];
  float	version;
  float nis_version;

  sscanf( rcsversion, "%s %f", foo, &version);

  error = ypmatch("kwho_version", &val, &len);
  if (error) {
    fprintf(stderr, "\nkwho: unable to find kwho_version in ktools NIS map.\n");
    return;
  }
  
  sscanf( val, "%f", &nis_version);

  if (nis_version > version) {
    if ( (nis_version - version) > 1) {
      fprintf(stderr, "\nNOTICE: A newer version of kwho is currently being used.  The data format used\n");
      fprintf(stderr, "        for the new version of kwho has made this version incompatible.  Please\n");
      fprintf(stderr, "        update the ktools software to restore functionality.\n");
      fprintf(stderr, "        Images can be found at dist.engr:/sgi/hacks/ktools.\n");
      exit (1);
    } else {
      fprintf(stderr, "\n NOTICE: A newer version of kwho is available.  Please upgrade at your\n");
      fprintf(stderr,    "        earliest convenience for added functionality and bug fixes.\n");
      fprintf(stderr, "        Images can be found at dist.engr:/sgi/hacks/ktools.\n");
    }
  }
}

  
void get_ktools_config() {
  FILE	*config_file;
  char 	string[900];

  if ((config_file = fopen(CONFIG_FILE, "r")) == NULL) {
    perror(CONFIG_FILE);
    fprintf(stderr, "Sorry, you can't use the ktools with the -noyp option until you copy\n\
the ktools configuration file, ktools.map, to your system.\n");
    exit(1);
  }

  while (fgets(string, sizeof(string), config_file) != NULL) {
    if (strlen(string) > 1 && (strchr(string, '\t')) != NULL) {
      sscanf( string, "%s", key_ptr );
      item.key = key_ptr; 
      strcpy( value_ptr, strchr(string, '\t') + 1);
      item.data = (void *)value_ptr;
      key_ptr += strlen(key_ptr) + 1;
      value_ptr = value_space[++key_counter];
      (void) hsearch(item, ENTER);
    }
  }
}

  
void handlealarm() {
  if (Debug)
    fprintf(stderr, "* sigalrm * \n");
}

void doit() { 
  static int counter= -1;
  int 	a, b, counter2;
  
  if (Debug)
    fprintf(stderr, "doit()\n");

  if (machine) {				/* just see if a single machine is active */
    int 	err;
    char 	*val;
    int		len;
    char	scratch[40];

    err = ypmatch(machine, &val, &len);
    if (err) {
      fprintf(stderr, "kwho: unable to locate %s in ktools NIS map.\n", machine);
      exit(1);
    }

    /* annex is field 15 */
    a = 0;
    while (val && a != 15) {
      if (*val++ == '`')
	a++;
    }

    a = 0;
    while (*val != '`') {
      scratch[a++] = *val++;
    }
    scratch[a] = NULL;

    annexwho( scratch );

  } else {
    while (Class[++counter].name) {
      if (classes[0][0]) {				/* KTOOLS_CLASS defined */
	a = 0;
	b = 0;
	while (classes[a][0]) {
	  if ( strcmp( classes[a], Class[counter].name ) )
	    b++;
	  a++;
	}
	if ( a == b ) {				/* no matches */
	  continue;
/*	  fprintf(stderr, "Hey, you shouldn't see this.\n");
	  exit(1);*/
	}
      }
      
      counter2 = -1;
      while (Class[counter].annexes[++counter2]) {
	annexwho( Class[counter].annexes[counter2] );
      }
    } /* while */
  } /* else */
} /* doit */


int annexwho (char *annex) {
    int n;
    char recvline[MAXLINE + 1];

    char port[2], user[20],when[20], idle[20], from[20], hostname[30], domain[10];
    char temp1[20];
    char temp2[10];
    static char  loc[30];
    unsigned long nethost;
    struct hostent *host;				/* for ip->ascii resolve */
    char foo[30];
    char *tableptr;

    struct hostent  *hostptr;
    struct sockaddr_in	serv_addr;
    int	sockfd;
    int	error;
    char annex2[50];


    /* the magic . at the end of the annex name greatly speeds gethostbyname, 
       since the magic . sez that it's a FQDN
       */ 
    strcpy(annex2, annex);
    strcat(annex2, ".");

    strcpy(loc, "");

    if (Debug) 
      fprintf(stderr, "gethostbyname(%s)\n", annex2);
    
    if (! (hostptr = gethostbyname(annex2))) { 
	if (Debug)
	  fprintf(stderr, "!!! Unable to get the ip addr of %s, skipping. !!!\n", annex2);
	return (1);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family	= AF_INET;
    
    bcopy(hostptr->h_addr, (char *)&serv_addr.sin_addr.s_addr, hostptr->h_length);
    serv_addr.sin_port 	= htons(SERV_TCP_PORT);
    
    /* open tcp socket */
    if (Debug)
      fprintf(stderr, "socket()\n");
    if ( (sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0 ) {
	if (Debug)
	  fprintf(stderr, "!!! Can't open stream socket to annex %s, skipping. !!!\n", annex2);
    }
  
    /* set alarm for quick timeout */
    if (Debug)
      fprintf(stderr, "alarm(%d)\n", DELAY);
    alarm(DELAY);
    /* connect to server */
    if (Debug)
      fprintf(stderr, "connect()\n");
    if ((error = connect(sockfd, (struct sockaddr *) &serv_addr,
			 sizeof(serv_addr))) < 0) {
	if (Debug)
	  fprintf(stderr, "!!! Can't connect to annex %s, errno %d, skipping. !!!\n", annex2, error);
	return (1);
    }
    /* the 'who' from the annex shouldn't take more than 6 seconds */
    alarm(6);
    
    if (writen(sockfd, WHO, sizeof(WHO)) != sizeof(WHO))  {
	if (Debug)
	  fprintf(stderr, "!!! Unable to write to socket !!!\n");
    }
      
    /* here we're ready to read the who output from the annex box.
       throw away all lines that we don't want (recvline[5] != P)
       and split the line into fields cuz we rearrange them into our
       format.

       note that annex code rev 10 inserted a gratitous space so we
       have to check recvline[6] as well.
       */
    while ( (n = readline(sockfd, recvline, MAXLINE)) > 0 && recvline[1] != 'v' ) {
	if (recvline[5] != 'P' && recvline[6] != 'P')
	  continue;
	recvline[n] = 0;
	
	sscanf(recvline, "%s%s%s%s%s", port, foo, user, loc, when);
	
	/* Connect time can be over 24 hours, in which case the day of week is
	   prepended, or over 7 days, in which case the month and day are also prepended.
	   */
	if (isalpha(when[0])) {			/* Sat, Sun, Jan, Feb, etc */
	    sscanf(recvline+47, "%s", foo);
	    strcat(when, " ");
	    strcat(when, foo);
	    
	    if (strlen(when) == 6) {			/* Jan, Feb, Mar, etc */
		sscanf(recvline+51, "%s", foo);
		strcat(when, " ");
		strcat(when, foo);
	    }
	}
	
	strncpy(idle, recvline+56, 7);
	idle[ (idle[6] == ' ' ? 6 : 7) ] = NULL;
	sscanf(recvline+ (recvline[64] == ' ' ? 65 : 64), "%s", from);
	
#if 0
	if ( (tableptr = (char *) lfind(from, (char *)addressTable, &tablecounter, ELSIZE, mystrcmp)) == NULL) {
#endif
	    nethost = inet_network(from);
	    host = gethostbyaddr( &nethost , sizeof(nethost), AF_INET );
	    if (host) {
		sscanf(host->h_name, "%[0-9A-z-].%[0-9A-z]", hostname, domain);
		if ( (sscanf(hostname, "%[0-9A-z]-%[0-9A-z]", foo, temp1)) == 2)
		  strcpy ( hostname, temp1) ;
		strcat(hostname, ".");
		strcat(hostname, domain);
	    } else {					/* couldn't resolve ip addr */
		strcpy(hostname, from);
	    }
#if 0
	    strcpy(foo, from);
	    strcat(foo, "\t");
	    strcat(foo, hostname);
	    lsearch(foo, (char *)addressTable, &tablecounter, ELSIZE, mystrcmp);
	    tablecounter++;
	}
	else {
	    fprintf(stderr, "*");
	    sscanf(tableptr, "%s\t%s", foo, hostname);
	}
#endif 
	sprintf(Hosts[Hostcounter++], "%-18s %-17s %-13s %-14s %-8s",
		loc, hostname, user, when, idle); 
    }
  
    close(sockfd);
    return 0;
} /* while */


int file_match(char *key, char **val) {
  
  item.key = key;
  if ((found_item = hsearch(item, FIND)) == NULL) {
    if (Debug)
      (void) fprintf(stderr,
		   "Can't find key %s in %s.\n", key, CONFIG_FILE);

    return(1);
  }
  *val = found_item->data;
  return(0);
}

ypmatch(char *key, char **val, int *len) {
  int err;
  int error = FALSE;

  *val = NULL;
  *len = 0;
  err = NoYP ? file_match(key, val) :
    yp_match(ypdomain, YPMAP, key, strlen(key), val, len);

  if (err == YPERR_KEY) {
    err = yp_match(ypdomain, YPMAP, key, (strlen(key) + 1),
		   val, len);
  }
  
  if (err) {
/*
    (void) fprintf(stderr,
		   "Can't match key %s in map %s.  Reason: %s.\n", key, YPMAP,
		   yperr_string(err) );
*/
    error = TRUE;
  }
  return (error);
}


/*
 * main
 */
main(int argc, char *argv[]) {
    int	n, header = 0;
    char line[LINE_SIZE];
    unsigned long	hostip;

    commandlineclass = NULL;

    argv++;

    while (--argc) {
    
	if ( (*argv)[0] == '-') {
	    /* check for -noyp flag */
	    if ( strcmp(argv[0], "-noyp") == 0) {
		NoYP = 1;
		hcreate(MAX_KEYS);
	    }
      
	    /* check for debug flag */
	    if ( strcmp(argv[0], "-d") == 0) {
		Debug = 1;
	    }
      
	    /* check for class specified on command line. */
	    if ( strcmp(argv[0], "-c") == 0 ) {
		commandlineclass = argv[1];
	
		if (commandlineclass == NULL) {
		    fprintf(stderr, "kwho: Need a class when -c flag specified.\n");
		    exit(1);
		}
	    }
	    argv++;
	} else {
	    if (!machine) {
		machine = *argv;
		if (Debug) 
		  fprintf(stderr, "Machine == %s\n", machine);
	    } else {
		fprintf(stderr, "kwho: Too many machines specified.\n");
		exit(1);
	    }
	}
    }
    
    if (Debug) 
      fprintf(stderr, "signal(SIGALRM)\n");
    signal(SIGALRM, handlealarm);
    
    if (NoYP) {
	get_ktools_config();
    } else {
	
	if (!getdomainname(default_domain_name, 64) ) {
	    ypdomain = default_domain_name;
	} else {
	    (void) fprintf(stderr, "kwho: invalid domainname.\n");
	    exit(1);
	}
	
	if (strlen(ypdomain) == 0) {
	    (void) fprintf(stderr, "kwho: invalid domainname.\n");
	    exit(1);
	}

	/* check rcs version with current one */
	checkversion();
    }

    slurpclasses();
    doclasses();
    doit();
    
    if (!machine) {				/* print header */
	if (commandlineclass != NULL) 
	  printf ("           class = %s\n", theirclass);
	else if (theirclass != NULL) 
	  printf ("           KTOOLS_CLASS = %s\n", theirclass);
    
	printf ("\
Mach Name          Connected to        User        Connect Time  Idle Time\n\
---------          ------------      ----------    ------------  ---------\n\
");
	header++;
    }

    /* sort it */
    qsort(Hosts, Hostcounter, strlen(Hosts[0]) + 1,  *strcmp);  
    
    /* print it */
    for (n = 0; n < Hostcounter; n++) {
	if (Hosts[n][0] == '\0')
	  continue;
	if (machine && (strncmp(machine, Hosts[n], strlen(machine)) == 0)) {
	    if (!header) {				/* only print header if there's a match */
		if (theirclass != NULL) printf ("           KTOOLS_CLASS = %s\n", theirclass);
		printf ("\
Mach Name          Connected to        User        Connect Time  Idle Time\n\
---------          ------------      ----------    ------------  ---------\n\
");
		header++;
	    }
	    printf("%s\n", Hosts[n]);
	    
	} else if (!machine) 
	  printf("%s\n", Hosts[n]);
    }
    
}
