/* 
 * AIM Suite III v3.1.1
 * (C) Copyright AIM Technology Inc. 1986,87,88,89,90,91,92.
 * All Rights Reserved.
 */

#ifndef	lint
	static char sccs_id[] = { " @(#) blend.c:3.2 5/30/92 20:18:30" };
#endif

#include <stdio.h>
#include "testerr.h"

#define debug 1		/* set to 1 when debugging */
#define NF 50		/* num factors displayed horizontally */
#define NR 30		/* max data records, one-per-machine */
#define NU 30		/* max size use dictionary */

	int  usewei[NU][50];	/* use weights */
	int  usepct[NU];	/* pct for each use */
	char usenam[NU][10];    /* use names  1-10 nonblank chars */
	char uselab[50][10];	/* col labels 1-10 nonblank chars */
	char usebyt[50][10];	/* col "bytes" 1-10 nonblank chars */
	char usesec[50][10];	/* col "sec" 1-10 nonblank chars */

int	nu;		
char    *defs = "defs";		/* usage weights definition file */
char    *qout = "qoutput";      /* qoutput database file */
char    *mods = "models";	/* default models file */
FILE	*mf;			/* mods stream pointer */
int     pe = -1;

struct {
	int weight;
	char tname[15];
} tasklist[50];

int	tmparr[50]; 

#define ECHO(x)  if (pe-->0) {x;}

char    buf[100];		/* collecting/formatting buffer */
char    model[300];		/* buffer for model printing */

main()
{
	FILE	*fp;
	char	tstuf[70];
	int     i, j, k;

	/*  Read dictionary and database */
	read_defs();	/* read mnemonics for modelling */

	mf = fopen("mixb","r");
	read_model(mf);

	for (i=0;i<50;i++)
	    tmparr[i] = 0;

	for (i=0;i<30;i++) {
	    if (usepct[i]>0) {
		for(j=1;j<50;j++) {
		    if (usewei[i][j] > 0)
			tmparr[j] = tmparr[j] + (usepct[i] * usewei[i][j]);
			DEBUG(fprintf(stderr,"usepct[%d] = %d, usewei[%d][%d] = %d\n",i,usepct[i],i,j,usewei[i][j]));
		}
	    }
	}

	k = 0;
	for (i=0;i<50;i++) {
	    if (tmparr[i] > 0) {
		tasklist[k].weight = tmparr[i];
		strncpy(tasklist[k].tname,uselab[i],10);
		++k;
	    }
	}

	fp = fopen("workfile","w");
	for (i=0;i<k;i++) {
	    sprintf(tstuf,"%d %s\n",tasklist[i].weight,tasklist[i].tname);
	    fprintf(fp,tstuf);
	}
	fclose(fp);
	exit(0);
}


read_defs()
{
	/*
	 * Read dictionary lines of the form:
	 *
	 *	usage      col,weight,label,unit   col,weight,label,unit ...
	 *	
	 *	col=test number, weight = integer test weight
 	 *	label=column heading.  optional, if redundant.
	 *	unit = column unit of time.
	 *	Example:
	 *	WP     20,1,CPU   69,4,TTY
	 *	NC     20,100       69,0
	 *      (the first usage name is wp, nc is the second, and
	 *	the labels, CPU and TTY are used as column headings.
	 *	Test 20 has weight 1, test 69 has weight 4.
	 *	With usage NC, only CPU(test 20) counts, at weight 100.)
	 *	
	*/
	int  i;
	int  c;
	char lab[50];	/* label collection buffer */
	char bytb[50];	/* byte collection buffer */
	char secb[50];	/* seconds collection buffer */
	int  col,wei;
	int  sep;
	FILE *fs;

	fs = fopen(defs,"r");	/* open defs file to read mnemonics
					for modelling */
	DEBUG(fprintf(stderr, "read_defs(0x%x)\n", fs));
	if (fs==NULL)
	    exit(fprintf(stderr, "no defs file\n"));
	ECHO(printf("\nUSAGE DICTIONARY\n"));
	/*  install default col labels */
	strncpy(uselab[0],"COST",10);
	for (i=0; i<NU; i++)
	    sprintf(usenam[i],"test%02d",i), usepct[i]=0;
        for (nu=0;nu<NU;nu++)  
            for (i=0;i<50;i++)
                usewei[nu][i] = 0;

	/*  read in dictionary and col labels */
	for(nu=0; nu<NU;nu++)  {
 	    /* skip white */
	    while ((c=getc(fs))==' ' || c=='\t' || c=='\n') ;
	    DEBUG( fprintf(stderr, "topreaddef c=%c\n",c) );
	    if (c==EOF) return (1);	/* sense end of file */
	    /*  collect usename  */
	    strncpy(usenam[nu],"         \0",10);
	    for(i=0; (c!=' ' && c!='\t' && c!=EOF); c=getc(fs))
		if (i<10) 
		    usenam[nu][i++]=c;
	    if (i<10) usenam[nu][i]='\0'; /* terminate */
		    		
	    if ( nu>0 && strncmp(usenam[nu],usenam[nu-1],10)==0 ) 
		nu = nu-1;	 /* continue 2nd line same uselab */
            DEBUG(fprintf(stderr,"%2d  %-.10s \n ",nu,usenam[nu]) );
            ECHO( printf("%2d  %-.10s \n ",nu,usenam[nu]) );
	    do  {
		    /*
		     *  Parse   Weight,Col,Label,bytes,secs
		    */
		    wei = col = bytb[0] = secb[0] = lab[0] = 0;

		    while (c==' '||c=='\t') c=getc(fs);	/* skip white */
		    for(wei=0; c>='0'&&c<='9'; c=getc(fs))
		        wei = wei*10 + (c-'0');		/* weight */

		    /* determine more defs on line or just comment */
		    if (c==',')
			c=getc(fs);			/* comma */
		    else   {
		        while ( c!=EOF && c!='\n')
			    c=getc(fs);
			break;			/* skip comment */
		    }

		    for(col=0; c>='0'&&c<='9'; c=getc(fs))
		        col = col*10 + (c-'0');	/* collect ,column */

		    if ((sep=c)==',')  {	/* collect ,label */
			for (i=0; (c=getc(fs))!=' ' && c!=',' && c!=EOF && c!='\n';)
			lab[i++]=c, lab[i]=0;
		    }

		    if ((sep=c)==',') {		/* collect ,bytes label */
			for (i=0; (c=getc(fs))!=' ' && c!=',' && c!=EOF && c!='\n';)
			bytb[i++]=c, bytb[i]=0;
		    }

		    if ((sep=c)==',') {		/* collect ,secs label */
			for (i=0; (c=getc(fs))!=' ' && c!='\t'
				  && c!=EOF && c!='\n';)
			secb[i++]=c, secb[i]=0;
		    }
		    DEBUG(fprintf(stderr,"%d  %d,%d%c%-.10s %-.10s %-.10s\n",nu,wei,col,sep,uselab[col],usebyt[col],usesec[col]));
		    /*
		     *  Install lab,col,wei
		     */
		    if ( col>=0 && col<50 )  {
			usewei[nu][col] = wei;
		        if (lab[0] !='\0')strncpy(uselab[col],lab, 10);
		        if (bytb[0]!='\0')strncpy(usebyt[col],bytb,10);
		        if (secb[0]!='\0')strncpy(usesec[col],secb,10);
			DEBUG(fprintf(stderr,"X2 %d  %d,%d%c%-.10s %-.10s %-.10s\n",nu,wei,col,sep,uselab[col],usebyt[col],usesec[col]));
			ECHO(printf("  %d,%d%c%-.10s %-.10s %-.10s\n",wei,col,sep,uselab[col],usebyt[col],usesec[col]));
		    }
	            if ( c == '\n' ) 
		        break;		/* sense EOL */
		} while ( c==' ' || c=='\t' );
	    DEBUG(fprintf(stderr,"\n"));
	    ECHO(printf("\n"));
	}
	fclose(fs);	/* and close it off */
	return(0);
}



