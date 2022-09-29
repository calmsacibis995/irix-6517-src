/*
 * Copyright (C) 1990 by Thomas Hoch and Wolfram Schafer.
 * You may distribute under the terms of the GNU General Public License.
 * See the file named COPYING.
 */

/*
NAME:

     bmerge (argc, argv)
     int argc;
     char *argv[];

PURPOSE:
     
     bmerge merges two or more log files that are specified as command
     line arguments. The merged log file is passed to standard output.

PRECONDITIONS:

     1. The passed arguments are names of existing log files.
     2. These files are in the logfile format. 

POSTCONDITIONS:
     
     If the number of named files is greater or equal one, then
     the specified log files are merged in a way that the counts for 
     true/false of every branch in the merged output is equal the 
     sum of the true/false counts of the same branch in all 
     specified log files.
     If no file is named, then a USAGE message is displayed.
     If the log files are modified with stripping out lines, then
     the output of lines is the 

ERRORS:

     1. Error return is "Can't open file"
     2. Error return is "Format error in file 'filename'"
     Error messages should specify the file that causes error.


ALGORITHM:
     1. open specified files (if not  possible ==> error 1)
     2. while no file == eof
        2.1. reading in next line (if not right format ==> error 2)
        2.2. do while branch number don't match and no file == eof
             2.2.1. read in lines that have a lower branch number 
                    than the highest seen so far.
        2.3. merge information
        2.4. output merged line
     3. exit 


*/

#include <stdio.h>
#include <string.h>

#define MAX_LOGFILES 100
  
int branch_number[MAX_LOGFILES];
int log_num;


int
lowest_num()
{
  int min,pos;
  int i;

  pos = 1;
  min = branch_number[1];
  for (i=1;i<=log_num;i++) 
    if (branch_number[i] < min) {
      min = branch_number[i];
      pos = i;
    }
  return(pos);
}

int
no_match()
{
  int i;

  for (i=1;i<=log_num;i++) {
    if (branch_number[i] != branch_number[1]) {
      return(1);
    }
  }
  return(0);
}
  
main(argc,argv)
     int argc;
     char *argv[];
{
  /* Notice, all arrays start at index 1 not 0 ! */

  FILE *logfile[MAX_LOGFILES];
  char logfile_name[MAX_LOGFILES][1024];
  int i;
  int error=1;
  int merge_true, merge_false;
  int true_count[MAX_LOGFILES], false_count[MAX_LOGFILES];
  int end_of_file,read_num;

  if (argc > MAX_LOGFILES+1)
    {
      fprintf(stderr,"bmerge: Only 100 logfiles can be merged at a time.\n");
      exit(1);
    }

  if (argc == 1)
    {
      fprintf(stderr,"USAGE:bmerge logfile1 logfile2...\n");
      exit(1);
    }
  else
    for (i= 1; i < argc; i++) {
      if (argv[i][0] != '-') 
	{
	  strcpy(logfile_name[i],argv[i]);
	    logfile[i]= fopen(logfile_name[i],"r");
	  if (NULL== logfile[i])
	    {
	      fprintf(stderr,"bmerge: Can't open file %s\n",
		      logfile_name[i]);
	      exit(1);
	    }    
	  
	}
      else
	{
	  fprintf(stderr,"bmerge: Unknown argument %s\n",argv[i]);
	  exit(1);	 
	}
    }
      
  log_num = argc -1;

  do {
    for (i=1;i<=log_num;i++) {
      merge_false=0;
      merge_true=0;
      read_num = fscanf(logfile[i],"%d %d %d\n",&branch_number[i],
			&true_count[i],&false_count[i]);
      if (end_of_file =(read_num !=3))
	break;
    }
    while ((no_match()==1) && (end_of_file==0)) {
      i = lowest_num();
      read_num = fscanf(logfile[i],"%d %d %d\n",&branch_number[i],
			&true_count[i],&false_count[i]);
      end_of_file = (read_num != 3);
    }
    if (!end_of_file) {
      for (i=1;i<=log_num;i++) {
	merge_true = merge_true + true_count[i];
	merge_false = merge_false + false_count[i];
      }
      error = 0;
      printf("%d %d %d\n",branch_number[1],merge_true,merge_false);
    }
  } while (!end_of_file);

  if (error) {
    fprintf(stderr,"bmerge: Format error in file %s \n",logfile_name[i]);
  }
  for (i=1;i<=log_num;i++) 
    fclose(logfile[i]);

  exit(0);
}


