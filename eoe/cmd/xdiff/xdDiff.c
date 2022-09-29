/****************************************************************************/
/*                                 xdDiff.c                                 */
/****************************************************************************/

/*
 *  Functionality to process the output of diff and produce a list
 *  containing the two files with flags to indicate what the
 *  differences were.
 */

/*
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  Copyright (c) 1994 Rudy Wortel. All rights reserved.
 */

/*
 *  $Source: /proj/irix6.5.7m/isms/eoe/cmd/xdiff/RCS/xdDiff.c,v $
 *  $Revision: 1.2 $
 *  $Author: blean $
 *  $Date: 1995/02/27 22:42:22 $
 */

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <xdDiff.h>

/****************************************************************************/

XDdiff *xdFindDiff (

/*
 *  Return a pointer to the difference whose index is equal to the
 *  given index.
 */

XDlist *diffs,                  /*< Look in this list.                      */
int index                       /*< Find the entry for this index.          */
)
{ /**************************************************************************/

XDdiff *diff;

    diff = diffs->first;
    while( diff != NULL && diff->index < index ){
        diff = diff->next;
    }

    return diff;
}

/****************************************************************************/

XDdiff *xdAnyUnselected (

/*
 *  Checks to see if there are any differences that remain unselected.
 */

XDlist *diffs                   /*< Look in this list.                      */
)
{ /**************************************************************************/

XDdiff *diff;

    diff = diffs->first;
    while( diff != NULL ){
        if( diff->type != XD_COMMON && diff->selection == XD_SELECTNONE ){
            break;
        }
        diff = diff->next;
    }

    return diff;
}

/****************************************************************************/

int xdAllocDiff (

/*
 *  Create a new difference entry. Return 0 when read error occurs.
 */

XDlist *diffs,                  /*| Difference list being built.            */
int index,                      /*< Overall line index.                     */
int type,                       /*< Flag the entry with this type.          */
FILE *left,                     /*< File pointer to the left file.          */
int lnuml,                      /*< Left line number.                       */
FILE *right,                    /*< File pointer to the right file.         */
int lnumr                       /*< Right line number.                      */
)
{ /**************************************************************************/

char text[ 10000 ];
XDdiff *diff;
char *p;

    diff = malloc( sizeof (XDdiff) );
    if( diff == NULL ){
        fprintf( stderr, "%s\n", "Out of memory... Bye!" );
        exit( 1 );
    }

    diff->index = index;
    diff->type = type;
    diff->lnuml = lnuml;
    diff->lnumr = lnumr;
    diff->prev = NULL;
    diff->next = NULL;
    diff->selection = XD_SELECTNONE;
    diff->flags = 0;

    diff->right = NULL;
    if( right != NULL && fgets( text, (sizeof text) - 1, right ) != NULL ){
        p = text;
        while( *p && *p != '\n' ){
            ++p;
         }
        *p = '\0';
        diff->right = strdup( text );
    }

    diff->left = NULL;
    if( left != NULL && fgets( text, (sizeof text) - 1, left ) != NULL ){
        p = text;
        while( *p && *p != '\n' ){
            ++p;
        }
        *p = '\0';
        diff->left = strdup( text );
    }

    if( diff->left == NULL && diff->right == NULL ){
        free( diff );
        diff = NULL;
    }else{
        /* put it into the list */
        if( diffs->first == NULL ){
            diffs->first = diff;
            diffs->last = diff;
            diffs->current = diff;
        }else{
            diffs->last->next = diff;
            diff->prev = diffs->last;
            diffs->last = diff;
        }
        ++diffs->lines;
    }

    return (diff == NULL) ? 0 : 1;
}

/****************************************************************************/

