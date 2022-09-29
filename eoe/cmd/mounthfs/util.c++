// Simple utility program.

#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include "ancillary.h"
#include <sys/stat.h>
#include <stdlib.h>
#include <stream.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>

static char *pgm;
static void Usage();
static void Error(char *txt,int errno);
static void Error(char *txt);
static int Util(char *file, char *type, char *creator);
static void GetAi(char *dir, char *file, ai_t *ai);
static void UpdateAi(char *dir, ai_t *ai);
static void UtoAname(char *aname, char *uname);

///////
// main
//
main(int argc, char **argv){
  int c;
  char *type=NULL;
  char *creator=NULL;
  
  pgm = argv[0];
  
  // Initialize.
  
  while ((c=getopt(argc,argv,"t:c:"))!=-1)
    switch(c){
    case 't':
      type=optarg;
      break;
    case 'c':
      creator=optarg;
      break;
    default: Usage();
    }
  if (optind>=argc)
    Usage();
  
  return Util(argv[optind],type,creator);
}


////////
// Usage
//
static void Usage(){
  cout << " Usage:\t" << pgm << " [-t type] [-c creator] file\n";
  exit(1);
}


////////
// Error
//
void Error(char *txt,int errno){
  cout << pgm << ": " << txt << " -- " << strerror(errno) << '\n';
  exit(1);
}

void Error(char *txt){
  cout << pgm << ": " << txt << '\n';
  exit(1);
}


///////
// util
//
static int Util(char *path, char *type, char *creator){
  char lpath[256];
  char *file, *dir;
  struct stat dsb, rsb;
  ai_t ai;
  
  // Separate out the directory from the file.
  
  strcpy(lpath,path);
  file = basename(lpath);
  dir  = dirname(lpath);
  
  if (file==NULL || dir==NULL)
       Error("Invalid file specification");
  
  // Stat the file to find out if it is a file or a directory.
  // Get the ancillary entry.

  if (stat(path,&dsb))
       Error("Stat failure, data fork", errno);
  GetAi(dir,file,&ai);

  // Process according to whether its a file or a directory.

  if (S_ISREG(dsb.st_mode)){
    char tpath[256];

    // Regular file.
    // Stat the resource fork.  If the stat fails because there is no file,
    //   just zero the stat block so we see a resource fork size of zero.

    strcpy(tpath,dir);
    strcat(tpath,"/.HSResource/");
    strcat(tpath,file);
    if (stat(tpath,&rsb))
      if (errno==ENOENT)
	memset(&ai,0,sizeof ai);
      else
	Error(tpath,errno);

    // Print the file name, the data and resource fork size, the current
    //   type and creator.
    cout << form("  [%4.4s,%4.4s] (%d:%d) [%s] %s\n",
		 &ai.ai_finfo.fi_type,
		 &ai.ai_finfo.fi_creator,
		 rsb.st_size,
		 dsb.st_size,
		 &ai.ai_sname,
		 file);
    // If a new type or creator is specified, set them.
    if (type || creator){
      if (type)
	strncpy((char*)&ai.ai_finfo.fi_type,type,4);
      if (creator)
	strncpy((char*)&ai.ai_finfo.fi_creator,creator,4);
      UpdateAi(dir,&ai);
      cout << form("->[%4.4s,%4.4s]\n",
		   type ? type : (char*)&ai.ai_finfo.fi_type,
		   creator ? creator : (char*)&ai.ai_finfo.fi_creator);
    }
  }
  else if (S_ISDIR(dsb.st_mode)){
    cout << form("(%d)\t%s/\n",
		 dsb.st_size,
		 file);
  }
  else Error("Invalid file type");
  
  return 0;
}

////////
// GetAi	Read in ancillary record for a file.
//
static void GetAi(char *dir, char *file, ai_t *ai){
  int  fd, cnt;
  char tpath[256];
  char tfile[256];

  strcpy(tpath,dir);
  strcat(tpath,"/.HSancillary");

  if ((fd=open(tpath,O_RDONLY))<0)
    Error(tpath,errno);

  UtoAname(tfile,file);
  while ((cnt=read(fd,ai,sizeof *ai))==sizeof *ai){
    if (strcmp(tfile,ai->ai_lname)==0){
      close(fd);
      return;
    }
  }
  Error("Ancillary file entry not found\n");
}


///////////
// UpdateAi	Update an ancillary record in a file.
//
static void UpdateAi(char *dir, ai_t *ai){
  int  fd, cnt;
  char tpath[256];
  ai_t tai;
  
  strcpy(tpath,dir);
  strcat(tpath,"/.HSancillary");

  if ((fd=open(tpath,O_RDWR))<0)
    Error(tpath,errno);

  if (lockf(fd,F_TLOCK,0))
    Error("Lock failure\n");
  
  while ((cnt=read(fd,&tai,sizeof tai))==sizeof tai)
    if (strcmp(tai.ai_lname,ai->ai_lname)==0){
      lseek(fd,-sizeof tai,SEEK_CUR);
      write(fd,ai,sizeof *ai);
      lockf(fd,F_ULOCK,0);
      close(fd);
      return;
    }
  Error("Ancillary file entry not found\n");
}

/////////
// AtoHex	Convert hex digit to binary.
//
static char AtoHex(char c){
  if (c>='A')
    return (c-'A') +10;
  else if (c>='a')
    return (c-'a')+10;
  else
    return (c & 0xf);
}

///////////
// UtoAname	Convert a unix file name to an apple file name.
//
static void UtoAname(char *a, char *u){
  if (*u=='.')
    u++;
  for (int i=0;*u;u++,i++,a++)
    if (u[0]==':' && isxdigit(u[1]) && isxdigit(u[2])){
      *a = AtoHex(*++u)<<4;
      *a |= AtoHex(*++u);
    }
    else
      *a = *u;
  *a=0;
}
