/*
 * Copyright (C) 1990 by Thomas Hoch and Wolfram Schafer.
 * You may distribute under the terms of the GNU General Public License.
 * See the file named COPYING.
 */

/*
  NAME:
  
  bsummary (argc, argv)
  int argc;
  char *argv[];
  
  PURPOSE:
  
  bsummary gives summary information about a report created by breport.
  This information is: 
  number and percentage of branches taken in true and false direction
  number and percentage of branches taken only in true direction
  number and percentage of branches taken only in false direction
  number and percentage of branches not taken at all
  number and percentage of cases taken
  total number of branches 
  total number of cases
  
  PRECONDITIONS:
  
  1. argv[1] is the name of an existing file or argc == 1
  2. file is in format of breport output
  
  POSTCONDITIONS:
  
  bsummary gives 7 lines of output according to PURPOSE.
  If argc >2 then a USAGE message will be displayed. 
  
  ERRORS:
  
  1. Error return "Can't open file"
  2. Error return "Format mismatch of input"
  
  ALGORITHM:
  
  1. decide upon input device
  2. if file, open it (if not possible ==> error 1)
  3. repeat until no more lines
  3.1. get lines (mismatch causes error 2)
  3.2. case result of line 
  - both directions
  - only true
  ...
  - case
  add appropriate counter
  4. calculate results
  5. print to standard output
  
  */

#include <stdio.h>
#include <string.h>

FILE *report;
int case_count =0;
int  branch_taken =0;
int branch_true_false=0;
int case_true=0;
int branch_true=0;
int branch_false=0;
int branch_not_taken=0;
int branch_count=0;
int false_count_total=0;
int true_count_total=0;
#ifdef sgi
int func_count = 0;
int func_called = 0;
#endif

char
  getline()
{
  char c;
  int true_count=0;
  int false_count=0;
  int case_taken=0;
  int true_detect = 0;
  int false_detect = 0;
  int case_detect = 0;
  int keyword_detect = 0;
  int default_detect = 0;
  int func_detect = 0;
  int func_taken = 0;

  /* start of line is a \n ? If so, then it's a blank line
     do nothing and return */
  if ( (((c=getc(report)) == '\n')) || (c==EOF) )
    {
      return (c);
    }

  while (((c = getc(report))!= EOF) && (c !='\n')) {
    
    if ((c == 'T') 
	&& (getc(report) == 'R') 
	&& (getc(report) == 'U')
	&& (getc(report) == 'E')
	&& (getc(report) == ' ')) {
      fscanf(report,"%d",&true_count); 
      true_detect++;
    }
    if ((c == 'F') 
	&& (getc(report) == 'A') 
	&& (getc(report) == 'L')
	&& (getc(report) == 'S')
	&& (getc(report) == 'E')) {
      fscanf(report,"%d",&false_count); 
      false_detect++;
    }
    if ((c == 'c') 
	&& (getc(report) == 'a') 
	&& (getc(report) == 's')
	&& (getc(report) == 'e')
	&& ((c= getc(report)) == ' ' || c == '(')) {
      case_detect++;
    }
    if ((c == 'd') 
	&& (getc(report) == 'e') 
	&& (getc(report) == 'f')
	&& (getc(report) == 'a')
	&& (getc(report) == 'u')
	&& (getc(report) == 'l')
	&& (getc(report) == 't')
	&& ((c= getc(report)) == ' ' || c == '(')) {
      default_detect++;
    }
    if ((c == 'w') 
	&& (getc(report) == 'a') 
	&& (getc(report) == 's')
	&& (getc(report) == ' ')) {

	if ((c = getc(report)) == 't'
	   && (getc(report) == 'a')
	   && (getc(report) == 'k')
	   && (getc(report) == 'e')
	   && (getc(report) == 'n') /* the following guarantees that only after */
	   && (getc(report) == ' ') /* case or default 'was taken' has a number */
	   && (!(true_detect||false_detect) && (case_detect||default_detect))) {
		fscanf(report,"%d",&case_taken); 
		keyword_detect =1;
	} else if ((c == 'c')
		&& (getc(report) == 'a') 
		&& (getc(report) == 'l')
		&& (getc(report) == 'l')
		&& (getc(report) == 'e')
		&& (getc(report) == 'd')
		&& (getc(report) == ' ')) {
		func_detect = 1;
		fscanf(report,"%d",&func_taken); 
	}
    }
  }
  /* format checking */
  /* three valid types : 
     true and false detected, nothing else
     case and keyword, nothing else
     default and keyword, nothing else         */
  
  if ( !(true_detect==1 && false_detect==1 && !case_detect 
	 && !default_detect && !keyword_detect && !func_detect )
      && !(!true_detect && !false_detect && case_detect==1 
	   && !default_detect && keyword_detect==1 && !func_detect)
      && !(func_detect && !true_detect && !false_detect
	   && !default_detect && !keyword_detect)
      && !(!true_detect && !false_detect && !case_detect 
	   && default_detect==1 && keyword_detect==1 && !func_detect) )
    {
      fprintf(stderr,"bsummary: Wrong input format.\n");
      exit(1);
    }
  
  /* case counting */
  case_count += (keyword_detect ==1);
  case_true += (case_taken >0);
  
  /* branch counting */
  branch_count += (!keyword_detect);
  true_count_total += true_count;
  false_count_total += false_count;
  branch_true_false += (true_count && false_count);
  branch_true += (true_count && !false_count);
  branch_false += (!true_count && false_count);
  branch_not_taken += (!true_count && !false_count && !keyword_detect);

  /* function counting */
  func_count += (func_detect == 1);
  func_called += (func_taken > 0);
  
  return (c);
}


