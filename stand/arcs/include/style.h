/* defines for stand gui appearance
 */
#ident "$Revision: 1.13 $"

#ifndef _STYLE_H
#define _STYLE_H

/* colors for gui background */
#define COMPANYNAMECOLOR	48	/* silicon graphics computer systems */
#define CLOGOCOLOR	 	49	/* calligraphy logo */
#define CLOGOSHADOWCOLOR	50	/* calligraphy shadow */

/* position of calligraphy */
#define CLOGOX		530
#define CLOGOX1024	445
#define CLOGODY_C	50
#define CLOGOY		80
#define CLOGODX		10
#define CLOGODY		(-14)

/* Threshold between 1024ish and 1280+ish layout */
#define APPROX1024		1050

/* position of "welcome to" */
#define WELCOMEX	150
#define WELCOMEX1024	65
#define WELCOMEY	200
#define WELCOMEDX	5
#define WELCOMEDY	(-6)
#define WELCOMEFONT	ncenB18

/* position of company name */
#define COMPANYNAMEX	520
#define COMPANYNAMEX_C	580
#define COMPANYNAMEY	50

/* button stuff */
#define BUTTONFONT	helvR10		/* font used on pushbuttons */
#define TEXTBUTH	28		/* text button minimum sizes */
#define TEXTBUTW	75
#define BUTTONGAP	8		/* gap between buttons */
#define BUTTONAMARGIN	5		/* text -> arrow margin */
#define BUTTONMINMARGIN	10		/* min left/right button margin */


/* message position (used by pon, autoboot, etc) */
#define MSGX		((gfxWidth() <= APPROX1024) ? WELCOMEX1024 : WELCOMEX)
#define MSGY1		((gfxHeight()*3)>>2)


/* menu stuff */
#define MENUBUTTW	50
#define MENUBUTTH	50
#define MENUVSPACE	25

/* dialog stuff */
#define DIALOGBDW		4		/* border width */
#define DIALOGMARGIN		8
#define DIALOGINTBDW		3		/* internal border width */
#define DIALOGICONW		32		/* icon width */
#define DIALOGICONMARGIN	4
#define DIALOGBUTMARGIN		12
#define DIALOGTEXTBDX		(DIALOGBDW+DIALOGMARGIN+DIALOGINTBDW+\
				 DIALOGICONW+DIALOGINTBDW+DIALOGICONMARGIN)
#define DIALOGTEXTMARGIN	15
#define DIALOGFONT		ScrB18
#define DIALOGLFONT		ncenB18

/* form (on a canvas) stuff */
#define FORMVERTMARGIN		18

/* text field stuff */
#define TFIELDBDW	4
#define TFIELDMARGIN	5

/* radio button stuff */
#define RADIOBUTW	24
#define RADIOLISTXGAP	6
#define RADIOLISTDESC	8
#define RADIOLISTYGAP	4

/* basic colors */
#define BLACK		0
#define RED		1
#define GREEN		2
#define YELLOW		3
#define BLUE		4
#define MAGENTA		5
#define CYAN		6
#define WHITE		7
#define BLACK2		51
#define RED2		52
#define GREEN2		53
#define YELLOW2		54
#define BLUE2		55
#define MAGENTA2	56
#define CYAN2		57
#define WHITE2		58

/* Colormap entries for grays needed for border/gui */
#define GRAY42		35
#define GRAY85		36
#define GRAY128		37
#define GRAY170		38
#define GRAY213		39

/* SGI style guide colors */
#define White			WHITE
#define VeryLightGray		GRAY213
#define LightGray		GRAY170
#define MediumGray		GRAY128
#define DarkGray		GRAY85
#define VeryDarkGray		GRAY42
#define Black			BLACK

/* other colors used by gui */
#define lightGrayBlue		20
#define lightBlue		21
#define lightGrayCyan		22
#define lightCyan		23
#define lightGrayGreen		24
#define lightGreen		25
#define lightGrayMagenta	26
#define lightMagenta		27
#define lightGrayRed		28
#define lightRed		29
#define TerraCotta		30
#define DarkTerraCotta		31
#define SlateBlue		32
#define TP_IDX			9

/* machine dependent icon colors */
#define MACHCLR0		59
#define MACHCLR1		60
#define MACHCLR2		61
#define MACHCLR3		62
#define MACHCLR4		63

/* font names */
#define NUMFONTS	4
#define ScrB18   0
#define Special  1
#define ncenB18  2
#define helvR10	 3

#endif
