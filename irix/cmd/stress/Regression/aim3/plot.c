/*
 * AIM Suite III v3.1.1
 * (C) Copyright AIM Technology Inc. 1986,87,88,89,90,91,92.
 * All Rights Reserved.
 */

#ifndef	lint
	static char sccs_id[] = { " @(#) plot.c:3.2 5/30/92 20:18:44" };
#endif


/*  PLOT - Plot numeric x,y,pen data on dumb terminals.
**
**  	Plot produces a quick-and-dirty x,y graph for terminal
**	examination of numeric data input in columns.  Windowing can be
**	specified, and different pen characters can be set between plot files.
**	Data lines beginning with the two characters ': ' are skipped,
**	remainder of lines beginning with '(  ' are listed verbatim.
**	("comment" and "echo" a-la shell conventions.)
**
**	Synopsis:
**		plot -xn w-w %f  file... 		default  -x1
**		     -yn w-w %f				default  -y2
**		     -pn w-w %c				
**		
**		     -x s,s... w-w %f  file...
**		     -y s,s... w-w %f  file...
**		     -p s,s... w-w %c  file...		default  -p 1
**
**	Keyword options:
**		-hn -wn -mhn -mwn -en -e str
**
**		-hn	n rows high plot		default  -h12
**		-wn	n column wide plot		default  -w40
**		-mhn	n row high margin 		default  -mh2
**		-mwn	n column wide margin      	default  -mw6
**		-en	echo first n-data points	default  -e0
**		-e str  preface graph with str
**
**	Positional Options:
**
**		-xn	x-column,y-column,pen-column. default -x1 -y2
**		-yn	x,y-values locate points to be plotted within 
**		-pn	window (w-w) range, while p-values index characters
**			in the pens string (%c).  A finite-differences list
**		   	may be specified (s,s,...) for x,y,p options.  Example
**			-p 1,1 produces p-values 1 2 3 4 5 etc., while -p1
**			means values shall be read from column 1 each record,
**			and both -p 1,0 and -p 1 produce pen values 1 1 1...
**
**		w-w	specifies low-high range for x,y and p.  
**		%f	specifies printf-style label for x,y.	default  %.1f
**		%c	pens string is c.			default  %123456789
**		n	optionally signed number with optional decimal point
**
**     Bugs:
**		Filenames must not look like numbers.
*/
#include <ctype.h>
#include <stdio.h>
#define mmax(x,min,max) if (x<min) min=x; else if(x>max) max=x;
#define up 60
#define over 120
double atof();
char *scra();
double col();
double getval();
FILE *f = 0;
char BLANKS[] = "                                                                                                                  ";

char *p;
struct	coors
      {	float 	xc,	/* column number */
		xv[5],	/* value sequence */
		xl,	/* left (min) */
		xr;	/* right (max) */
	char 	xfmt[up];/* label format */
      }
	xopt = {1.,0.,0.,0.,0.,0.,0.,0.,"%.1f"},	/* x-options list */
	yopt = {2.,0.,0.,0.,0.,0.,0.,0.,"%.1f"},	/* y-options list */
	popt = {0.,1.,0.,0.,0.,0.,0.,0.,"%123456789"},/* p-options list */
	*coor = &xopt;	/* any option list */
float	h = 12.,	/* number rows high, plot+margin */
	w = 40.,	/* number cols wide, plot+margin */
	mw = 6.,	/* margin width, in cols left of plot */
	mh = 2.;	/* margin height, in rows below plot */
float	num,		/* global return from isnum() */
	tmp;		/* tmp for arg handling */
int	aw = 1;		/* auto windo flag */
int	n;		/* number points in xyp[] arrays */
int	nplots = 0,	/* count of plots output */
	nfiles = 0;	/* count of arg files read */
int echo = 0;
float	x[1000],y[1000]; 
int	pv[1000];
int	ixy = 0;
char str[5120];
char scr[up][over];
char stmp[128];
int     col_not_there;		/* flag set by col if missing columns */

