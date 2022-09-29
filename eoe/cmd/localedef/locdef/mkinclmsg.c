/* @[$] mkinclmsg.c 1.0 frank@ceres
 * make message include file
 *
 * mkinclmsg ifile msgsfile incl_file catalog_name
 */

#include	<stdio.h>
#include	<string.h>

#define	PREFIX_DEF	"_MSG_"
char prefix[] = "";

#define	SIZELINE	16384
#define USAGE		"Usage: %s [ -x | -a ] <input_file> <header_file> <catalog_file> <catalog_name>\n"
char line[SIZELINE];

/* 
 * main function
 */
int main(argc, argv)
    int argc;
    char **argv;
{
    int lnbr = 0;
    int msgnbr = 0;
    char set_id [ 256 ];
    char * msg_id;
    char * msg;
    char *s;
    char * input_fn;
    char * header_fn;
    char * cat_fn;
    char * cat_name;

    FILE * input, *header, *cat;

    int setnbr = 1;
    int n;
    char quote_char = 0;
    char c;
    int is_xpg4 = 1;

    if(argc != 6) {
	fprintf ( stderr, USAGE, argv[0] );
	exit(1);
    }
    if ( !strcmp ( argv[1], "-x" ) )
	is_xpg4 = 1;
    else if ( !strcmp ( argv[1], "-a" ) )
	is_xpg4 = 0;
    else {
	fprintf ( stderr, USAGE, argv[0] );
	exit(1);
    }

    input_fn = argv[2];
    header_fn = argv[3];
    cat_fn = argv[4];
    cat_name = argv[5];

    if ( !(input = fopen( input_fn, "r"))) {
	printf ("%s: Cannot open input file '%s'\n", argv[0], input_fn );
	exit(2);
    }
    
    if ( !(header = fopen( header_fn, "w"))) {
	printf ("%s: Cannot open header output file '%s'\n", argv[0], header_fn );
	exit(2);
    }

    if ( !(cat = fopen( cat_fn, "w"))) {
	printf ("%s: Cannot open catalog output file '%s'\n", argv[0], cat_fn );
	exit(2);
    }

    while(fgets(line, SIZELINE, input)) {
	lnbr++;

	if ((s = strrchr(line, '\n')) != NULL) 
	    *s = '\0';
	else
	    fprintf ( stderr, "Warning: truncating line %d\n", lnbr );
      
	if ( is_xpg4 )
	{
	    if ( strspn ( line, " \t\n" ) == strlen ( line ) )
		/* Empty line */
		continue;

	    /* XPG4 FORMAT */
	    if ( ! strncmp ( line, "$ ", 2 ) )
		/* A comment */
		fprintf ( cat, "%s\n", line );
	    else if ( sscanf ( line, "$quote %c", &c ) == 1 )
		/* New quoting character */
		quote_char = c;
	    else if ( sscanf ( line, "$quote\n" ) == 1 )
		/* No quoting character */
		quote_char = 0;

	    else if ( sscanf ( line, "$set %s\n", set_id ) == 1 ) {
		/* New set directive */
		if ( atoi ( set_id ) != 0 )
		    /* Numeric set */
		    setnbr = atoi ( set_id );
		else
		{
		    setnbr ++;
		    fprintf ( header, "#define\t%s%s_SET_NUM\t%d\n", prefix, set_id, setnbr );
		}
		msgnbr = 0;
		fprintf ( cat, "$set %d\n", setnbr );
	    }
	    else if ( s = strchr ( line, ' ' ) )
	    {
		*s++ = '\0';
		msg_id = line;
		msg = s;

		if ( atoi ( msg_id ) != 0 )
		    /* Numeric msg id */
		    msgnbr = atoi ( msg_id );
		else
		{
		    msgnbr ++;

		    fprintf ( header, "#define\t%s%s_MSG_NUM\t%d\n",
			      prefix, msg_id, msgnbr );

		    fprintf ( header, "#define\t%s%s_MSG_SET_NUM\t%d\n",
			      prefix, msg_id, setnbr );

		    fprintf ( header, "#define\t%s%s_DEF_MSG\t\"%s\"\n",
			      prefix, msg_id, msg );
		}
		fprintf ( cat, "%d %s\n", msgnbr, msg );

	    }
	    else
	    {
		printf("%s: line %d - syntax error\n", input_fn, lnbr);
		exit(5);
	    }
	}
	else
	{
	    int d;
	    /* AT&T FORMAT */

	    if ( strspn ( line, " \t\n" ) == strlen ( line ) )
		/* Empty line */
		continue;

	    if ( line[0] == '#' )
		/* A comment */
		continue;
	    else if ( s = strchr ( line, ':' ) )
	    {
		*s++ = '\0';
		msg_id = line;
		msg = s;

		msgnbr ++;

		fprintf ( header, "#define\t%s%s_MSG_NUM\t%d\n",
			  prefix, msg_id, msgnbr);
		fprintf ( header, "#define\t%s%s_MSG_NUM_STR\t\":%d\"\n",
			  prefix, msg_id, msgnbr);
		fprintf ( header, "#define\t%s%s_CAT_NUM_STR\t\"%s:%d\"\n",
			  prefix, msg_id, cat_name, msgnbr);
		fprintf ( header, "#define\t%s%s_DEF_MSG\t\"%s\"\n",
			  prefix, msg_id, msg );
                /* HCL addtion starts from here*/
                fprintf ( header, "#define\t%s%s_CAT_NUM_STR_DEF_MSG\t\"%s:%d:%s\"\n",
                          prefix, msg_id, cat_name, msgnbr, msg );
                fprintf ( header, "#define\t%s%s_MSG_NUM_STR_DEF_MSG\t\":%d:%s\"\n",
                          prefix, msg_id, msgnbr, msg );
                /* HCL additions ends Here */

		fprintf ( cat, "%s\n", msg );
	    }
	    else {
		printf("%s: line %d - syntax error\n", input_fn, lnbr);
		exit(5);
	    }
	}


    }

}

