/*
 * Copyright (C) 1990 by Thomas Hoch and Wolfram Schafer.
 * You may distribute under the terms of the GNU General Public License.
 * See the file named COPYING.
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#define PATH_BUF_LEN	1025  /* Because there's no portable system define. */

/*
NAME:

     brestore (argc, argv)
     int argc;
     char *argv[];

PURPOSE:

     brestore copies files mentioned in the control file for the
     btool_backup directory(ies) back into their original directory. The
     files have been copied into these backup directories by the
     Instrumentation part of the Tool. This is done in order to synchronize 
     the original and the test directories. The brestore program is run after
     the instrumented files have been compiled. It then replaces the
     instrumented versions of the source code with the original versions. 
     This has the advantage that you can instrument your files again without
     having to worry about the fact that you can't instrument an already
     instrumented file. Furthermore, you can compare the output of bresport 
     with the source code in your test directory since breport refers to the
     original source code and the instrumented code is different. brestore
     takes two options. One, -test-ctrl allows you to specify a different
     control file name. Without that option the control file name is assumed 
     to be 'btool_ctrl'. The second option, -test-dir, allows you to specify
     the master directory. This is necessary if you have multiple test
     directories. A rule is to use this second option whenever you used it
     for btool.  


PRECONDITIONS:

     1. The passed in arguments (argv[1..3]) matches the synopsis.
     2. Control file is existing and in the right format
     3. Enough memory to build the new file name.
     4. The old and new file name is not longer than the maximal pathlengh
     

POSTCONDITIONS:

      1. The files mentioned in the control file which were acrtually 
         instrumented by the Tool are in its orginal version.
      2. The control files has not changed.
      
ERRORS:

     1. Error return "brestore: Unknown argument 'argument'"
     2. Error return "brestore: Can't open control file 'filename'" 
     3. Error return "brestore: Virtual Memory full"
     4. Error return "brestore: filename 'filename'... too long"
     


ALGORITHM:

     1. separate passed in arguments (if not ok ==> error (1))
     2. control file (if error ==> error (2))
     3. until there are no more lines to read in the control file
        4.1 get filename (if longer than max pathlen ===> error(4))
        4.2 separate name in control file into directory info and file name
	4.3 allocate memory for new file name (if not enough memory 
	    ===> error (3))
	4.4 add 'btool_backup' to dir-info and file name (if new name too long
	    ===> error(4))
	4.5 issue copy command 
     5. close control file

*/

main(argc,argv)
     int argc;
     char *argv[];
{
  FILE *ctrlfile;
  char *ctrlfile_name = "btool_ctrl";
  char *full_ctrlfile_name;
  char *master_dir =".";

  int i;
  char c;
  char *ctrl_string;

  struct stat statbuf;
  int stat();
 

  for (i= 1; i < argc; i++) {
    if (argv[i][0] == '-') 
      {
        switch(argv[i][1]) 
	  {
       	  case 't':
	    if (!strcmp(argv[i]+1,"test-control"))
		{
		  ctrlfile_name = argv[++i];
		}
	    else
	      if (!strcmp(argv[i]+1,"test-dir"))
		{
		  master_dir = argv[++i];
		}
	      else
		{ 
		  fprintf(stderr,"brestore: Unknown argument %s\n",argv[i]);
		  exit(1);
		}
	    break;
	    
	  default:
	    fprintf(stderr,"brestore: Unknown argument %s\n",argv[i]);
	    exit(1);
	  }
      }
    else
      {
	fprintf(stderr,"brestore: Unknown argument %s\n",argv[i]);
	exit(1);
	
      }
  }

  if ((full_ctrlfile_name = (char *) malloc ( strlen(master_dir) + 2 +
					     strlen(ctrlfile_name))) == 0)
    {
      fprintf(stderr,"brestore: Virtual Memory full \n");
      exit(1);
    }
  strcpy (full_ctrlfile_name, master_dir);
  strcat (full_ctrlfile_name,"/");
  strcat (full_ctrlfile_name,ctrlfile_name);


  ctrlfile = fopen(full_ctrlfile_name,"r");
  if (NULL == ctrlfile) {
   fprintf(stderr,"brestore: Can't open control file %s\n",full_ctrlfile_name);
   exit(1);
  }
  
  do {
    
    char file_name[PATH_BUF_LEN];

    i = 1;
    ctrl_string = file_name;
    
    /* now let's get character for character */
    while ( (c=getc(ctrlfile)) != EOF )
      {
	if (c== '#') 
	  while((c=getc(ctrlfile))!='\n' && (c!=EOF));
	if  (c == ' ' || c == '\t' || c== '\n')
	  break;
	*ctrl_string++=c;
	i++;
	if (i==PATH_BUF_LEN-1) {
	  *ctrl_string = '\0';
   	  fprintf(stderr,"brestore: filename %s... too long. \n",file_name);
	}
	
      }
    *ctrl_string = '\0';
    
/* Notice that leading blanks and tabs in a line are skiped because they are 
   treated like single lines which contain only one character (blank lines).
*/
  
    if (i!=1) 
      {
	char backup[PATH_BUF_LEN], dest[PATH_BUF_LEN];
	char buf[2*PATH_BUF_LEN];
	char pathname[PATH_BUF_LEN];
	char name[PATH_BUF_LEN];
	char *basename;
	
	basename = strrchr(file_name, '/');
	if ((char *)0 != basename)  {
	  basename++; /* over slash  */
	  strncpy(pathname,file_name,basename-file_name);
	  pathname[basename-file_name] = '\0';
	  strcpy(name,basename);	    
	}
	else {
	  strcpy(pathname,"");
	  strcpy(name,file_name);
	}

	if ('/' != pathname[0])	/* Relative pathname */
	  {
	    strcpy(backup,master_dir);
	    strcat(backup,"/");
	    strcpy(dest,master_dir);
	    strcat(backup,pathname);
	    strcat(backup,"btool_backup/");
	    strcat(backup,name);
	    strcat(dest,"/");
	    strcat(dest,file_name);
	  }
	else
	  {
	    strcpy(backup, pathname);
	    strcat(backup, "btool_backup/");
	    strcat(backup, name);
	    strcpy(dest, pathname);
	    strcat(dest, name);
	  }
	
	
	if (-1==stat(backup,&statbuf)) {
	  fprintf(stderr,"brestore: File not found: %s\n",backup);
	}
	else {
	  sprintf(buf,"cp %s %s\n",backup,dest);
	  printf(buf);
	  if (system(buf) != 0)
	    fprintf(stderr, "brestore:cp of %s to %s failed\n", backup, dest);
	  chmod(dest, statbuf.st_mode);
	}
	if ((c!='\n') && (c!=EOF)) {
	  while((((c=getc(ctrlfile))!='\n') && (c!=EOF)));
	}
      }
  }
  while (c !=EOF);

  fclose(ctrlfile);
  exit(0);
}