main(argc,argv)
     int argc;
     char *argv[];
{
  char *report_name;
  float per_true_false, per_false,per_true,per_not,per_case,per_func;
  switch (argc) {
  case 1:
    report = stdin;
    break;
  case 2:
    report_name = argv[1];
    report = fopen(report_name,"r");
    if (NULL== report)
      {
	fprintf(stderr,"bsummary: Can't open file %s\n",
		report_name);
	exit(1);
      }
    break;
  default:
    fprintf(stderr,"USAGE:bsummary [reportname]\n");
    exit(1);
  }
  
  while (getline()!=EOF) {};
  
  if (branch_count==0) 
    {
      per_true_false = 0.00;
      per_false = 0.00;
      per_true = 0.00;
      per_not = 0.00;
    }
  else 
    {
      per_true_false = (branch_true_false*100.00/branch_count);
      per_false = (branch_false*100.00/branch_count);
      per_true = (branch_true*100.00/branch_count);
      per_not = (branch_not_taken*100.00/branch_count);
    }
  if (case_count==0) 
    {
      per_case = 0.0;
    }
  else 
    {
      per_case = (case_true*100.00/case_count);
    }
  if (func_count==0) 
    {
      per_func = 0.0;
    }
  else 
    {
      per_func = (func_called*100.00/func_count);
    }
  printf("Branches taken in true and false direction %d (%5.2f%%)\n",
	 branch_true_false, per_true_false);
  printf("Branches taken only in true direction %d (%5.2f%%)\n",
	 branch_true, per_true);
  printf("Branches taken only in false direction %d (%5.2f%%)\n",
	 branch_false, per_false);
  printf("Branches not taken at all %d (%5.2f%%)\n",branch_not_taken,per_not);
  printf("Total number of branches %d\n",branch_count);
  printf("Cases taken %d (%5.2f%%)\n",case_true,per_case);
  printf("Total number of cases %d\n",case_count);
  printf("Functions called %d (%5.2f%%)\n",func_called,per_func);
  printf("Total number of functions %d\n",func_count);
  if (report != stdin)
    fclose(report);
  exit (0);
  
}

