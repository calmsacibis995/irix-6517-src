/* autolabel()  This routine creates an Autolabel Hex format file with */
/* 	label information. */
/*	   Label size   IC size   #chars/line  */
/*	     .625"	14 pin		8	*/
/*	     .800"	16 pin		10	*/
/*	     .800"	20 pin		10	*/

#define FALSE	0
#define TRUE	1
#define xCR	0x0D

#include <stdio.h>              /* include standard IO file */
#include <string.h>
#include <unistd.h>

FILE *fplabel;			/* file pointer to label file */
char lb[] =			/* initialize input buffer */
"                                                ";

void usage(char *);

main (argc,argv)
int argc;
char *argv[];
{
    int i = 0;			/* index variable */
    int j, checksum;
    char linelength, c, linecount, lines, chksum;	
    char Partno[20];		/* Programmed part number string */
    char SPartno[13];		/* Programmed part number string */
    char Labelname[13];		/* Prom/pal Label string */ 
    char Labelfile[20];		/* Prom/pal label filename string */ 
    char tmpstring[84];
    char labeldata[84];		/* Converted ascii chars to send to handler */
    char PWidth[84];		/* ASCII string with part width */
    int pincount = 14;
    int pinsize = 0;
    int partwidth = 0;

    /* Initialize the arrays */
    for (j=0; j<11; j++)
    {
	Partno[j] = 0;
	Labelname[j] = 0;
    }

    for (j=0; j<20; j++)
    {
	Labelfile[j] = 0;
    }

    for (j=0; j<84; j++)
    {
	tmpstring[j] = 0;
    }

    /* Check # of arguments */
    if (argc < 3) {
	usage(argv[0]);
    }

    /* Scan the input */
    sscanf(argv[1],"%s", Partno);
/*    printf("Partno has %d chars\n",strlen(Partno)); */
    sscanf(argv[2],"%s", Labelname);
    if (argc > 3)
	sscanf(argv[3],"%d", &pincount);
    if (argc == 5)
	sscanf(argv[4],"%s", PWidth);

/*  Process Partno to reduce to 8 chars */

/*  Take first 4 chars of Partno */
    for (i=0,j=0; j<4; j++)
    {
	tmpstring[i++] = Partno[j];
    }

/*  Strip leading 0 chars out of the Partno */
    for (i=4,j=5; j<9; j++)
    {
	tmpstring[i++] = Partno[j];
    }

/*  Strip leading 0 chars out of the Partno */
    for (i=8,j=10; j<12; j++)
    {
	tmpstring[i++] = Partno[j];
    }

    tmpstring[j] = 0;
    strcpy(SPartno,tmpstring);
/*    printf("Partno = %s\n",Partno); */
/*    printf("SPartno = %s\n",SPartno); */

    /* Clear tmpstring */
    for (j=0; j<84; j++)
    {
	tmpstring[j] = 0;
    }

    if (!strncmp(PWidth,".600",4))
    {
	partwidth = 1;
    } else {
	partwidth = 0;
    }
/*    printf("PWidth = %s\n", PWidth); */
/*    printf("partwidth = %x\n", partwidth); */

    printf("Partno = %s\n", Partno);
    printf("Label = %s\n", Labelname);
    printf("Pin count = %d\n", pincount);
    printf("Part width = %s inches ==> ", (partwidth?".600":".300"));
    if (partwidth)	   /* If 300 mil part, limit label to 2 lines */
    	lines = 4;
    else
    	lines = 2;

/*    printf("Label has %d lines\n", lines); */

    strcpy(Labelfile,Partno);
    strcat(Labelfile,".label");

    printf("Opening file %s\n",Labelfile);
    if ((fplabel = fopen(Labelfile, "w")) == NULL)
    {
	fprintf(stderr,"Error opening %s\n",Labelfile);	/* open output file */
	exit(0);
    }
    fputs("Q001FE\n",fplabel);		/* write header record to file */

    /* Set up device width */
    if (partwidth == 0)		/* part width = .300" */
    	fputs("Q20200FD\n",fplabel);
    else
    	fputs("Q20201FC\n",fplabel);	/* part width = .600" */

    /* Set up pincount */
/*    printf("Set up pincount\n"); */
    switch(pincount)
    {
	case 14:
		pinsize = 0;
		break;
	case 16:
		pinsize = 1;
		break;
	case 18:
		pinsize = 2;
		break;
	case 20:
		pinsize = 3;
		break;
	case 24:
		pinsize = 4;
		break;
	case 28:
		pinsize = 5;
		break;
	case 32:
		pinsize = 6;
		break;
	case 40:
		pinsize = 7;
		break;
    }

    fputs("Q302", fplabel);	/* Write data record and length to file */
    tmpstring[0] = pinsize;
    tmpstring[1] = 0;
    checksum = calcksum(2,tmpstring);	/* Calculate checksum */
    sprintf(tmpstring,"%02d%02x",pinsize,checksum); /* Write pinsize */
    /* convert to upper case characters */
    Uppercase(tmpstring);
    fputs(tmpstring,fplabel);

    		   /* Get label information */
/*    printf("Set up label\n"); */


    /* Set up first line */
    if (lines == 4) {
	sprintf(labeldata,"\r"); 	/* Skip the first line */
    }
    strcat(labeldata,SPartno); 	/* Add first line to label string */
    sprintf(tmpstring,"\r");
    strcat(labeldata,tmpstring);	/* Add CR to terminate the line */

    /* Set up second line */

    /* Convert to uppercase */
    Uppercase(Labelname); 

    strcat(labeldata,Labelname);	/* Add first line to label string */
    sprintf(tmpstring,"\r");
    strcat(labeldata,tmpstring);	/* Add CR to terminate the line */



    linelength = (strlen(labeldata) + 1) & 0xFF; /* Add 1 for checksum */
    checksum = calcksum(linelength,labeldata);	/* Calculate checksum */

    hexconvert(labeldata,tmpstring);	/* Convert labeldata to ascii string */
    strcpy(labeldata, tmpstring);

    /* Prepend length to labeldata string */
    sprintf(tmpstring, "%02x", linelength);
    Uppercase(tmpstring);
    strcat(tmpstring,labeldata);	/* Pre-pend linelength to labeldata */
    strcpy(labeldata,tmpstring);	/* Copy string back into labeldata */


    /* Now write label data to the file */
    fputs("\nQ1",fplabel);	/* Write data record */
    fputs(labeldata,fplabel);	/* Write record length and label data to file */
    sprintf(tmpstring,"%02x",checksum);
    Uppercase(tmpstring);
    fputs(tmpstring, fplabel);		/* Write checksum to record */

    /* Write termination record */
    fputs("\nQ901FE\n", fplabel);	/* Write termination record */
    fclose(fplabel);			/* close output file */
}