main(argc,argv)
int argc; char **argv;
{
	/*  
	**  Initiallize screen, default arglist.
	*/

	int i;
	p = &scr[0][0];
	for (i=0;i<up*over;i++)
	      *p++ = ' ';

	/* 
	**  munch args, expecting options and filenames 
	*/

	while (--argc>0)
	      {
		p = *++argv;
		if (p[0]=='-' && p[1]=='\0') 	/* standard input */
		      {
			if (f!=NULL)		
			      fclose(f);	/* close open file */

			f = stdin;
			n = rd_data("std-input");
			continue;
		      }
		else if (p[0]=='-' && isalpha(p[1]))
		      {	/*
			 *  decompose -xypmwhe options.
		 	*/

			switch (p[1])
			      {
	case 'x':
		coor = &xopt; if((p+=2)[0]!='\0') coor->xc = atof(p); continue;
	case 'y':
		coor = &yopt; if((p+=2)[0]!='\0') coor->xc = atof(p);  continue;
	case 'p':
		coor = &popt; if((p+=2)[0]!='\0') coor->xc = atof(p); continue;
	case 'm':	/* decompose -mh and -mw */
		tmp = atof(p+3); *((p[2]=='h')?&mh:&mw) = tmp; continue;
	case 'h':
		h = atof(p+=2); continue;
	case 'w':
		w = atof(p+=2); continue;
	case 'e':
		if ((p+=2)[0]=='\0') {if (argc>1) printf("%s\n",(--argc,*++argv)); }
		else
		      echo = atof(p);
		continue;
		      }
			      }
		else if (isnum(p))
		      { /*
			** 	decompose   num  num-num  num,num
			*/

			int k;
			if (p[0]!='-')		/*  check break character */
			      {
			      	for (k=0;k<5;k++)
					coor->xv[k] = 0;  /* clear array */
				coor->xc = 0;	/* clear any col nums in there */
			      	for (k=0;k<5;k++)
				      {
					if (p[0]==','||p[0]=='\0')
					      coor->xv[k] = num;
					if (p[0]=='\0')
					      break;
		
					if (p[0]==',')
					      if (!isnum(++p)) /* shift */
						    break;
				      }
		  	      }

			/*
			** 	decompose   num-num
			*/

			if (p[0]=='-')
			      {
				coor->xl = num;
				coor->xr = atof(++p);
			      }
			continue;
		      }
		else if (p[0]=='%')
		      {
			/*
			 *  process %fmt option.
			*/

			strncpy((char *) coor->xfmt, p, up);
			continue;
		      }
		else
     		      {
			/*  
			**	else arg must be filename  to plot
			*/

			f = (f==NULL) ? fopen(p,"r") : freopen(p,"r",f);
			n = rd_data(p);
			nfiles++;
		      }
	      }


	if (nfiles==0)
	      f = stdin, n = rd_data("std-input");/* default stdinput */
	if (nplots==0 && n>0)
	      graph(aw);		/* flush pending graph */

	exit(0);
}

rd_data(filenam)
char *filenam;
{
	int	np;	/*  number chars in format string */
	int	pval;	/*  temporary p holder */
	int	awx, awy;	/* save auto-window indications */
	int	sxy = ixy;	/* save ixy on entry  */

	np = strlen(popt.xfmt);	/* number of pens in pfmt */

	/*  ignore unopened files */

	if (f==NULL)			
	      {
	      	fprintf(stderr, "graph:can't read %s\n",filenam);
		return(0);
	      }

	if (echo>0)
	      printf("plot: file %s\n",filenam);

	/*	
	**	read thru file, accepting data if pen value OK.
	*/

	while (fgets(str,5120,f)!=NULL)		/* munch until eof */
	      { 
		p = str;
		while (*p==' ')
		      p++;	/* skip leading whitespace */

		if (p[0]==':' && p[1]==' ')
		      continue;		/* skip  :  comment line */
		if ( p[0]=='(' && (p[1]==' '||p[1]=='	') )
		      {
			puts(p);	/* ( commentary line ) */
			continue;
		      }
		else if ( p[0]=='e'&&p[1]=='c'&&p[2]=='h'&&p[3]=='o'&&(p[4]==' '||p[4]=='	') )
 		      {
		        puts( p+=5 );	/* 'echo' remainder */
			continue;
		      }
		else if (p[0]=='\n')
		      continue;		/* skip blank lines */
		else
		      {	/*
			**  extract field or compute plot data 
			*/
			col_not_there = 0;		/* clear flag */
			x[ixy] =  getval(&xopt);
			y[ixy] =  getval(&yopt);
			pval   =  getval(&popt);

			if (pval<1 || pval>=np || col_not_there )
			      continue;		/* if pen off list */
			else
			      pv[ixy] = popt.xfmt[pval];
		
			if (ixy==sxy)	/* very late initialization */
			      {
				if (awx = xopt.xr==xopt.xl)
				      xopt.xr = xopt.xl = x[sxy];
				if (awy = yopt.xr==yopt.xl)
				      yopt.xr = yopt.xl = y[sxy];
			      }

			if (awx)
			      mmax( x[ixy], xopt.xl, xopt.xr);
			if (awy)
			      mmax( y[ixy], yopt.xl, yopt.xr);

			if ( x[ixy]<xopt.xl || x[ixy]>xopt.xr ||
			     y[ixy]<yopt.xl || y[ixy]>yopt.xr )
			      continue;	/* skip point outside window */
		      }

		if (echo-->0)
		      {
		      	printf("%4d\t",ixy+1);
			printf(xopt.xfmt,x[ixy]),putchar('\t');
			printf(yopt.xfmt,y[ixy]);
			printf("\t%c\n",pv[ixy]);
		      }

		if (ixy<999)
		      ixy++;		/*  accept point in array */

	      }

	return (ixy);			/* successful return */
}

