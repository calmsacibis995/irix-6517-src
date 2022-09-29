/****************************************************************************/
/*                                  xdiff                                   */
/****************************************************************************/

/*
 *  Produce a graphical difference of two files.
 *
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
 *  $Source: /proj/irix6.5.7m/isms/eoe/cmd/xdiff/RCS/xdDiff.h,v $
 *  $Revision: 1.2 $
 *  $Author: blean $
 *  $Date: 1995/02/27 22:42:24 $
 */

#include <string.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/stat.h>

#define XD_COMMON           0   /* Attributes for common lines.             */
#define XD_ONLY             1   /* Attributes for lines only in one file.   */
#define XD_ABSENT           2   /* Attributes for lines absent in one file. */
#define XD_CHANGED          3   /* Attributes for changed lines.            */
#define XD_SELECTED         4   /* Attributes for selected lines.           */
#define XD_DELETED          5   /* Attributes for deleted lines.            */
#define XD_MATCHED

/* Difference states */
#define XD_COMMONTEXT       0   /* This line is common between files.       */
#define XD_LEFTTEXT         1   /* This line appears only on the left.      */
#define XD_RIGHTTEXT        2   /* This line appears only on the right.     */
#define XD_CHANGEDTEXT      3   /* This line has changed between files.     */

/* Difference flags */
#define XD_MATCHED_LEFT     0x0001  /* Pattern matched the left text.       */
#define XD_MATCHED_RIGHT    0x0002  /* Pattern matched the right text.      */
#define XD_MATCHED_MASK     0x0003  /* Mask for the above flags.            */

/* Selection states */
#define XD_SELECTNONE       0   /* Haven't decided yet.                     */
#define XD_SELECTLEFT       1   /* Want the left file text                  */
#define XD_SELECTRIGHT      2   /* Want the right file text                 */
#define XD_SELECTNEITHER    4   /* Don't want either side.                  */

/* Command line flags */
#define XD_IGN_TRAILING     0x0001  /* Ignore trailing white space.         */
#define XD_IGN_WHITESPACE   0x0002  /* Ignore all white space.              */
#define XD_IGN_CASE         0x0004  /* Ignore case.                         */
#define XD_EXIT_NODIFF      0x0008  /* Exit if no differences.              */

#define XD_LEFT_WRITABLE    0x0010  /* Is the left file writable.           */
#define XD_RIGHT_WRITABLE   0x0020  /* Is the right file writable.          */

typedef struct xddiff {
    int             index;      /* Overall line index.                      */
    struct xddiff   *prev;      /* Linked list                              */
    struct xddiff   *next;      /* Linked list                              */
    char            *left;      /* Text for the left file.                  */
    char            *right;     /* Text for the right file.                 */
    int             lnuml;      /* Line number for the left file.           */
    int             lnumr;      /* Line number for the right file.          */
    int             type;       /* What was different.                      */
    int             selection;  /* What you want to keep.                   */
    int             flags;      /* Various flags.                           */
} XDdiff;

typedef struct xdlist {
    struct xddiff   *first;     /* First item in the list.                  */
    struct xddiff   *last;      /* Last item in the list.                   */
    struct xddiff   *current;   /* Current interest point.                  */
    int             lines;      /* Number of lines in the difference list.  */
    char            *lfile;     /* Left file name.                          */
    char            *rfile;     /* Right file name.                         */
    int             llines;     /* Number of lines in the left file.        */
    int             rlines;     /* Number of lines in the right file.       */
    int             flags;      /* Flags to use when calling diff(1).       */
    int             digits;     /* How much room needed for a line number.  */
} XDlist;

/*
 * The symbol __RedoDiff__ turns on the added "Redo Diff" menu choice.
 * It is all ifdef'd so that if there are any problems it will be easy to
 * spot the changes made to implement it.  It is *not* expected to be
 * turned off, though xdiff should build fine even if it is turned off.
 */
#ifndef __RedoDiff__
#define __RedoDiff__
#endif
