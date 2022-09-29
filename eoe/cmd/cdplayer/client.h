/*
 *	client.h
 *
 *	Description:
 *		Header file for client data (passed to call-backs) for the CD Audio
 *		tool
 *
 *	History:
 *      rogerc      11/06/90    Created
 */


#define TIME_TRACK 0
#define TIME_TRACKLEFT 1
#define TIME_CD 2
#define TIME_CDLEFT 3
#define TIME_LAST	3

#define READY_NORMAL 0
#define READY_TOTAL 1
#define READY_TRACK 2
#define READY_LAST 2

#define PLAYMODE_NORMAL		0
#define PLAYMODE_SHUFFLE	1
#define PLAYMODE_PROGRAM	2


typedef struct clientdata_tag {
	unsigned int	quit_flag : 1;
	unsigned int	user_paused : 1;
	unsigned int	skipped_back : 1;
	unsigned int	time_display : 2;
	unsigned int	ready_time_display : 2;
	unsigned int	play_mode : 2;
	unsigned int	data_changed : 1;
	unsigned int	only_program : 1;
	unsigned int	repeat : 1;
	unsigned int	repeat_button : 1;
	unsigned int	skipping : 1;
	unsigned int	cueing : 1;
	int				start_track;
	CDPLAYER		*cdplayer;
	CDSTATUS		*status;
	struct _CDData		*data;
	XtWorkProcId	work_proc_id;
	XtIntervalId	updateid;
	XtIntervalId	pauseid;
	XtIntervalId	ffid;
	XtIntervalId	rewid;
	XtIntervalId	timeid;
	Widget                  toplevel;
	Widget			time;
	Widget			select;
	Widget			progdbox;
	Widget			normalButton;
	Widget			shuffleButton;
	Widget			programButton;
	XFontStruct		*font;
	CDPROGRAM		*program;
	void			(*select_init)( struct clientdata_tag *, int );
	void			(*update_func)( struct clientdata_tag *, XtIntervalId * );
	int				prog_num;
}	CLIENTDATA;
