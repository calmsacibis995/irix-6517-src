#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xperform.h"

#define TOKEN			" \t\n"

const char * banyan_format = "%s/.desktop-%s/ozPanelLayout-1.00/Background" ;
const char * bonsai_format = "%s/.desktop-%s/layouts/Background" ;

typedef struct cmd_t
{
	char * name ;
	int    id ;
} CMD ;

enum { ICON_X = 1, ICON_Y, ICON_NAME } ;

int
IconParser(char *buf, char *name, int *x, int *y)
{
	int    i ;
	int    cmd ;
	char   iconName[256] ;
	char * ptr ;
	static CMD    iconCmd[] =
	{
		"ViewX:",		ICON_X,
		"ViewY:",		ICON_Y,
		"Name:",		ICON_NAME,
		"PrinterName:",	ICON_NAME
	} ;

	iconName[0] = '\0' ;
	*x   = 0 ;
	*y   = 0 ;

#ifdef DEBUG
	printf("IconParser()\n") ;
#endif

	ptr = strtok(buf, TOKEN) ;
	if( !ptr )	return 0 ;

	do
	{
		cmd = FindArg(iconCmd, ptr) ;
		switch( iconCmd[cmd].id )
		{
			case ICON_X:
				*x = atoi(ptr + strlen(iconCmd[cmd].name)) ;
				break ;

			case ICON_Y:
				*y = atoi(ptr + strlen(iconCmd[cmd].name)) ;
				break ;

			case ICON_NAME:
				strcpy(iconName, ptr + strlen(iconCmd[cmd].name)) ;
				for( i = strlen(iconName) - 1 ; i >= 0 ; i-- )
					if( iconName[i] == '/' )
					{
						i++ ;
						strcpy(iconName, iconName + i) ;
						break ;
					}
				break ;

			default:
				break ;
		}
	}
	while( ptr = (char *)strtok(NULL, TOKEN) )
		;

	if( strcmp(iconName, name) == 0 )
	{
		return 1 ;
	}
	else
		return 0 ;
}

int
GetHostName(char *hostname)
{
	FILE * fp ;

	fp = fopen("/etc/sys_id", "r") ;
	if( !fp )
	{
		strcpy(hostname, "unknown") ;
		return 0 ;
	}
	else
	{
		fscanf(fp, "%s", hostname) ;
		fclose(fp) ;
		return 1 ;
	}
}

int
FindIcon(char *name, int *x, int *y)
{
	FILE * fp ;
	int    rval = 0 ;
	char   buf[256] ;
	char   hostname[64] ;
	char   iconfile[256] ;
	enum   { BANYAN, BONSAI } os_type ;
	int    binarysize = 0;
	char   *binarybuf =0;
	char   *nameptr =0;
	int    index = 0;
	int    flag = 0;
	int    retval = 1;

#ifdef DEBUG
	printf("FindIcon (%s)\n", name) ;
#endif
	GetHostName(hostname) ;

	/* First assume this is a bonsai machine */
        sprintf(iconfile, bonsai_format, getenv("HOME"), hostname) ;

        fp = fopen(iconfile, "r") ;
        if ( fp ) os_type = BONSAI;
        if( !fp )
        {
		/* Prepare for possible later error message */
                char buff[512] ;
                sprintf(buff, "*** error: unable to open desktop files: \n") ;
                sprintf(buff + strlen(buff),  "*** %s\n", iconfile) ; 

		/* If it's not bonsai, it might be banyan */
                sprintf(iconfile, banyan_format, getenv("HOME"), hostname) ;
                fp = fopen(iconfile, "r") ;
                if ( fp ) os_type = BANYAN;
                if( !fp )
                {
			/* Neither bonsai nor banyan.  Give up */
                        sprintf(buff + strlen(buff),  "*** %s\n", iconfile) ;
                        Log(buff) ;
                        return 0 ;
                }
        }

	/* For Banyan we have Matt's code */
        if ( os_type == BANYAN ) 
        {
                while( rval = (int)fgets(buf, sizeof(buf) - 1, fp) )
                {
                        if( IconParser(buf, name, x, y) )
                                break ;
                }
                fclose(fp) ;

                if( !rval )
                {
                        char buff[256] ;
                        sprintf(buff, "*** error: unable to find icon ") ;
                        sprintf(buff + strlen(buff),  "%s\n", name) ;
                        Log(buff) ;
                        return 0 ;
                }
        }

	/* For Bonsai we have to parse binary data */
	if ( os_type == BONSAI )
	{
		/* count on the short-circuit below */
                while( binarysize == 0 && ( rval = (int)fgets(buf, sizeof(buf) - 1, fp) ) )
		{
			/* IconPositions tells us the size of the binary data */
			if( strncmp( "IconPositions = Raw", buf, strlen("IconPositions = Raw") ) 
						== 0)
			{
				binarysize = atoi(&buf[strlen("IconPositions = Raw")+1]);
			}
                }
                if( !rval )
                {
                        char buff[256] ;
                        sprintf(buff, "*** error: unable to find icon ") ;
                        sprintf(buff + strlen(buff),  "%s\n", name) ;
                        Log(buff) ;
                        return 0 ;
                }

		binarybuf = (char *) malloc( binarysize );

		/* Snag the binary data */
		rval = (int)fread(binarybuf, 1, binarysize, fp);
		if ( !rval )
		{
                        char buff[256] ;
                        sprintf(buff, "*** error: unable to find icon ") ;
                        sprintf(buff + strlen(buff),  "%s\n", name) ;
                        Log(buff) ;
                        return 0 ;
		}

		/* Step through the binary data looking for our icon */
		while( (flag == 0) && (index < binarysize) )
		{
			if( !strcmp(&binarybuf[index], name ) )
			{
				/* OUR ICON */
				flag = 1;
				index += strlen(&binarybuf[index]) + 1;
                                if( index % 2 == 1 ) index++;
				/* There HAS to be a better way */
				*x = (int)binarybuf[index] * 256;
				*x += (int)binarybuf[index+1];
				*x += 5;
				*x += strlen(name)*3; /* extra padding for long names */
				*y = (int)binarybuf[index+2] * 256;
				*y += (int)binarybuf[index+3];
				*y += 5;
			}
			else
			{
				/* Hop over unwanted icon name, padding, and X, Y values */
				index += strlen(&binarybuf[index]) + 5;
				if( index % 2 == 1 ) index++;
			}
		}
		if (flag == 0) {
			retval = 0;
			printf ("Couldn't find <%s>\n", name);
		}


	}


#ifdef DEBUG
	printf("    (%d,%d)\n", *x, *y) ;
#endif

	return retval ;
}


