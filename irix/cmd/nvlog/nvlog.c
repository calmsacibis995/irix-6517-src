/*
 * NAME
 *	frulog - read or clear fru analysis non-volatile ram area.
 * SYNOPSIS
 *	frlog [-clr] 
 * DESCRIPTION
 *	Get or clear the FRU analysis from NVRAM.
 *
 *
 * $Id: nvlog.c,v 1.1 1997/09/08 18:47:46 jfd Exp $
 */
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <sys/syssgi.h>
#include <sys/types.h>
#include <sys/systeminfo.h>
#include <sys/wait.h>
#include <sys/capability.h>

/*
 * In the event of a system crash. Simply run this command and the
 * last known fru analysis will be dumped from the NVRAM where the
 * kernel stored the analysis.
 */


/*
 * Get NVRAM buffer with fru data.
 */
void get_frulog()
{
  char buf[4096]; 
  char nam[10];
  sprintf(nam, "%s","fru");
  if (syssgi(SGI_GETNVRAM, nam, buf) < 0){
    perror("get_fru_nvram");
    exit(1);
  }
  printf("%s\n",buf);
}

/*
 * Clear fru NVRAM.
 */
void clr_frulog()
{
  char nam[10];
  sprintf(nam, "%s","fru");
  if (syssgi(SGI_SETNVRAM, nam, "<no errors>") < 0){
    perror("set_fru_nvram");
    exit(1);
  }
}

/*
 * print out how it is used.
 */
void usage(int argc,char *argv[])
{
  printf("%s: unrecognized option \"%s\"\n",argv[0],argv[1]);
  printf("%s [-clr]\n",argv[0]);
  exit(-1);
}

/*
 * We recognize -clr and -c to clear. When we run it, it prints
 * out the FRU analysis last run on the system in the event of
 * a crash.
 */
int main(int argc, char *argv[])
{
  if(argc == 2){
    if(!strcmp(argv[1],"-clr")){
      clr_frulog();
    }
    else if(!strcmp(argv[1],"-c")){
      clr_frulog();
    } 
    else{
      usage(argc,argv);
    }
  }
  else{
    get_frulog();
  }
  exit(0);
}

