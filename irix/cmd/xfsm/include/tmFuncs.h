
#ifndef _TM_FUNCS_H
#define _TM_FUNCS_H

#include "tm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Display related functions */
extern void Tm_SetAppContextInfo _ANSI_ARGS_((XtAppContext c, String appname,
	    String appclass));
extern Tm_Display *Tm_OpenDisplay _ANSI_ARGS_((char *dispname));
extern Tm_Display *Tm_AllocateDisplay _ANSI_ARGS_((void));
extern void Tm_DisplayAddShell _ANSI_ARGS_ ((Tm_Display *d,Tm_Widget *p));
extern void Tm_DisplayRemoveShell _ANSI_ARGS_ ((Tm_Display *d,Tm_Widget *p));

/* Widget creation functions */
extern int Tm_AnyCmd _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
            int argc, char *argv[]));
extern int Tm_ScrolledCmd _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
            int argc, char *argv[]));
extern int Tm_HasChildrenCmd _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
            int argc, char *argv[]));
extern int Tm_DialogCmd _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
            int argc, char *argv[]));
extern int Tm_RowColumnCmd _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
            int argc, char *argv[]));
extern int Tm_TopLevelShellCmd _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
            int argc, char *argv[]));

#if XmVERSION >= 2
extern int Tm_ComboBoxCmd _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
            int argc, char **argv));
#endif

/* widget creation functions */
extern int Tm_MrmFetchWidget _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
	     int argc, char **argv));
extern int Tm_MrmFetchWidgetOverride _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
	     int argc, char **argv));

/* wdiget specific commands */
extern int Tm_AnyWidgetCmd _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
	     int argc, char **argv));
extern int Tm_CommandWidgetCmd _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
	     int argc, char **argv));
extern int Tm_DrawnWidgetCmd _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
	     int argc, char **argv));
extern int Tm_ListWidgetCmd _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
	     int argc, char **argv));
extern int Tm_TextWidgetCmd _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
	     int argc, char **argv));
extern int Tm_PopupMenuWidgetCmd _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
	     int argc, char **argv));
extern int Tm_RootCmd _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
	     int argc, char **argv));
extern int Tm_ShellWidgetCmd _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
	     int argc, char **argv));

#if XmVERSION >= 2
extern int Tm_CSTextWidgetCmd _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
	     int argc, char **argv));
extern int Tm_NotebookWidgetCmd _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
	     int argc, char **argv));
#endif

/* send commands */
extern int Tm_SendCmd _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
            int argc, char **argv));
extern int Tm_RegisterInterp _ANSI_ARGS_((Tcl_Interp *interp, char *name,
	    Tm_Display *dispPtr));

/* other commands */
extern int Tm_XEvent _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
            int argc, char **argv));

/* drag and drop functions */
extern void Tm_DropSiteSetValues _ANSI_ARGS_((char *path, Tcl_Interp *interp,
             Widget w, char **argv, int argc, Arg args[], int *num_args));
extern void Tm_DropProcHandler _ANSI_ARGS_ ((Widget w, XtPointer client_data,
	    XtPointer call_data));
extern void Tm_DropTransferHandler _ANSI_ARGS_ ((Widget w, XtPointer closure,
	    Atom *seltype, Atom *type, XtPointer value,
	    unsigned long *length, int *format));
extern Boolean Tm_ConvertProcHandler _ANSI_ARGS_ ((Widget w, Atom *selection,
	    Atom *target, Atom *type, XtPointer *value, unsigned long *length,
	     int *format));
extern int Tm_InstallDropType _ANSI_ARGS_ ((Tcl_Interp *interp,
	    char *typeStr, char *(*converter) ()));
	    
/* result capturing */
extern int Tm_SaveResult _ANSI_ARGS_ ((Tcl_Interp *interp));
extern void Tm_ClearResult _ANSI_ARGS_ ((Tcl_Interp *interp));
extern void Tm_AppendResult _ANSI_ARGS_ ((Tcl_Interp *interp, char *str));
extern void Tm_StartSavingResult _ANSI_ARGS_ ((Tcl_Interp *interp));
extern void Tm_StopSavingResult _ANSI_ARGS_ ((Tcl_Interp *interp));
extern char  *Tm_Result _ANSI_ARGS_ ((Tcl_Interp *interp));

/* callback handlers */
extern void Tm_WidgetCallbackHandler _ANSI_ARGS_((Widget w, 
	     XtPointer client_data, XtPointer call_data));
extern void Tm_DestroyWidgetHandler _ANSI_ARGS_((Widget w, 
	     XtPointer client_data, XtPointer call_data));
extern void Tm_DestroyReclaimHandler _ANSI_ARGS_((Widget w, 
	     XtPointer client_data, XtPointer call_data));
extern void Tm_TextVerifyCallbackHandler _ANSI_ARGS_((Widget w, 
	     XtPointer client_data, XtPointer call_data));
extern void Tm_SelectionCallbackHandler _ANSI_ARGS_(( Widget w,
             XtPointer clientData, Atom *selection, Atom *type,
             XtPointer value, unsigned long *length, int *format));

#if XmVERSION >= 2
extern void Tm_CSTextVerifyCallbackHandler _ANSI_ARGS_((Widget w, 
	     XtPointer client_data, XtPointer call_data));
extern void Tm_DestinationCallbackHandler _ANSI_ARGS_((Widget w, 
	     XtPointer client_data, XtPointer call_data));
extern void Tm_ConvertCallbackHandler _ANSI_ARGS_((Widget w, 
	     XtPointer client_data, XtPointer call_data));
#endif
#if USE_UIL
extern void Tm_UILCallbackHandler _ANSI_ARGS_((Widget w, 
	     XtPointer client_data, XtPointer call_data));
#endif

