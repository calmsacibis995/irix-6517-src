/* Record Header Fields */
#define MAGIC 		1
#define RECTYPE 	2
#define OUTSTRING 	3
#define SEQUENCE 	4
#define ERRNO 		5
#define TIME 		6
#define TICKS 		7
#define SYSCALL 	8
#define SUBSYSCALL 	9
#define HOST_ID 	10 /* a */
#define CAP		11

#define SATID 		12 /* c */
#define TTY 		13
#define PPID 		14
#define PID 		15

#define PNAME 		16 /* 10 */
#define PLABEL 		17
#define EUID 		18 /* 12 */
#define RUID 		19
#define EGID 		20 /* 14 */

#define RGID 		21
#define NGROUP		22 /* 16 */
#define GROUPS 		23
#define CWD 		24 /* 18 */
#define ROOTDIR 	25

#define PCAP		26
/*
 * #define	27
 * #define  	28
 * #define  	29
 */
/* Record Body Fields */
#define  FILE0INODE	30 /* 1e */
#define  FILE1INODE	31
#define  FILE2INODE	32 /* 20 */

#define  FILE0DEVICE	35
#define  FILE1DEVICE	36 /* 24 */
#define  FILE2DEVICE	37

#define  FILE0OWN	40 /* 28 */
#define  FILE1OWN	41
#define  FILE2OWN	42

#define  FILE0GRP	45
#define  FILE1GRP	46 /* 2e */
#define  FILE2GRP	47

#define  FILE0MODE	50 /* 32 */
#define  FILE1MODE	51
#define  FILE2MODE	52

#define  FILE0LABEL	55  /* 37 also used for process and IPC object label */
#define  FILE1LABEL	56
#define  FILE2LABEL	57

#define  FILE0REQNAME 	60 /* 3c */
#define  FILE1REQNAME 	61
#define  FILE2REQNAME 	62

#define  FILE0ACTNAME	65 /* 41 */ 
#define  FILE1ACTNAME	66
#define  FILE2ACTNAME	67

#define  FILE0ACL	68 /* 44 */
#define  FILE0CAP	69


#define FILEDES0	70 
/* 70-89 in use for file descriptors 0-19 */
#define FILEDES19	89

#define MAPPEDNAME0	90
/* 90-109 in use for mapped names of file descriptors 0-19 */
#define MAPPEDNAME19	109

#define OBJUMASK	119
#define	OPENCREATE	120 /* 78 */
#define	OPENFLAGS	121
#define	NEWEUID		122
#define	NEWEGID		123
#define	NEWPID		124
#define	FDSET		125
#define FDLIST		126
#define NFDS		127 /* 7f */

#define	ATIME		130 /* 82 */
#define MTIME		131
#define ACCTSTATE	132
#define SIGNAL		133
#define	PROCBLKCMD	134
#define	EXITSTATUS	135
#define	OBJRUID		136
#define	OBJEUID		137 /* 89 */
#define	OBJPID		138
#define	OBJGLIST_LEN	139

#define	OBJOGLIST	140 /* 8c */
#define	OBJGLIST	141
#define	OBJRGID		142
#define	OBJEGID		143 /* 8f */

#define	SVIPCID		144 /* 90 */
#define	SVIPCKEY	145
#define	SOCKTYPE	146
#define	SOCKID		147 /* 93 */
#define	SOCKID1		148
#define	SOCKDSCR	149
#define	SOCKDSCR1	150 /* 96 */
#define	COMMDOMAIN	151

#define	SOUID		152 /* 98 */
#define	SOUID1		153
#define	SORCVUID	154 /* 9a */
#define	SORCVUID1	155

#define	SOACLUIDCOUNT	156
#define	SOACLUIDCOUNT1	157
#define	SOACLUIDLIST	158
#define	SOACLUIDLIST1	159 /* 9f */

#define	RENDEZVOUS	160 /* A0 */

#define	PORT 		162
#define	HOST		163
#define	ADDR		164
#define	SOCADDR		165
#define	ADDRLNTH	166 /* A6 */
#define	SERV		167
#define	DESTPORT	168
#define	DESTADDR	169 /* A9 */

#define	DGLABEL		170
#define	DGUID		171 /* AB */
#define	IFNAME		172
#define	IPOPTS		173
#define	IOCTLCMD	174
#define	IFFLAGS		175
#define	IFMETRIC	176 /* b0 */

#define	IPACKETS	177
#define	IERRORS		178
#define	OPACKETS	179
#define	OERRORS		180 /* b4 */
#define	COLLISIONS	181

#define	LABEL_MAX	182
#define	LABEL_MIN	183
#define	DOI		184
#define	AUTHORITY_MAX	185
#define	AUTHORITY_MIN	186 /* ba */
#define	IDIOM		187

#define	IOC_UNKNOWN	188
#define	HOSTNAME_SET 	189
#define	DOMAINNAME_SET	190 /* be */
#define	HOSTID_SET	191 /* bf */

#define	PRIVSTATE	192 /* c0 */
#define	CTL_SAT_CMD	193
#define	CTL_SAT_ARG	194
#define CTL_SAT_PID	195 /* c3 */

#define STRING		196
#define SHUTDOWN	197
