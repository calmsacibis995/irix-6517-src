/****************************************************************************/
/*                              xdDefault.c                                 */
/****************************************************************************/

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
 *  $Source: /proj/irix6.5.7m/isms/eoe/cmd/xdiff/RCS/xdDefault.c,v $
 *  $Revision: 1.1 $
 *  $Author: dyoung $
 *  $Date: 1994/09/02 21:21:29 $
 */

#include <X11/Intrinsic.h>

typedef struct {                    /* Have to do my own fallback resources!*/
    char            *resource;      /* Resource name.                       */
    char            *value;         /* Default value.                       */
} xdDefault;

static xdDefault xdFallbacks[] = {
    {   "fontCommon",           "fixed"                 },
    {   "fgcCommon",            "black"                 },
    {   "bgcCommon",            "white"                 },
    {   "fgcOnly",              "white"                 },
    {   "bgcOnly",              "black"                 },
    {   "fgcAbsent",            "white"                 },
    {   "bgcAbsent",            "black"                 },
    {   "fgcChanged",           "white"                 },
    {   "bgcChanged",           "black"                 },
    {   "fgcSelected",          "black"                 },
    {   "bgcSelected",          "white"                 },
    {   "fgcDeleted",           "white"                 },
    {   "bgcDeleted",           "black"                 },

    {   "msgWriteFailed",       "Write failed."         },
    {   "msgWriteSucceeded",    "Write succeded."       },
    {   "msgNoSelection",       "No selection here."    },
    {   "msgClickWidget",       "Click in a widget."    },
    {   "msgSplit",             "Can't split that."     },
    {   "msgOkSave",            "Ok to save"            },
};

/****************************************************************************/

char *xdGetDefault (

/*
 *  Replacement for XtGetDefault that will return a fallback if the
 *  search in the application defaults fails.
 *
 *  Handle fallback resources manually since IRIX will not pickup the
 *   fallbacks if an application default file exists.
 */

Display *display,               /*< Pass this on to XGetDefault.            */
char *appClass,                 /*< Pass this on to XGetDefault.            */
char *resourceName              /*< Look up this resource.                  */
)
{ /**************************************************************************/

char *resourceValue;
int fallback;

    /* first go through normal channels */
    resourceValue = XGetDefault( display, appClass, resourceName);

    if( resourceValue != NULL ){
        return resourceValue;
    }

    /* Not there so look in our table */
    for( fallback = 0; fallback < XtNumber( xdFallbacks ); ++fallback ){
        if( strcmp( resourceName, xdFallbacks[ fallback ].resource ) == 0 ){
            return xdFallbacks[ fallback ].value;
        }
    }

    /* Not there either. */
    return NULL;
}