void
usage(cmdname)
char *cmdname;
{
    printf("usage: %s partnumber labelname [no_of_pins] [partwidth]\n",cmdname);
    exit(0);
}


cput(a)
char a;
{
    char tmpbuf[3];
    int i;

    sprintf(tmpbuf,"%02x", a);
    for (i = 0; i<3; i++)
        tmpbuf[i] = toupper(tmpbuf[i]);
    fputs(tmpbuf,fplabel);
}

/* calcksum -- This routine calculates the checksum of the first num bytes */
/*          of the string pointed to by string.  It returns the least */
/*          significant byte of the calculated checksum */

calcksum(num, string)
int num;	/* number of bytes to calculate */
char *string;	/* pointer to string to calculate */
{
	int i, sum;

	i = 0;
	sum = num;
	while (string[i] != 0)
	{
		sum += string[i++];
	}
	sum = (~sum & 0xff);
	return (sum);
}

hexconvert(instr, outstr)
char *instr;
char *outstr;
{
	int i, j;
	char tmpstr1[168];
	char tmpstr2[168];

/*	printf("hexconvert: instr = %s (%d chars)\n", instr, strlen(instr)); */
	for (i = 0; i<168; i++) 	/* initialize tmpstring */
	{
		tmpstr1[i] = 0;
		tmpstr2[i] = 0;
	}

	/* convert instring into ascii representations of the hex values */
	for (i = 0; i < strlen(instr); i++)
	{
		sprintf(tmpstr1,"%02x",instr[i]);
		strcat(tmpstr2,tmpstr1);
	}

	/* convert to upper case characters */
	for (i = 0; i<strlen(tmpstr2); i++)
	{
		tmpstr2[i] = toupper(tmpstr2[i]);
	}

	/* copy converted string to outstr */
	strcpy(outstr,tmpstr2);
/*	printf("hexconvert: outstr = %s\n", outstr); */
}

Uppercase(instring)
char *instring;
{
	int i, j;
	char tmpstring[168];


	for (i = 0; i<168; i++) 	/* initialize tmpstring */
	{
		tmpstring[i] = 0;
	}

	/* convert to upper case characters */
	for (i = 0; i<strlen(instring); i++)
	{
		tmpstring[i] = toupper(instring[i]);
	}

/*	printf("Uppercase:  instring = %s\n",instring); */
/*	printf("Uppercase:  uppercase = %s\n",tmpstring); */

	/* copy converted string to outstr */
	strcpy(instring,tmpstring);
}
