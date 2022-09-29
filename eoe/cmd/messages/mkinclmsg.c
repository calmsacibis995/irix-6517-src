/* @[$] mkinclmsg.c 1.0 frank@ceres
 * make message include file
 *
 * mkinclmsg ifile msgsfile incl_file catalog_name
 */

#include	<stdio.h>
#include	<string.h>

#define	SIZELINE	16384

char line[SIZELINE];
char cline[SIZELINE];
char inclfile[4096];

/* 
 * Substitues double-quote with backslash double-quote
 */
void transform ( char * in, char * out )
{
    char * i = in;
    while ( *i )
    {
	if ( *i == '"' )
	    if ( i == in || *(i-1) != '\\' )
		*out++ = '\\';

	*out++ = *i++;
    }
    *out = '\0';

}

/* 
 * main function
 */
int main(argc, argv)
    int argc;
    char **argv;
{
    register int lnbr = 0;
    register int msgnbr = 0;
    register char *s;
    FILE *infile, *mf, *of;
    char msgnbr_str [ 256 ];
    char * id;
    char * catalog_name = 0;

    if(argc != 5) {
	printf("%s: Illegal argc\n", argv[0]);
	printf ( "Usage: %s <source> <output> <header> <catalogname>\n", argv[0] );
	exit(1);
    }
    if (!(infile = fopen(argv[1], "r"))) {
	printf("%s: Cannot open input file '%s'\n", argv[0], argv[1]);
	exit(2);
    }
    if (!(mf = fopen(argv[2], "w"))) {
	printf("%s: Cannot open msgs output file '%s'\n",
	       argv[0], argv[2]);
	exit(3);
    }
    sprintf(inclfile, "%s.h", argv[3]);
    if (!(of = fopen(inclfile, "w"))) {
	printf("%s: Cannot open include output file '%s'\n", argv[0], inclfile);
	exit(4);
    }

    catalog_name = argv[4];

    /*
     * write header to message file
     */
    while(fgets(line, SIZELINE, infile)) {
	lnbr++;
	if ((s = strrchr(line, '\n')) != NULL) {
	    *s = '\0';
	}
	if (line[0] == '#')
	    /* a comment */
	    continue;

	else {
	    s = strchr(line, ':');
	    if (!s) {
		printf("%s: line %d - syntax error\n", argv[1], lnbr);
		exit(5);
	    }
	    *s++ = 0;
	    msgnbr++;

	    if ( *line == '\0' )
	    {
		/* Anonymous message, no identifier specified */
		sprintf ( msgnbr_str, "%s_%d", catalog_name, msgnbr );
		id = msgnbr_str;
	    }
	    else
		id = line;

	    fprintf(of, "#define\t_SGI_D%s\t\":%d\"\n",
		    id, msgnbr);
	    fprintf(of, "#define\t_SGI_%s\t\"%s:%d\"\n",
		    id, catalog_name, msgnbr);
	    
	    transform ( s, cline );
	    
	    fprintf(of, "#define\t_SGI_M%s\t\"%s:%d:%s\"\n",
		    id, catalog_name, msgnbr, cline);
	    fprintf(of, "#define\t_SGI_S%s\t\"%s\"\n",
		    id, cline);
	}
	if(fputs(s, mf) == EOF || fputc('\n', mf) == EOF)
	    goto errorwrite;
    }
    return(0);

 errorwrite:
    printf("%s: Cannot write msgs output file '%s'\n", argv[0], argv[2]);
    exit(6);
}