/*  COL -- extract k-th column from record
**
**	columns separated by spaces.
**
**	Parameters:
**		k -- column number
**		p -- pointer to record
**
**	Returns:
**		VAL -- numeric value of desired column 
**
*/

double
col(k,p)
int k; char *p;
{
	double tmp;
	/*  skip to desired column */

	while (--k>0)			
	      {
		while ( *p!=' ' && *p!='\0' )
		      ++p;		

		/*  slough trailing blanks */

		while (*p==' ')
		      ++p;

	      }

	col_not_there |= (*p=='\000');/* accumulate missing col flag */
	tmp = atof(p);
	return (tmp);
}


graph(awf)
int awf;		/* auto-window flag */
{
	int 	i,j;
	int	nh,		/* num-1 rows high plot */
		nw,		/* num-1 cols wide plot */
		xo,		/* screen loc for x_origin */
		yo;		/* screen loc for y_origin */
	float	xs,		/* floating sum x */
		ys,		/* floating sum y */
		x2,		/* floating sum x**2 */
		y2;		/* floating sum y**2 */
	
	float	xd,		/* max-min x axis */
		yd,		/* max-min y axis */
		pd;		/* max-min p axis */


	/* 
	**  initialization
	*/

	xs = ys = x2 = y2 = 0.0;
	nh = h-mh-1;		/* height-1 of plot area */
	nw = w-mw-1;		/* width-1 */
	xo = mw;
	yo = nh;		/* offset of plot origin in scr buffer */
	
	for (i=0;i<n;i++)
	      {
		xs += x[i];
		ys += y[i];
		x2 += x[i]*x[i];
		mmax( x[i], xopt.xl, xopt.xr);
		mmax( y[i], yopt.xl, yopt.xr);
	      }

	if (0)
	      awf+x2+y2+xs+ys+scrf(0,0);	/* lint plug */
	if ((xd = xopt.xr-xopt.xl)<=0)
	      xd = 1.;			/* minimum scale = 1 */
	if ((yd = yopt.xr-yopt.xl)<=0)
	      yd = 1.;
	if ((pd = popt.xr-popt.xl)<=0)

	for (i=0;i<=nh;i++)
	      scrw(xo,yo-i,'|');	/*  draw y axis   */
	for (i=0;i<=nw;i++)
	      scrw(xo+i,yo,'-');	/*  draw x axis  */

	/*
	**  pass2 -- write chars in screen buffer.  Check points in
	**  window bounds, scale x,y and mark pen value in plot area.
	**  If margins allow, labels will be applied for each point.
	*/

	for (i=0;i<n;i++)
	      {	
		int xscr, yscr, xml, yml;
		xscr = .5 + xo + (x[i]-xopt.xl)/xd*nw;
		yscr = .5 + yo - (y[i]-yopt.xl)/yd*nh;

		scrw(xscr,yscr,pv[i]);	/* write point on screen */

		/*
		**  look in margins for enough whitespace
		**  to format the coordinates for this point
		*/

		sprintf(stmp,xopt.xfmt,x[i]);/* x-label */
		xml = strlen(stmp);	/* get length of formatting */
		for (j=1;j<=mh;j++) 	/* look in lines below plot */
		      {
			if (strncmp(scra(xscr-xml+1,yo+j),stmp,xml)==0)
			      break;	/* sense it's already there */
		
			if (strncmp(scra(xscr-xml,yo+j),BLANKS,xml+2)==0)
			      {	
				strncpy(scra(xscr-xml+1,yo+j),stmp,xml);
				break;
			      }
		      }

		sprintf(stmp,yopt.xfmt,y[i]);/* label y axis */
		yml = strlen(stmp);
		if (strncmp(scra(xo-yml,yscr),BLANKS,yml) == 0)
		      strncpy(scra(xo-yml,yscr),stmp,yml);
	      }

	/* force in window corner labels */
	{
	    int xscr, yscr, xml, yml;

#define wedge(yopt,xl,yscr,yopnh,yml,xomyml,yscryo) \
		sprintf(stmp,yopt.xfmt,yopt.xl); yscr=.5+yopnh; \
		yml=strlen(stmp); strncpy(scra(xomyml,yscryo),stmp,yml);

		wedge(yopt,xl,yscr,yo-00,yml,xo-yml,yscr);
		wedge(yopt,xr,yscr,yo-nh,yml,xo-yml,yscr);
		wedge(xopt,xl,xscr,xo+00,xml,xscr-xml+1,yo+1);
		wedge(xopt,xr,xscr,xo+nw,xml,xscr-xml+1,yo+1);
	}

	/*
	**  write out plot from screen
	*/

	for(i=yo-nh;i<=yo+mh;i++)
	      {
		for (j=xo-mw;j<=xo+nw;j++)
		      putchar(scr[i][j]);
	
		putchar('\n');
	      }

	nplots++;		/* indicate plotted */
}


