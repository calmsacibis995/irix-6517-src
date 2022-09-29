/*
 * Copyright (C) 1990 by Thomas Hoch and Wolfram Schafer.
 * You may distribute under the terms of the GNU General Public License.
 * See the file named COPYING.
 */

#include <stdio.h>
#include <string.h>
#include "config.h"

#define PATH_BUF_LEN	1025  /* Because there's no portable system define. */

#ifdef USG

getwd(buf)
	char *buf;
{
    return (getcwd(buf, PATH_BUF_LEN) != NULL);	/* TEMP */
}

#endif



/*
NAME:

     breport (argc, argv)
     int argc;
     char *argv[];

PURPOSE:

     breport joins the btool_map file with a particular log file.
     The result is a readable report patterned after the output
     of the UNIX C compiler.
     breport takes the name of the log file as a command line argument.
     The name of a map file is optional. If no map file name is 
     specified, breport assumes the map file is caled btool_map. To specify
     a different name use the -test-map option.
     In addition, the -l option enforces the printing of absolute path names
     instead of relative (default).

PRECONDITIONS:

     1. The passed in arguments (argv[1..3]) matches the synopsis.
     2. every named file is existing and in the right format
     3. The log file and the map file are consistent (from the same
        version of the instrumented program), this means the branch
        numbers in logfile is a subset of the branch numbers in the 
        mapfile.

POSTCONDITIONS:

      1. The files remain unchanged
      2. The output format is of the following form:
         if, for, while, do while, ? statements:
           "filename", line xx: if was taken TRUE x, FALSE x times.
           if there is more than one instance of if, for, ..
           on one line this is indicated by the number of instance
           after the statement identifier. 
         case statemtents:
           "filename", line xx: case was taken x times.
           if there is more than one case statements on one line this 
           is indicated by the number of the instance after the "case"
      
ERRORS:

     1. Error return "bad or misplaced arguments" and a usage line
     2. Error return "Can't open file 'filename'"
     3. Error return "File mismatch between map and log file"
     4. Error return "Format error in 'filename'"

ALGORITHM:

     1. separate passed in arguments (if not ok ==> error (1))
     2. open map file (if error ==> error (2))
     3. if specified open log file (if error ==> error (2))
     4. until there are no more lines to read in one file
        4.1. if branch numbers don't match (if not right format ==> error (4)) 
             4.1.1. do while logfile branch number < map file branch number
                    read in line
             4.1.2. if log file number > map file number ==> error 3
        4.2. merge the informations
        4.3. if one same more than one of this type of branch, add to
             output the (#)
        4.4. output line
     5. if log file is not at end of file ==> error (3)

*/