/* resource free functions */
extern void Tm_FreeResourceValues _ANSI_ARGS_((void));
extern void Tm_InitFreeResourceList _ANSI_ARGS_((int size));
extern void Tm_AddToFreeResourceList _ANSI_ARGS_((char *data,Tm_FreeProc free));
extern void Tm_FreeResourceList _ANSI_ARGS_((void));

/* other funcs */
extern void Tm_LoadWidgetCommands _ANSI_ARGS_((Tcl_Interp *interp));
extern Tm_Widget *Tm_WidgetInfoFromPath _ANSI_ARGS_((Tcl_Interp *interp,
	     char *path));
extern Widget Tm_ParentWidgetFromPath _ANSI_ARGS_((Tcl_Interp *interp,
	     char *path));
extern char *Tm_HiddenParentPath _ANSI_ARGS_((char *path));
extern char *Tm_ParentPath _ANSI_ARGS_((char *path));
extern void Tm_StoreWidgetInfo _ANSI_ARGS_((char *path, Tm_Widget *w,
	     Tcl_Interp *interp));
extern char *Tm_GetGC _ANSI_ARGS_((char *pathName, Tcl_Interp *interp,	
	     Widget w, WidgetClass Class, char **argv, int argc));
extern int Tm_GetValues _ANSI_ARGS_((char *pathName, Tcl_Interp *interp,	
	     Widget w, WidgetClass Class, char **argv, int argc));
extern int Tm_SetValues _ANSI_ARGS_((char *pathName, Tcl_Interp *interp,	
	     Widget w, Widget parent, WidgetClass Class, char **argv, int argc,
	     Arg args[], int *num_args));
extern void Tm_GetExtensionResources _ANSI_ARGS_((WidgetClass Class,
	     XtResourceList *resources, Cardinal *num_resources));
extern char *Tm_ExpandPercents _ANSI_ARGS_((char *pathName, Widget w, 
	     XEvent *event, XtPointer call_data, 
	     char *before));
extern char *Tm_NameFromPath _ANSI_ARGS_((char *pathName));
extern void Tm_ActionsHandler _ANSI_ARGS_((Widget w, XEvent *event,
	     char ** argv, Cardinal *argc));
extern void Tm_InputHandler _ANSI_ARGS_(( XtPointer clientData,
    	     int *source, XtInputId *id));
extern void Tm_TimerHandler _ANSI_ARGS_(( XtPointer clientData,
    	     XtIntervalId *id));
extern int Tm_MakeXEvent _ANSI_ARGS_((Widget w, Tcl_Interp *interp, XEvent *xev,
	     int argc, char **argv));
extern int Tm_ParseAction _ANSI_ARGS_ ((char *orig, char **action,
	     char *params[], Cardinal *num_params));
extern int Tm_ResourceList _ANSI_ARGS_ ((Tcl_Interp *interp, Widget w, 
	     WidgetClass Class));
extern int Tm_GetAppResources _ANSI_ARGS_ ((Tcl_Interp *interp, Widget w,
	     String resource_list));
extern int Tm_Init _ANSI_ARGS_ ((Tcl_Interp *interp));
extern String Tm_XmStringToString _ANSI_ARGS_ ((XmString xmstr));
extern XmString Tm_StringToXmString _ANSI_ARGS_ ((Tcl_Interp *interp,
	     char *str));
extern int Tm_CreateTmInfo _ANSI_ARGS_ (( Tcl_Interp *interp,
    	     Widget w, char *name, char *parent, Tm_Display *displayInfo));
extern void Tm_CreateTmInfoInChildren _ANSI_ARGS_ (( Tcl_Interp *interp,
    	     Widget w, char *name, Tm_Display *displayInfo));
extern void Tm_CreateTmMenuInfo _ANSI_ARGS_ (( Tcl_Interp *interp,
    	     Widget w, char *name, Tm_Display *displayInfo));


/* Converter functions */
extern Boolean Tm_CvtStringToWidget _ANSI_ARGS_((Display *display,
	      XrmValuePtr args, Cardinal *num_args,
	      XrmValuePtr fromVal, XrmValuePtr toVal,
	      XtPointer* destructor_data));
extern Boolean Tm_CvtXmStringToString _ANSI_ARGS_((Display *display,
	      XrmValuePtr args, Cardinal *num_args,
	      XrmValuePtr fromVal, XrmValuePtr toVal,
	      XtPointer* destructor_data));
extern void Tm_RegisterConverters _ANSI_ARGS_((Tcl_Interp *interp,
	      XtAppContext appContext));
extern Boolean Tm_ConvertValueFromString _ANSI_ARGS_((Widget w, 
              XtResourceList resources, int num_resources,
              char * resource, char *orig_value, XtArgVal *new_value));
extern Boolean Tm_ConvertValueFromStringQuark _ANSI_ARGS_((Widget w, 
              XtResourceList resources, int num_resources,
              char * resource, char *orig_value, XtArgVal *new_value));
extern Boolean Tm_ConvertValueToString _ANSI_ARGS_((Widget w,
	    XtResourceList resources, int num_resources,
	      char *resource, char **new_value));
extern Boolean Tm_ConvertValue _ANSI_ARGS_((Widget w,char *fromType,
					  char *fromValue,unsigned int fromSize,
					  char *toType,XtArgVal *toValue,
					  unsigned int toSize));

/* external widgets stuff */

extern void Tm_ExternWidgetsInitialise _ANSI_ARGS_ ((Tcl_Interp *interp));
extern char *Tm_ExternExpandPercent _ANSI_ARGS_ ((char *pathName,
    		Widget w, XEvent *event, XtPointer call_data,
    		char *value, int length));


#ifdef __cplusplus
}
#endif

#endif /* _TM_FUNCS_H */