int xdDiffFiles (

/*
 *  Reads the output of the diff pipe and flags the lines read in
 *  from the two files. When lines are missing from a file this
 *  also adds null lines to take their place.
 */

XDlist *diffs,                  /*| Difference structure being built.       */
FILE *left,                     /*< File pointer to the left file.          */
FILE *right,                    /*< File pointer to the right file.         */
FILE *fpd                       /*< File pointer for reading diff output.   */
)
{ /**************************************************************************/

char diff[ 1000 ];              /* Buffer a line of output of diff.         */
int difference;                 /* What kind of difference.                 */
int leftStart, leftEnd;         /* Left line ranges from the diff output.   */
int rightStart, rightEnd;       /* Right line ranges from the diff output.  */
char *type;                     /* Pointer to the difference chaaracter.    */
int leftLine, rightLine;        /* Current line number for the two files.   */
int differences;                /* How many differences were found.         */
int index;                      /* Overall line index.                      */

    leftLine = 0;
    rightLine = 0;
    differences = 0;
    index = 1;

     while( fgets( diff, (sizeof diff) - 1, fpd ) ){
        /*
         *  We only need to look at the lines that look like
         *  <leftRange> [dac] <rightRange>
         */
        if( isdigit( diff[ 0 ] ) && (type = strpbrk( diff, "dac" )) != NULL ){

            ++differences;

            /* Get the left range. */
            if( sscanf( diff, "%d,%d", &leftStart, &leftEnd ) == 1 ){
                leftEnd = leftStart;
            }

            /* What kind of difference. */
            difference = *type++;

            /* Get the right range. */
            if( sscanf( type, "%d,%d", &rightStart, &rightEnd ) == 1 ){
                rightEnd = rightStart;
            }

            switch( difference ){
            case 'd':
                /* read the common part */
                while( rightLine < rightStart ){
                    xdAllocDiff( diffs, index++, XD_COMMONTEXT,
                            left, ++leftLine, right, ++rightLine );
                }
                /* Flag lines leftStart to leftEnd as left file only. */
                while( leftLine < leftEnd ){
                    xdAllocDiff( diffs, index++, XD_LEFTTEXT,
                            left, ++leftLine, NULL, rightLine );
                }
                break;
            case 'a':
                /* read the common part */
                while( leftLine < leftStart ){
                    xdAllocDiff( diffs, index++, XD_COMMONTEXT,
                            left, ++leftLine, right, ++rightLine );
                }
                /* flag lines rightStart to rightEnd as right file only */
                while( rightLine < rightEnd ){
                    xdAllocDiff( diffs, index++, XD_RIGHTTEXT,
                            NULL, leftLine, right, ++rightLine );
                }
                break;
            case 'c':
                /* read the common part */
                while( (leftLine + 1) < leftStart ){
                    xdAllocDiff( diffs, index++, XD_COMMONTEXT,
                            left, ++leftLine, right, ++rightLine );
                }
                /*
                 *  flag lines leftStart to leftEnd and rightStart to
                 *  rightEnd as changed.
                 */
                while( (leftLine < leftEnd) || (rightLine < rightEnd) ){
                    if( (leftLine < leftEnd) && (rightLine < rightEnd) ){
                        /* read from both files */
                        xdAllocDiff( diffs, index++, XD_CHANGEDTEXT,
                                left, ++leftLine, right, ++rightLine );
                    }else if( leftLine < leftEnd ){
                        /* read only from the left */
                        xdAllocDiff( diffs, index++, XD_CHANGEDTEXT,
                                left, ++leftLine, NULL, rightLine );
                     }else if( rightLine < rightEnd ){
                        /* read only from the right */
                        xdAllocDiff( diffs, index++, XD_CHANGEDTEXT,
                                NULL, leftLine, right, ++rightLine );
                    }
                }
                break;
            }
        }
    }

    /* Read the rest of the two files. */
    while( xdAllocDiff( diffs, index, XD_COMMONTEXT,
            left, ++leftLine, right, ++rightLine ) ){
        ++index;
    }

    diffs->llines = leftLine - 1;
    diffs->rlines = rightLine - 1;
    sprintf( diff, "%d", diffs->lines );
    diffs->digits = strlen( diff );

    return differences;
}

/****************************************************************************/

char *xdMakeTemporaryFile (

/*
 *  Copy the data from the input file pointer and save it in a
 *  temporary file.  Return the name of this file. Returns a pointer
 *  to a filename which is stored in a static area that will be over
 *  written on subsequent calls.
 */

FILE *ifp                       /*< Input file pointer.                     */
)
{ /**************************************************************************/

static char tempFile[ 1000 ];
char *tmpDir;
FILE *ofp;
int nread;
char iobuf[ BUFSIZ ];

    if( (tmpDir = getenv( "TMPDIR" )) == NULL ){
        tmpDir = "/usr/tmp";
    }
    sprintf( tempFile, "%s/xdiffXXXXXX", tmpDir );
    (void) mktemp( tempFile );
    if( ifp != NULL && (ofp = fopen( tempFile, "w" )) != NULL ){
        while( (nread = fread( iobuf, sizeof (char), BUFSIZ, ifp )) > 0 ){
            if( fwrite( iobuf, sizeof (char), nread, ofp ) != nread ){
                fclose( ofp );
                unlink( tempFile );
                return NULL;
            }
        }
        fclose( ofp );
    }else{
        return NULL;
    }

    return tempFile;
}

