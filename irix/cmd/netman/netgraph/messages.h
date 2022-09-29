#define NOADDPRMSCSMSG	0
#define NOSNOOPDMSG	1
#define NOSTARTSNOOPMSG	2
#define ILLEGALINTFCMSG	3
#define BADINTFCMSG	4
#define NEGINTVLMSG	5
#define NEGPERIODMSG	6
#define BADPERIODMSG	7
/* the following msg IS used, but I don't expect it to ever be called anymore.*/
#define BADSTRIPMSG	8
#define MALLOCERRMSG	9
#define TOOMANYERRSMSG	10
#define GETHOSTNAMEMSG  11
#define SSREADERRMSG	12
#define SSSUBSCRIBEERRMSG 13
#define ILLEGALTOKENMSG	14
#define SETSNOOPLENMSG	15
#define BADXDRWROPENMSG	16
#define BADXDRRDOPENMSG	17
#define BADXDRWRITEMSG	18
#define BADXDRREADMSG	19
#define NOHISTMSG	20
#define HISTNORECORDMSG	21
#define LOGNOOPENMSG	22
#define RCTOOBIGMSG	23

static char *Message[] = {
	"Could not add promiscuous snoop filter",
	"Could not connect to snoopd..., Message in ::post",
	"Could not start snooping",
	"Data Station name not recognized..., Message in ::post",
	"Bad Interface",
	"Interval must not be less than 0.1",

	"Period must not be less than 0.1",
	"Period must be larger than interval",
	"Error setting up graph strip",
	"Could not allocate memory for new strip",
	"Cannot continue -- too many errors",

	"Could not get name of node",
	"Error in snoop read",
	"Snoopd not configured... message in ::post",
	"Illegal graph specification token",
	"Error in setting snoop length",

	"Error in opening history file, so can not save history",
	"Error in opening history file, so can not play back history",
	"Error writing to history file",
	"Error reading history file",
	"History file cannot be specified here",

	"When playing back history, cannot simultaneously record history",
	"Could not open alarm log file, so alarms will go to standard output", 
	"ControlsFile has too many lines. It probably isn't in the correct format", 
						};