/*  SCRF, SCRA, SCRW - screen buffer read, address and write
**
**	Functions check x,y bounds before addressing
**
** 	Parameters:
**		x -- adm x-coordinate
**		y -- adm y-coordinate
**
**	Returns:
**		character at scr[x,y] or address thereof
**
*/

scrf(x,y)		/* return screen character function */
{
	if (x>=0 && x<over && y>=0 && y<up)
	      return (scr[y][x]);
	else
	      return (0);	/* 0 if address bad */
}

char *
scra(x,y)		/* return address of screen */
{
	if (x>=0 && x<over && y>=0 && y<up)
	      return (&scr[y][x]);
	else
	      return (&scr[0][0]);
}

scrw(x,y,c)		/* write screen function */
{
	if (x>=0 && x<over && y>=0 && y<up)
	    if (scr[y][x] >= '0' && scr[y][x] <= '9')	/* if there's a pt there */
	    	return (scr[y][x] = '*');/* put an '*' there for overlapped pts */
	    else	/* if nothing has been plotted there */
	    	return (scr[y][x] = c); /* then plot it */
	else
	      return (c);
}

/*  ISNUM -- atof with side effects.
**
**	Isnum gets characters from global string pointer p, and
**	advances the pointer in the case a floating number is
**	there.
**
**	Parameters:
**		none;
**
**	Assumes:
**		p -- global pointer to string for decomposition.
**
**	Returns:
**		0 -- no number
**		n -- string length of number
**
**	Side-effects:
**		p -- advanced by the length of number returned.
**		     not changed if no number read.
**		num -- global float cell contaning atof result.
*/

isnum(dumy)
char *dumy;
{
	char *ps = p, ch;	/*  save p */
	double div = 1., dec = 1.;/*  collecting past decimal pt */
	float sign = 1.;
	num = 0.;		/* start collecting now */
	dumy = dumy;		/* ling plug */
	while ( (ch = *p)==' ' || ch=='\t' )
	      ++p;		/* skip leading whitespace */

	if ( (ch = *p)=='+' || ch=='-' )
	      ++p, sign = (ch=='+') ? 1 : -1;	/* optional sign */

	while ( isdigit(ch = *p) )
	      ++p, num = 10.*num + (ch-'0');	/* digits */
	
	if ( (ch = *p)=='.' )
	      ++p, dec = 10.;			/* optional decimal */

	while ( isdigit(ch = *p) )		/* more digits */
	      ++p, div *= dec, num = num*10. + (ch-'0');

	if ( p==ps )
	      return ((p = ps),0);	/* null, or junk follows number */

	num = sign*num/div;
	return (p-ps);
}

double
getval(cp)
struct coors *cp;
{
	if (cp->xc>0.0)		/*  check for val from column */
	      return ( col( (int) cp->xc, p) );
	else
	      {
		double tmp; int i;
		tmp = cp->xv[0];	/*  else val from sequence  */
		for(i=0;i<4;i++)	/*  step sequence forward  */
		      cp->xv[i] += cp->xv[i+1];	
		return (tmp);
	      }
}
