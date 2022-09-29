/*
 *  bufview - display file system buffer cache activity
 *
 *  This file defines the locations on the screen for various parts of the
 *  display.  These definitions are used by the routines in "display.c" for
 *  cursor addressing.
 */

#define  x_ticks	53
#define  y_ticks	0

#define  y_sysstat	0
#define  y_datastat	1
#define  y_emptystat	2
#define  y_getstat	3
#define  y_message	5

#define  x_sysstat	0
#define  x_datastat	0
#define  x_emptystat	0
#define  x_getstat	0

#define  x_header	0
#define  y_header	6	/* unused */
#define  x_idlecursor	0
#define  y_idlecursor	5
#define  y_buffers	7

/* Number of lines of header information on the standard screen */
#define Header_lines	7