main(argc,argv)
     int argc;
     char *argv[];
{
  FILE *mapfile;
  FILE *logfile;
  char *mapfile_name = "btool_map";
  char *logfile_name = (char *)0;

  int i, number,branch_number,occur,line_number;
  int branch_no_log = -1;
  int true_count, false_count, log_number;
  int absolute_names =0;
  char file_name[PATH_BUF_LEN], branch_type[100];
  char long_file_name[2*PATH_BUF_LEN];
  char pathname[PATH_BUF_LEN], old_pathname[PATH_BUF_LEN];
  char *basename;
  char new_dir[PATH_BUF_LEN], org_dir[PATH_BUF_LEN];
  char fname[PATH_BUF_LEN];

  for (i= 1; i < argc; i++) {
    if (argv[i][0] == '-') 
      {
        switch(argv[i][1]) 
	  {
	
	  case 'l':
	    absolute_names = 1;
	    if (!getwd(org_dir))
	      {
		fprintf(stderr,"breport: getwd error: %s\n",org_dir);
		exit(1);
	      }
	    else
	      {
		if (strlen(org_dir) == PATH_BUF_LEN -1) {
		  fprintf(stderr,"breport: Directory too long %s\n",org_dir);
		  exit(1);
		}
		strcat(org_dir,"/");
	      }
	    break;
    
	  case 't':
	    if (!strcmp(argv[i]+1,"test-map"))
		{
		  if (i+1 == argc)
		    {
		      fprintf(stderr, "breport: -test-map requires an argument.\n");
		      exit(1);
		    }
		  mapfile_name = argv[++i];
		}
	    else
	      { 
		 fprintf(stderr,"breport: Unknown argument %s\n",argv[i]);
		 exit(1);
	       }
	    break;
	  
	  default:
	    fprintf(stderr,"breport: Unknown argument %s\n",argv[i]);
	    exit(1);
	  }
      }
    else
      if (i!=argc-1) {
	fprintf(stderr,"breport: breport takes only one file as argument\n");
	exit(1);
      }
      else {
	logfile_name = argv[i];
      }
  }
  mapfile = fopen(mapfile_name,"r");
  if (NULL == mapfile)
    {
      fprintf(stderr,"breport: Can't open mapfile %s\n",mapfile_name);
      exit(1);
    }
  
  if (logfile_name == (char *)0)
    {
      logfile = stdin;
    }
  else
    {
      logfile = fopen(logfile_name,"r");
    }
  if (NULL == logfile)
    {
     fprintf(stderr,"breport: Can't open logfile %s\n",logfile_name);
     exit(1);
    } 
  
  do {
    number = fscanf(mapfile,"%d %s %d %s %d %s\n",&branch_number,file_name,
		    &line_number,branch_type, &occur, fname);

    if ((number !=6) && (feof(mapfile)==0)) {
      fprintf(stderr,"breport: Syntax error in mapfile %s\n",mapfile_name);
      exit(1);
    }

    if (number ==6)
      {
	/* The absolute pathnames are evaluated by switching into the 
	   directory specified in the control file and getting the directory
	   name using the getwd command. This is done because it might be
	   possible that symbolic links exist */
	
	if ((absolute_names) && (file_name[0] != '/')) {
	  basename = strrchr(file_name, '/');
	  if ((char *)0 != basename)
	    {
	      basename++; /* over slash */
	      strncpy(pathname,file_name,basename-file_name);
	      pathname[basename-file_name] = '\0';

	      if (strcmp(pathname,old_pathname)) {	/* if change */
		strcpy(old_pathname,pathname); 
		if(chdir(pathname)==0) {
		  if (!getwd(new_dir))
		    {
		      fprintf(stderr,"breport: getwd error: %s\n",new_dir);
		      exit(1);
		    }
		  else
		    {
		      if (strlen(org_dir) == PATH_BUF_LEN -1) {
			fprintf(stderr,"breport: Directory too long %s\n",
				new_dir);
			exit(1);
		      }
		      strcat(new_dir,"/");
		    }
		}
		else {
		  fprintf(stderr,"breport: chdir error: %s\n",pathname);
		  exit(1);
		}
		if (chdir(org_dir)==-1) {
		  fprintf(stderr,"breport: chdir error: %s\n",org_dir);
		  exit(1);
		}
	      }
	      strcpy(long_file_name,new_dir);
	      strcat(long_file_name,basename);
	    }
	  else {
	    strcpy(long_file_name,org_dir);
	    strcat(long_file_name,file_name);
	  }
	}
	else
	  {
	    strcpy(long_file_name,file_name);
	  }

	log_number =3;
	while ((log_number==3)&&(branch_number>branch_no_log)) { 
	  
	  log_number = fscanf(logfile,"%d %d %d\n",&branch_no_log,
			      &true_count,&false_count);
	}

	if (log_number !=3)
	  {
	    if (feof(logfile))
	      fprintf(stderr,"breport: Mapfile and Logfile don't match\n");
	    else
	      fprintf(stderr,"breport: Syntax error in logfile %s\n",logfile_name);
	    exit(1);
	}

	if (branch_number == branch_no_log) 
	  {
	    if (strcmp(branch_type, "func") == 0) {
		  printf("\"%s\", line %d: %s was called %d times.\n",
			 long_file_name,line_number,fname,true_count);
	    } else
	    if (!strcmp(branch_type,"case") || 
		(!strcmp(branch_type,"default")))
	      if (occur>1)
		{
		  printf("\"%s\", line %d: %s(%d) was taken %d times.\n",
			 long_file_name,line_number,branch_type,occur,true_count);
		}
	      else
		{
		  printf("\"%s\", line %d: %s was taken %d times.\n",
			 long_file_name,line_number,branch_type,true_count);
		}
	    else
	      if (occur >1)
		{
		  printf("\"%s\", line %d: %s(%d) was taken TRUE %d, FALSE %d times.\n",
			 long_file_name,line_number,branch_type,occur,true_count,false_count);
		}
	      else
		{
		  printf("\"%s\", line %d: %s was taken TRUE %d, FALSE %d times.\n",
			 long_file_name,line_number,branch_type,true_count,false_count);
		}
	  }
      }
  }
  while (number ==6);
  fclose(logfile);
  fclose(mapfile);

  exit(0);
}