/****************************************************************************/

int xdBuildDiffs (

/*
 *  Take the two file names in the XDlist structure and opens them and
 *  invokes diff with these as arguments.  Calls the function above to read
 *  the files and build the lists.  A file name of NULL means stdin.
 */

XDlist *diffs                   /*| Difference structure being built.       */
)
{ /**************************************************************************/

char str[ 1000 ];
char *copy;
char *leftFile, *rightFile;
FILE *diffFp;
FILE *fp1;
FILE *fp2;
int differences;
int status;
#ifdef __RedoDiff__
XDdiff *diff;			/* TO index through the diffs list */
#endif /* __RedoDiff__ */

    fp1 = NULL;
    fp2 = NULL;
    copy = NULL;

    status = 0;

#ifdef __RedoDiff__
    /* If not the first time, free the memory allocted the previous time */
    if ( diffs->first ) {
      /* Free allocated memory */
        for (diff = diffs->first; diff != NULL; diff = diff->next) free(diff);
        for (diff = diffs->last;  diff != NULL; diff = diff->next) free(diff);
      /* Cancel pointers to items we just freed */
	diffs->first = diffs->last = diffs->current = NULL;
      /* Flag no lines exist. */
	diffs->lines = diffs->rlines = diffs->llines = 0;
    }
#endif /* __RedoDiff__ */

    /* check the path names for directories */
    if( diffs->lfile == NULL ){
        if( (copy = xdMakeTemporaryFile( stdin )) != NULL ){
            fp1 = fopen( copy, "r" );
        }
        leftFile = copy;
    }else{
        fp1 = fopen( diffs->lfile, "r" );
        leftFile = diffs->lfile;
    }

    if( diffs->rfile == NULL ){
        if( (copy = xdMakeTemporaryFile( stdin )) != NULL ){
            fp2 = fopen( copy, "r" );
        }
        rightFile = copy;
    }else{
        fp2 = fopen( diffs->rfile, "r" );
        rightFile = diffs->rfile;
    }

    if( fp1 != NULL && fp2 != NULL ){
        sprintf( str, "diff %s%s%s %.400s %.400s",
                (diffs->flags & XD_IGN_TRAILING) ? "-w " : "",
                (diffs->flags & XD_IGN_WHITESPACE) ? "-b " : "",
                (diffs->flags & XD_IGN_CASE) ? "-i " : "",
                leftFile, rightFile );
        if( (diffFp = popen( str, "r" )) != NULL ){
            differences = xdDiffFiles( diffs, fp1, fp2, diffFp );
            status = pclose( diffFp );
        }
    }

    if( fp1 != NULL ){
        fclose( fp1 );
    }else{
         fprintf( stderr, "xdiff: Couldn't open '%s'\n", leftFile );
        status = -1;
    }

    if( fp2 != NULL ){
        fclose( fp2 );
    }else{
        fprintf( stderr, "xdiff: Couldn't open '%s'\n", rightFile );
        status = -1;
    }

    if( copy != NULL ){
        unlink( copy );
    }

    if( status == -1 ){
        return status;
    }else{
        return differences;
    }
}

/****************************************************************************/

int xdWriteFile (

/*
 *  Write the selected differences to the given file name. If the file
 *  name is NULL then write to stdout.
 */

char *fileName,                 /*< Write to this file.                     */
XDlist *diffs,                  /*< List of differences.                    */
int side                        /*< Which side to take for common text.     */
)
{ /**************************************************************************/

FILE *ofp;
XDdiff *diff;
int status;

    status = 0;

    if( fileName == NULL ){
        ofp = stdout;
    }else{
        ofp = fopen( fileName, "w" );
    }

    if( ofp != NULL ){
        for( diff = diffs->first; diff != NULL; diff = diff->next ){
            if( diff->type == XD_COMMON ){
                if( side == XD_LEFTTEXT ){
                    status = fprintf( ofp, "%s\n", diff->left );
                }else{
                    status = fprintf( ofp, "%s\n", diff->right );
                }
            }else if( diff->selection == XD_SELECTLEFT ){
                if( diff->left != NULL ){
                    status = fprintf( ofp, "%s\n", diff->left );
                }
            }else if( diff->selection == XD_SELECTRIGHT ){
                if( diff->right != NULL ){
                    status = fprintf( ofp, "%s\n", diff->right );
                }
            }else if( diff->selection == XD_SELECTNONE ){
                if( diff->left != NULL ){
                    status = fprintf( ofp, "<:%s\n", diff->left );
                }
                if( diff->right != NULL ){
                    status = fprintf( ofp, ">:%s\n", diff->right );
                }
            }
            if( status == EOF ){
                break;
            }
        }
    }else{
        status = EOF;
    }

    if( fileName != NULL ){
        fclose( ofp );
    }

    return status;
}