read_model(fs)
FILE *fs;
{	
	/* 
	 *  Read freeform weight-use pairs into usepct[] array:
	 *	weight use    weight use   weight use ... EOL
	 *  Returns 0 on EOF.
	*/
	int  weight;	/* collect weight */
	char wnam[11];	/* collect name  */
	int usei;	/* scan use names */
	int i;
	int c;
	float wsum;
	
	if ( fs==NULL ) 
	    exit(printf("can't read models file\n"));
	DEBUG(fprintf(stderr,"read_model(0x%x)\n",fs));
	ECHO(fflush(stdout));
	ECHO(printf("\nModel Parameters...\n"));
	for (i=0; i<nu; i++)
	      usepct[i] = 0;	/* zero model */
	model[0] = '\0';

	for(;;)  {
again:
		/*  skip whitespace */
		while ((c=getc(fs))==' '||c=='\t') ;
		/*   collect an integer weight */
		for (weight=0; (c>='0' && c<='9'); c=getc(fs))
			weight = 10*weight + (c-'0');
		if (weight==0) weight = 1;	/* null weight = 1 */

		/*   sense end conditions  */
		if ( c==':' )  {
			/* # comments are copied with model */
			for(i=0;c!=EOF&&c!='\n';c=getc(fs))
			      buf[i++]=c;/* keep for print */
			buf[i] = '\0';
			strcat(model,buf);	/* with model */
		}

		if (c==EOF) 	  return(0);	/* EOF exit */
		else if (c=='\n') break;	/* 1 line-per-model */

		/* skip whitespace */
		while (c==' '||c=='\t') c=getc(fs);
		
		/*   collect use mnemonic */
		for (i=0; c!=EOF && c!='\n' && c!='\t' && c!=' '; c=getc(fs))
		       if (i<10)wnam[i++]=c;
		if(i<10)wnam[i]='\0';	/* terminate */

		/*  look up in usenam table  */
		usei = -1;
		for (i=0; i<nu; i++)
		      if ( strncmp(wnam,usenam[i],10)==0 )
			    {usei = i;break;}	/* bingo */

		/* skip any # comment lines */
		if ( strcmp(wnam,"#")==0 )  {
		      while ( c != '\n' )
			  c=getc(fs);	/* skip to eol */
		      goto again;
		}
		
	 	/* validate tranlation */	
		if ( usei<0 || usei>=nu ) 
		      exit(printf("read_model-unknown name: %s\n",wnam));
		usepct[usei] = weight;	/* install in this model */
		sprintf(buf,"%d %.10s   ",weight,wnam);
		DEBUG(fprintf(stderr,"%s",buf));
		ECHO(printf("%s",buf));
		strcat(model,buf);
		if (c=='\n') break;	/* sense EOL */
	}

	DEBUG(fprintf(stderr,"\n"));
	ECHO(printf("\n"));

	/*  Normalize usepct to 100 percent */
	wsum = 0.;
	for (i=0; i<nu; i++)
		wsum += usepct[i];
	if (wsum==0)exit(printf("read_model-no weights\n"));
	for (i=0; i<nu; i++)
		usepct[i] = (100 * usepct[i] / wsum);
	DEBUG(for (i=0; i<nu; i++) fprintf(stderr,"usepct[%d] = %d\n",i,usepct[i]);
	      fprintf(stderr,"wsum is %d\n",wsum));

	return (1);		/* return model correctly input */
}