/****************************************************************************/

void xdSplitDifferences (

/*
 *  If a range of differences is of type XD_CHANGEDTEXT then this will split
 *  it into two ranges where one is XD_LEFTTEXT and the other is
 *  XD_RIGHTTEXT.  The side argument chooses which side is listed first. The
 *  selection is retained after the split.
 */

XDlist *diffs,                  /*< List of differences.                    */
int side                        /*< Which side is listed first.             */
)
{ /**************************************************************************/

XDdiff *leftDiff;
XDdiff *rightDiff;
XDdiff *firstDiff;
XDdiff *topDiff;
XDdiff *bottomDiff;
XDdiff *prevDiff;
XDdiff *newDiff;
int lline;
int rline;
int added;
int index;

    topDiff = diffs->current;
    bottomDiff = topDiff;
    if( topDiff->type != XD_CHANGEDTEXT ){
        return;
    }

    /* go to the top of the region */
    while( topDiff->prev != NULL && topDiff->prev->type == XD_CHANGEDTEXT ){
        topDiff = topDiff->prev;
    }

    /* find the bottom of the region */
    while( bottomDiff->next != NULL &&
            bottomDiff->next->type == XD_CHANGEDTEXT ){
        bottomDiff = bottomDiff->next;
    }

    rightDiff = topDiff;
    prevDiff = NULL;
    added = 0;

     if( side == XD_LEFTTEXT ){
        lline = bottomDiff->lnuml;
        rline = topDiff->lnumr - 1;
    }else{
        lline = topDiff->lnuml - 1;
        rline = bottomDiff->lnumr;
    }

    /* For each line in the region make a new entry for the left side */
    while( rightDiff != NULL && rightDiff->type == XD_CHANGEDTEXT ){
        leftDiff = malloc( sizeof (XDdiff) );
        if( leftDiff == NULL ){
            fprintf( stderr, "%s\n", "Out of memory... Bye!" );
            exit( 1 );
        }

        /* Split the information between the two differences */
        leftDiff->lnuml = rightDiff->lnuml;
        leftDiff->lnumr = rline;
        rightDiff->lnuml = lline;

        leftDiff->left = rightDiff->left;
        leftDiff->right = NULL;
        rightDiff->left = NULL;

        leftDiff->type = XD_LEFTTEXT;
        rightDiff->type = XD_RIGHTTEXT;

        leftDiff->flags = rightDiff->flags & ~XD_MATCHED_RIGHT;
        rightDiff->flags &= ~XD_MATCHED_LEFT;

        leftDiff->selection = rightDiff->selection;
        leftDiff->index = rightDiff->index;

        /* link up the new differences */
        if( prevDiff == NULL ){
            firstDiff = leftDiff;
        }else{
            prevDiff->next = leftDiff;
        }
        leftDiff->prev = prevDiff;
        leftDiff->next = NULL;
        prevDiff = leftDiff;

        rightDiff = rightDiff->next;
        ++added;
    }

    /* new range is bounded by firstDiff and leftDiff */
    /* old range is bounded by topDiff and bottomDiff */
    if( side == XD_LEFTTEXT ){
        /* insert the new differences before topDiff */
        if( topDiff->prev == NULL ){
            diffs->first = firstDiff;
        }else{
            topDiff->prev->next = firstDiff;
        }
        leftDiff->next = topDiff;
        firstDiff->prev = topDiff->prev;
        topDiff->prev = leftDiff;
    }else{
        /* insert the new differences after bottomDiff */
        if( bottomDiff->next == NULL ){
            diffs->last = leftDiff;
        }else{
            bottomDiff->next->prev = leftDiff;
         }
        leftDiff->next = bottomDiff->next;
        firstDiff->prev = bottomDiff;
        bottomDiff->next = firstDiff;
    }

    /* adjust the index values to compensate for the new entries */
    index = topDiff->index;
    topDiff = diffs->first;
    index = 1;
    while( topDiff != NULL ){
        topDiff->index = index;
        ++index;
        topDiff = topDiff->next;
    }

    diffs->lines += added;
}
