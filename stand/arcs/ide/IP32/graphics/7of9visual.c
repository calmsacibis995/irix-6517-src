#include <libsc.h>
#include <sys/IP32.h>
#include <sys/sema.h>
#include <sys/sbd.h>
#include <sys/gfx.h>
#include <sys/types.h>

#include <sys/crime.h>
#include <sys/crimechip.h>
#include <sys/crime_gfx.h>
#include <sys/crime_gbe.h>
#include <uif.h>
#include <IP32_status.h>

#include "crmDefs.h"
#include "crm_stand.h"
#include "gbedefs.h"
#include "crm_gfx.h"
#include "crm_timing.h"

/************************************************************************/
/* extern Declarations                                                  */
/************************************************************************/
extern struct crime_info crm_ginfo;
extern struct gbechip *crm_GBE_Base;
extern Crimechip *crm_CRIME_Base;
extern crime_timing_info_t *currentTimingPtr;

extern void crmInitGraphics(int x, int y, int depth, int refresh);
extern void crmDeinitGraphics(void);
extern void crmRESetFGColor(unsigned int color);
extern void crmClearFramebuffer(void);
extern void crmREDrawPoint(int x1, int y1);
extern void crmREDrawLine(int x1, int y1, int x2, int y2);
extern void crmREDrawRect(int x1, int y1, int x2, int y2);
extern void crmREDrawNRect(int x1, int y1, int x2, int y2);
extern void crmSetTLB(char tlbFlag);

/************************************************************************/
/* Function Prototypes                                                  */
/************************************************************************/
void BlackScreen(void);
void TextString(void);
void DrawH(unsigned int x, unsigned int y);
void DrawE(unsigned int x, unsigned int y);
void DrawU(unsigned int x, unsigned int y);
void DrawP(unsigned int x, unsigned int y);
void DrawT(unsigned int x, unsigned int y);
void Menu(void);
void MainMenuSelect(void);
void VertMenu(void);
void VertMenuSelect(void);
void HorizMenu(void);
void HorizMenuSelect(void);
void CBMenu(void);
void CBMenuSelect(void);
void OtherMenu(void);
void OtherMenuSelect(void);
int fpinitialsetup(int argc, char *argv[]);
void VGradientFixed(void); 
void VGradientSmooth(void); 
void VBars(void);
void HBars(void);
void HGradientFixed(void);
void HGradientSmooth(void);
void FullScreenProgression(void);
void OneByOne_CB(void);
void TwoByOne_CB(void);
void CB_OneByOnePixel(void);
void CB_TwoByOnePixel(void);
void CB_OneByOneRGB(void);
void CB_TwoByOneRGB(void);
void Crosstalk(void);
void WhitePatternOnBlack(void);
void EdgeLine(void);
void ColorBar(void);
void ImageSticking(void);
void VerticalLines1(void);
void VerticalLines2(void);
void HorizontalLines1(void);
void HorizontalLines2(void);
int getInput(void);

/* Global Declarations */
static int Width;
static int Height;
static int ResolutionWidth;
static int ResolutionHeight;

#define VERTICAL 0
#define HORIZONTAL 1

int mask[] = {0, 1, 3, 7, 15, 31, 63, 127, 255};
char char_gray[80] ;

/*****************************************/



int getInput()
{
return (atoi(gets(char_gray)));
}


void Menu(void)
{
  printf("\n\n\n");
  printf("            M  A  I  N    M  E  N  U\n\n");
  printf("--------------------------------------------------\n");
  printf(" Please select a submenu:\n");
  printf(" press 1 for Vertical Patterns\n");
  printf(" press 2 for Horizontal Patterns\n");
  printf(" press 3 for Checkerboards\n");
  printf(" press 4 for all other tests\n");
  printf(" press x to exit 7of9 diagnostics\n\n\n");
}

void MainMenuSelect()
{
  char reply, escape = 0;
  
  Menu();

  reply = getchar();

  switch (reply) {
  case '1':
    VertMenuSelect();
    break;
  case '2':
    HorizMenuSelect();
    break;
  case '3':
    CBMenuSelect();
    break;
  case '4':
    OtherMenuSelect();
    break;
  case 'x':
    printf("Diagnostics Complete\n"); 
    escape = 1;
    break;
  default:
    printf("\n\n\nPlease enter a number between 1 and 4 or the letter x!\n\n");
    break;
  }
  if (!escape) 
    MainMenuSelect();
}


void VertMenu(void)
{
  printf("\n\n\n");
  printf("      V E R T I C A L   T E S T S   M E N U\n\n");
  printf("--------------------------------------------------\n");
  printf(" Please select a test:\n");
  printf(" press 1 for Fixed Vertical Gradient\n");
  printf(" press 2 for Smooth Vertical Gradient\n");
  printf(" press 3 for Vertical Bars\n");
  printf(" press 4 for Vertical Lines (1/1)\n");
  printf(" press 5 for Vertical Lines (2/2)\n");
  printf(" press x to return to Main Menu\n\n\n");
}

void VertMenuSelect()
{
  char reply, escape = 0;
  
  VertMenu();

  reply = getchar();

  switch (reply) {
  case '1':
    VGradientFixed();
    break;
  case '2':
    VGradientSmooth();
    break;
  case '3':
    VBars();
    break;
  case '4':
    VerticalLines1();
    break;
  case '5':
    VerticalLines2();
    break;
  case 'x':
    escape = 1;
    break;
  default:
    printf("\n\n\nPlease enter a number between 1 and 5 or the letter x!\n\n");
    break;
  }
  if (!escape) 
    VertMenuSelect();
}


void HorizMenu(void)
{
  printf("\n\n\n");
  printf("     H O R I Z O N T A L   T E S T S   M E N U\n\n");
  printf("--------------------------------------------------\n");
  printf(" Please select a test:\n");
  printf(" press 1 for Fixed Horizontal Gradient\n");
  printf(" press 2 for Smooth Horizontal Gradient\n");
  printf(" press 3 for Horizontal Bars\n");
  printf(" press 4 for Horizontal Lines (1/1)\n");
  printf(" press 5 for Horizontal Lines (2/2)\n");
  printf(" press x to exit to Main Menu\n\n\n");
}

void HorizMenuSelect()
{
  char reply, escape = 0;
  
  HorizMenu();

  reply = getchar();

  switch (reply) {
  case '1':
    HGradientFixed();
    break;
  case '2':
    HGradientSmooth();
    break;
  case '3':
    HBars();
    break;
  case '4':
    HorizontalLines1();
    break;
  case '5':
    HorizontalLines2();
    break;
  case 'x':
    escape = 1;
    break;
  default:
    printf("\n\n\nPlease enter a number between 1 and 5 or the letter x!\n\n");
    break;
  }
  if (!escape)
    HorizMenuSelect();
}


void CBMenu(void)
{
  printf("\n\n\n");
  printf("   C H E C K E R B O A R D   T E S T S   M E N U\n\n");
  printf("--------------------------------------------------\n");
  printf(" Please select a test:\n");
  printf(" press 1 for 1x1 B/W Pixels\n");
  printf(" press 2 for 2x1 B/W Pixels\n");
  printf(" press 3 for 1x1 RGB Pixels\n");
  printf(" press 4 for 2x1 RGB Pixels\n");
  printf(" press x to exit to Main Menu\n\n\n");
}

void CBMenuSelect()
{
  char reply, escape = 0;
  
  CBMenu();

  reply = getchar();

  switch (reply) {
  case '1':
    CB_OneByOnePixel();
    break;
  case '2':
    CB_TwoByOnePixel();
    break;
  case '3':
    CB_OneByOneRGB();
    break;
  case '4':
    CB_TwoByOneRGB();
    break;
  case 'x':
    escape = 1;
    break;
  default:
    printf("\n\n\nPlease enter a number between 1 and 4 or the letter x!\n\n");
    break;
  }
  if (!escape)
     CBMenuSelect();
}


void OtherMenu(void)
{
  printf("\n\n\n");
  printf("       O T H E R   T E S T S   M  E  N  U\n\n");
  printf("--------------------------------------------------\n");
  printf(" Please select a test:\n");
  printf(" press 1 for Full Screen Progression\n");
  printf(" press 2 for Crosstalk Check\n");
  printf(" press 3 for White Patterns on Black Field\n");
  printf(" press 4 for Edge Line\n");
  printf(" press 5 for Color Bars\n");
  printf(" press 6 for Image Sticking Check\n");
  printf(" press 7 for Text Samples\n");
  printf(" press x to exit to Main Menu\n\n\n");
}

void OtherMenuSelect()
{
  char reply, escape = 0;
  
  OtherMenu();

  reply = getchar();

  switch (reply) {
  case '1':
    FullScreenProgression();
    break;
  case '2':
    Crosstalk();
    break;
  case '3':
    WhitePatternOnBlack();
    break;
  case '4':
    EdgeLine();
    break;
  case '5':
    ColorBar();
    break;
  case '6':
    ImageSticking();
    break;
  case '7':
    TextString();
    break;
  case 'x':
    escape = 1;
    break;
  default:
    printf("\n\n\nPlease enter a number between 1 and 7 or the letter x!\n\n");
    break;
  }
  if (!escape)
    OtherMenuSelect();
}



void DrawBar2(short direction)
{
  unsigned int thickness, percent, total, k;
  
  if (direction == VERTICAL) {
    total = Width;
    msg_printf(SUM, "Vertical bar pattern (Black and White)\n\n");
    msg_printf(SUM, "Please enter the bar thickness as a percentage of width (%d)\n",ResolutionWidth); 
    msg_printf(SUM, "\n?");
  }
  else {
    total = Height;
    msg_printf(SUM, "Horizontal bar pattern (Black and White)\n\n");
    msg_printf(SUM, "Please enter the bar thickness as a percentage of height (%d)\n",ResolutionHeight); 
    msg_printf(SUM, "\n?");
  }    

  percent = getInput();
  thickness = (percent * total) / 100;

		crmRESetFGColor(0x0); /* BLACK */
		crmREDrawNRect(0, 0,Width, Height);

	for ( k = 0 ; k < total ; k+=(2*thickness)) {
		crmRESetFGColor(0xffffffff); /* WHITE */
		if (direction == VERTICAL)
		  crmREDrawNRect(k, 0, k+thickness, Height);
		else
		  crmREDrawNRect(0, k, Width, k+thickness);
	}
}


void VBars()
{
  DrawBar2(VERTICAL);
}

void HBars() 
{
  DrawBar2(HORIZONTAL);
}


void HGradientFixed()
{
  int i, w, rem, pause; 

  w = (ResolutionWidth / 9);
  rem = ResolutionWidth % 9;

  for ( i = 0 ; i < 9; i++) {
    crmRESetFGColor(mask[i] << 24 |mask[i] << 16 |mask[i] << 8 |mask[i]);
    crmREDrawNRect(i*w, 0, (i+1)*w, Height );
  }
  
  /* Draw the remaining pixels white to fill out remainder */

  crmRESetFGColor(0xffffffff);
  crmREDrawNRect(9*w, 0, 9*w + rem, Height);

  msg_printf(SUM, "\n Press the <RETURN> key to continue...\n"); 
  pause = getInput();
  
  BlackScreen();
}

void VGradientFixed()
{ 
  int i, h, rem, pause;
  h = (ResolutionHeight / 9);
  rem = ResolutionHeight % 9;
	
  for ( i = 0 ; i < 9; i++) {
    crmRESetFGColor(mask[i] << 24 |mask[i] << 16 |mask[i] << 8 |mask[i]);
    crmREDrawNRect(0, i*h, Width, (i+1)*h);
  }

  /* Draw the remaining pixels white to fill out remainder */

  crmRESetFGColor(0xffffffff);
  crmREDrawNRect(0, 9*h, Width, 9*h + rem);

  msg_printf(SUM, "\n Press the <RETURN> key to continue...\n"); 
  pause = getInput();
  
  BlackScreen();
}

void HGradientSmooth()
{
  int i, w, rem, offset, pause; 

  w = (ResolutionWidth / 256);
  rem = ResolutionWidth % 256;

  for ( i = 0 ; i < 256; i++) {
    crmRESetFGColor(i << 24 | i << 16 | i << 8 | 0xff);
    crmREDrawNRect(i*w, 0, ((w*(i+1))-1), Height);
  }
  
  offset = 256*w;

  /* Draw the remaining lines white to fill out screen completely */

  for ( i = 0; i < rem; i++) {
    /*    crmRESetFGColor(i << 24 | i << 16 | i << 8 | 0xff);*/
    crmRESetFGColor(0xffffffff);
    crmREDrawLine(i+offset, 0, i+offset, Height );
  }
  
  msg_printf(SUM, "\n Press the <RETURN> key to continue...\n"); 
  pause = getInput();
  
  BlackScreen();
}

void VGradientSmooth()
{ 
  int i, h, rem, offset, pause;

  h = (ResolutionHeight / 256);
  rem = ResolutionHeight % 256;
	
  for ( i = 0 ; i < 256; i++) {
    crmRESetFGColor(i << 24 | i << 16 | i << 8 | 0xff);
    crmREDrawNRect(0, i*h, Width, ((h*(i+1))-1));
  }

  offset = 256*h;

  /* Draw the remaining lines white to fill out screen completely */

  for ( i = 0; i < rem; i++) {
    /* crmRESetFGColor(i << 24 | i << 16 | i << 8 | 0xff); */
    crmRESetFGColor(0xffffffff);
    crmREDrawLine(0, i+offset, Width, i+offset );
  }

  msg_printf(SUM, "\n Press the <RETURN> key to continue...\n"); 
  pause = getInput();
  
  BlackScreen();
}

void FullScreenProgression()
{
  unsigned int gray, pause;
	
  printf(" All Gray and Color \n ");
	
  msg_printf(SUM, "Please enter the gray level number 0-255\n"); 
  msg_printf(SUM, "\n?");
  gray = getInput();

  crmRESetFGColor (0); /* Black */
  crmREDrawNRect(0, 0, Width, Height);

  msg_printf(SUM, "\n Press any key to advance patterns\n");
  msg_printf(SUM, "\tBlack\n");
  pause = getInput();
  
  crmRESetFGColor (0x7f7f7fff); /* Gray */
  crmREDrawNRect(0, 0, Width, Height);
  
  msg_printf(SUM, "\tGray\n");
  pause = getInput();
  
  crmRESetFGColor (0xffffffff); /* White */
  crmREDrawNRect(0, 0, Width, Height);

  msg_printf(SUM, "\tWhite\n");
  pause = getInput();

  crmRESetFGColor(gray << 24 | 0 | 0 | 0xff ); /* Red */ 
  crmREDrawNRect(0, 0, Width, Height);

  msg_printf(SUM, "\tRed\n");
  pause = getInput();

  crmRESetFGColor(gray << 24 | 0 | gray << 8 | 0xff ); /* Magenta */
  crmREDrawNRect(0, 0, Width, Height);

  msg_printf(SUM, "\tMagenta\n");
  pause = getInput();

  crmRESetFGColor (0); /* Black */
  crmREDrawNRect(0, 0, Width, Height);

  msg_printf(SUM, "\nTest Complete\n");

}

void OneByOne_CB(void)
{
  int x,y;
  
  for ( y = 0 ; y <= Height ; y++) 
    for ( x = 0 ; x <= Width ; x++) 
      if ((((y % 2) == 0) && ((x % 2) == 0)) ||     /* Even row, even column */
	  (((y % 2) == 1) && ((x % 2) == 1)))       /* Odd row, odd column */
	crmREDrawPoint(x,y);
}

void TwoByOne_CB(void)
{
  int x,y;

    for ( y = 0 ; y <= Height ; y++) 
      for ( x = 0 ; x <= Width ; x++) 
	if ((((y % 2) == 0) && ((x % 4) <= 1)) ||     /* Even row, cols 0,1 */
	    (((y % 2) == 1) && ((x % 4) >= 2)))       /* Odd row, cols 2,3 */
	  crmREDrawPoint(x,y);
}

void CB_OneByOnePixel() 
{
  unsigned int red, green, blue, pause;

  msg_printf(SUM, " Please enter the red level number 0-255\n");
  msg_printf(SUM, "\n?");
  red = getInput();

  msg_printf(SUM, " Please enter the green level number 0-255\n");
  msg_printf(SUM, "\n?");
  green = getInput();

  msg_printf(SUM, " Please enter the blue level number 0-255\n");
  msg_printf(SUM, "\n?");
  blue = getInput();

  crmRESetFGColor (0x0); /* Black Background */
  crmREDrawNRect(0, 0, Width, Height);    
  
  crmRESetFGColor (red << 24 | green << 16 | blue << 8 | 0xff);
  
  OneByOne_CB();

  msg_printf(SUM, "\n Press the <RETURN> key to continue...\n"); 
  pause = getInput();
  
  BlackScreen();

}

void CB_TwoByOnePixel() 
{

  unsigned int red, green, blue, pause;

  msg_printf(SUM, " Please enter the red level number 0-255\n");
  msg_printf(SUM, "\n?");
  red = getInput();

  msg_printf(SUM, " Please enter the green level number 0-255\n");
  msg_printf(SUM, "\n?");
  green = getInput();

  msg_printf(SUM, " Please enter the blue level number 0-255\n");
  msg_printf(SUM, "\n?");
  blue = getInput();

  /*    unsigned int gray, pause;

    msg_printf(SUM, " Please enter the gray level number 0-255\n");
    msg_printf(SUM, "\n?");
    gray = getInput();

    */
    crmRESetFGColor (0x0); /* Black Background */
    crmREDrawNRect(0, 0, Width, Height);
    
    /*    crmRESetFGColor (gray << 24 | gray << 16 | gray << 8 | 0xff);*/

    crmRESetFGColor (red << 24 | green << 16 | blue << 8 | 0xff);

    TwoByOne_CB();

    msg_printf(SUM, "\n Press the <RETURN> key to continue...\n"); 
    pause = getInput();
    
    BlackScreen();
}

void CB_OneByOneRGB() 
{

    unsigned int gray, pause;

    msg_printf(SUM, " Please enter the gray level number 0-255\n");
    msg_printf(SUM, "\n?");
    gray = getInput();
    
    /* Green Background @ gray level */
    crmRESetFGColor (0x0 << 24 | gray << 16 | 0x0 << 8 | 0xff);

    crmREDrawNRect(0, 0, Width, Height);

    /* Magenta Pixels @ gray level */
    crmRESetFGColor (gray << 24 | 0x0  << 16 | gray << 8 | 0xff);

    OneByOne_CB();

    msg_printf(SUM, "\n Press the <RETURN> key to continue...\n"); 
    pause = getInput();
  
    BlackScreen();
}

void CB_TwoByOneRGB() 
{

    unsigned int gray, pause;
    msg_printf(SUM, " Please enter the gray level number 0-255\n");
    msg_printf(SUM, "\n?");
    gray = getInput();
    
    /* Green Background @ gray level */
    crmRESetFGColor (0x0 << 24 | gray << 16 | 0x0 << 8 | 0xff);

    crmREDrawNRect(0, 0, Width, Height);

    /* Magenta Pixels @ gray level */
    crmRESetFGColor (gray << 24 | 0x0  << 16 | gray << 8 | 0xff);

    TwoByOne_CB();

    msg_printf(SUM, "\n Press the <RETURN> key to continue...\n"); 
    pause = getInput();
    
    BlackScreen();
}

void Crosstalk()
{
  unsigned int gray, i, j;
  short state = 0x0;
  char toggle = 0;

  msg_printf(SUM, " Please enter the gray level number 0-255\n");
  msg_printf(SUM, "\n?");
  gray = getInput();
  
  /* Draw gray background */

  crmRESetFGColor(gray << 24 | gray << 16 | gray << 8 | 0xff );
  crmREDrawNRect(0,0, Width, Height);

  msg_printf(SUM, " To exit at any time, press \"x\"\n All other keys toggle image\n");

  while (toggle != 'x') {

    toggle = getchar();
    
    if (state == 0x0) {
      /* Black block */
      crmRESetFGColor(0);
      state = 0x1;
    }

    else {
      /* White block */
      crmRESetFGColor(0xffffffff);
      state = 0x0;
    }
    crmREDrawNRect(Width/4, Height/4, 3*Width/4, 3*Height/4);
  }

  msg_printf(SUM, "Test complete\n"); 

  BlackScreen();

}

void WhitePatternOnBlack()
{
  unsigned int pause;

  crmRESetFGColor(0x0);
  crmREDrawNRect(0,0, Width, Height);  /* Draw black background */

  /* First white block */
  crmRESetFGColor(0xffffffff);
  crmREDrawNRect(0,0, Width/2 + 100, Height/2 + 100);

  msg_printf(SUM, "Enter to continue\n");
  pause = getInput();

  /* Second white block */
  crmRESetFGColor(0x0);
  crmREDrawNRect(0,0, Width, Height);  /* Draw black background */
  crmRESetFGColor(0xffffffff);
  crmREDrawNRect(Width/2 - 100,0, Width, Height/2 + 100 );

  msg_printf(SUM, "Enter to continue\n");
  pause = getInput();
 	
  /* Third white block */
  crmRESetFGColor(0x0);
  crmREDrawNRect(0,0, Width, Height);  /* Draw black background */
  crmRESetFGColor(0xffffffff);
  crmREDrawNRect(Width/2 - 100, Height/2, Width, Height);
  
  msg_printf(SUM, "Enter to continue\n");
  pause = getInput();

  /* Fourth white block */
  crmRESetFGColor(0x0);
  crmREDrawNRect(0,0, Width, Height);  /* Draw black background */
  crmRESetFGColor(0xffffffff);
  crmREDrawNRect(0,Height/2, Width/2 + 100, Height);
  
  msg_printf(SUM, "Enter to continue\n");
  pause = getInput();

  /* 100 pixel white frame */
  crmRESetFGColor(0xffffffff);
  crmREDrawNRect(0,0, Width, Height);  /* Draw white background */

  crmRESetFGColor(0x0);                /* Black center */
  crmREDrawNRect(100, 100, Width-100, Height-100);

  msg_printf(SUM, "\n Press the <RETURN> key to continue...\n"); 
  pause = getInput();
    
  BlackScreen();
}

void EdgeLine()
{
  unsigned int red, green, blue, pause;
  char toggle = 0;
  
  msg_printf(SUM, " Please enter the red level number 0-255\n");
  msg_printf(SUM, "\n?");
  red = getInput();
  msg_printf(SUM, "\n Please enter the green level number 0-255\n");
  msg_printf(SUM, "\n?");
  green = getInput();
  msg_printf(SUM, "\n Please enter the blue level number 0-255\n");
  msg_printf(SUM, "\n?");
  blue = getInput();
  
  crmRESetFGColor(0x0);  /* Draw black background */
  crmREDrawNRect(0,0, Width, Height);
  
  /* Set the color */
  crmRESetFGColor(red << 24 | green << 16 | blue << 8 | 0xff );

  /* Top Line */
  crmREDrawLine(0, 0, Width, 0);

  /* Bottom Line */
  crmREDrawLine(0, Height, Width, Height);

  /* Left Line */
  crmREDrawLine(0, 0, 0, Height);

  /* Right Line */
  crmREDrawLine(Width, 0, Width, Height);
  
  msg_printf(SUM, "\n Press the <RETURN> key to continue...\n"); 
  pause = getInput();
    
  BlackScreen();
}

void ColorBar()
{
  unsigned int gray, w, rem, pause;

  w = (ResolutionWidth / 8);
  rem = ResolutionWidth % 8;

  msg_printf(SUM, " Please enter the gray level number 0-255\n");
  msg_printf(SUM, "\n?");
  gray = getInput();
  
  /* Blank screen */

  crmRESetFGColor(0);
  crmREDrawNRect(0, 0, Width, Height);

  /* White */

  crmRESetFGColor(gray << 24 | gray << 16 | gray << 8 | 0xff );
  crmREDrawNRect(0, 0, w, Height);
    
  /* Yellow */
  
  crmRESetFGColor(gray << 24 | gray << 16 | 0  | 0xff );
  crmREDrawNRect(w, 0, 2*w, Height);
  
  /* Cyan */
  
  crmRESetFGColor(0 << 24 | gray << 16 |  gray << 8 | 0xff );
  crmREDrawNRect(2*w, 0, 3*w, Height);
  
  /* Green */
  
  crmRESetFGColor(0 << 24 | gray << 16 | 0 << 8 | 0xff );
  crmREDrawNRect(3*w, 0, 4*w, Height);
  
  /* Magenta */
  
  crmRESetFGColor(gray << 24 | 0 << 16 | gray << 8 | 0xff );
  crmREDrawNRect(4*w, 0, 5*w, Height);
  
  /* Red */
  
  crmRESetFGColor(gray << 24 | 0 << 16 | 0 << 8 | 0xff );
  crmREDrawNRect(5*w, 0, 6*w, Height);
  
  /* Blue */
  
  crmRESetFGColor(0 << 24 | 0 << 16 | gray << 8 | 0xff );
  crmREDrawNRect(6*w, 0, 7*w, Height);
  
  /* Draw the remaining pixels black to repeat the pattern */
  
  crmRESetFGColor(0x0);
  crmREDrawNRect(7*w, 0, 7*w + rem, Height);

  msg_printf(SUM, "\n Press the <RETURN> key to continue...\n"); 
  pause = getInput();
    
  BlackScreen();
	
}

#define BLOCK_SIZE 30
#define PITCH_SIZE 30

void ImageSticking()
{
  unsigned int gray, x, y, i, j, pause;

  msg_printf(SUM, " Please enter the gray level number 0-255\n");
  msg_printf(SUM, "\n?");
  gray = getInput();

  crmRESetFGColor(0);  /* Draw black background */
  crmREDrawNRect(0, 0, Width, Height);

  crmRESetFGColor(0xffffffff);
  
  msg_printf(SUM, " Please enter the x coordinate\n");
  msg_printf(SUM, "\n?");
  x = getInput();
  
  msg_printf(SUM, "\n Please enter the y coordinate\n");
  msg_printf(SUM, "\n?");
  y = getInput();
  /*  msg_printf(SUM, " x -> %d and y ->%d \n", x, y); */

  for ( j = y; j+(BLOCK_SIZE+PITCH_SIZE) <= Height; j+=(BLOCK_SIZE+PITCH_SIZE))
    for ( i = x; i+(BLOCK_SIZE+PITCH_SIZE) <= Width; i+=(BLOCK_SIZE+PITCH_SIZE)) 
      crmREDrawNRect(i, j, i+BLOCK_SIZE, j+BLOCK_SIZE);

  msg_printf(SUM, " To toggle to gray background, press any key\n");
  pause = getInput();

  crmRESetFGColor(gray << 24 | gray << 16 | gray << 8 | 0xff );
  crmREDrawNRect(0, 0, Width, Height);

  msg_printf(SUM, "\n Press the <RETURN> key to continue...\n"); 
  pause = getInput();
    
  BlackScreen();

}


void DrawH (unsigned int x, unsigned int y) {
  
  crmREDrawLine(x,y,x,y+6);
  crmREDrawLine(x,y+3,x+4,y+3);
  crmREDrawLine(x+4,y,x+4,y+6);

}

void DrawE (unsigned int x, unsigned int y) {
  
  crmREDrawLine(x,y,x,y+6);
  crmREDrawLine(x,y,x+4,y);
  crmREDrawLine(x,y+3,x+3,y+3);
  crmREDrawLine(x,y+6,x+4,y+6);

}

void DrawU (unsigned int x, unsigned int y) {
  
  crmREDrawLine(x,y,x,y+6);
  crmREDrawLine(x,y+6,x+4,y+6);
  crmREDrawLine(x+4,y,x+4,y+6);

}

void DrawP (unsigned int x, unsigned int y) {
  
  crmREDrawLine(x,y,x,y+6);
  crmREDrawLine(x,y,x+4,y);
  crmREDrawLine(x,y+3,x+4,y+3);
  crmREDrawLine(x+4,y,x+4,y+3);

}

void DrawT (unsigned int x, unsigned int y) {
  
  crmREDrawLine(x,y,x+4,y);
  crmREDrawLine(x+2,y,x+2,y+6);

}

void TextString()
{
  unsigned int gray, x, y, i, j, pause;

  msg_printf(SUM, " Please enter the background gray level number 0-255\n");
  msg_printf(SUM, "\n?");
  gray = getInput();

  crmRESetFGColor(gray << 24 | gray << 16 | gray << 8 | 0xff );
  crmREDrawNRect(0, 0, Width, Height);

  msg_printf(SUM, " Please enter the text gray level number 0-255\n");
  msg_printf(SUM, "\n?");
  gray = getInput();

  crmRESetFGColor(gray << 24 | gray << 16 | gray << 8 | 0xff );

  for (j=400; j <= 600; j+=10) {
    for (i=3; i < (Width-10); i+=10) {
      DrawH(i,j);
      i+=7;
      DrawE(i,j);
      i+=7;
      DrawU(i,j);
      i+=7;
      DrawP(i,j);
      i+=7;
      DrawT(i,j);
    }
  }

  msg_printf(SUM, "\n Test complete\n Press the <RETURN> key to continue...\n"); 
  pause = getInput();
    
  BlackScreen();
}


void VerticalLines1()
{
  unsigned int gray, i, j, pause;
  
  msg_printf(SUM, " Please enter the gray level number 0-255\n");
  msg_printf(SUM, "\n?");
  gray = getInput();
  
  crmRESetFGColor(0);  /* Draw black background */
  crmREDrawNRect(0, 0, Width, Height);
  
  crmRESetFGColor(gray << 24 | gray << 16 | gray << 8 | 0xff );
  
  for ( i = 0; i <= Width; i+=2)
    crmREDrawLine(i, 0, i, Height);

  msg_printf(SUM, "\n Press the <RETURN> key to continue...\n"); 
  pause = getInput();
    
  BlackScreen();

}

void VerticalLines2()
{
  unsigned int gray, i, j, pause;

  msg_printf(SUM, " Please enter the gray level number 0-255\n");
  msg_printf(SUM, "\n?");
  gray = getInput();
  
  crmRESetFGColor(0);
  crmREDrawNRect(0, 0, Width, Height);
  
  crmRESetFGColor(gray << 24 | gray << 16 | gray << 8 | 0xff );
  
  for ( i = 0; i <= Width; i+=4)
    crmREDrawNRect(i, 0, i+1, Height);

  msg_printf(SUM, "\n Press the <RETURN> key to continue...\n"); 
  pause = getInput();
    
  BlackScreen();

}

void HorizontalLines1()
{
  unsigned int gray, i, j, pause;
  
  msg_printf(SUM, " Please enter the gray level number 0-255\n");
  msg_printf(SUM, "\n?");
  gray = getInput();
  
  crmRESetFGColor(0x0);
  crmREDrawNRect(0, 0, Width, Height);
  
  crmRESetFGColor((gray << 24 | gray << 16 | gray << 8 | 0xff));
  
  for ( i = 0; i <= Height; i+=2)
    crmREDrawLine(0, i, Width, i);
  
  msg_printf(SUM, "\n Press the <RETURN> key to continue...\n"); 
  pause = getInput();
    
  BlackScreen();

}

void HorizontalLines2()
{
  unsigned int gray, i, j, pause;
  
  msg_printf(SUM, " Please enter the gray level number 0-255\n");
  msg_printf(SUM, "\n?");
  gray = getInput();

  crmRESetFGColor(0);
  crmREDrawNRect(0, 0, Width, Height);
  
  crmRESetFGColor(gray << 24 | gray << 16 | gray << 8 | 0xff );
  
  for ( i = 0; i <= Height; i+=4)
    crmREDrawNRect(0, i, Width, i+1);

  msg_printf(SUM, "\n Press the <RETURN> key to continue...\n"); 
  pause = getInput();
    
  BlackScreen();

}

int
fpinitialsetup(int argc, char *argv[])
{
  int xmax, ymax, depth, refresh, i;
  
#ifdef 0

    if (argc != 5) {
    msg_printf(ERR, "Usage: init <xmax> <ymax> <depth> <refresh>\n");
    return 0;
  } 
   
  xmax = atoi(argv[1]);
  ymax = atoi(argv[2]);
  depth = atoi(argv[3]);
  refresh = atoi(argv[4]);

#endif 

  /* I decided to hardcode these values, as they aren't going to change.
     The flexible method is ifdef'd out above */

  xmax = 1600;
  ymax = 1024;
  depth = 32;
  refresh = 50;

   /* Some parts of the code want the actual resolution; other parts need the
    * maximum value for line or column numbers.
    * ResolutionWidth and ResolutionMax contain the nominal screen resolution.
    * Width and Max contain the actual values (i.e., since pixels start at 0,
    * the Width would be 1599 for a ResolutionWidth of 1600).
    */

   ResolutionWidth = xmax;  
   ResolutionHeight = ymax;
   Width = xmax-1;
   Height = ymax-1;

   crmInitGraphics(xmax, ymax, depth, refresh);

   gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
   gbeSetReg( crm_GBE_Base, did_control, 0x00000000);

   for (i = 0; i < 32; i++)
     {
       gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
       gbeSetTable( crm_GBE_Base, mode_regs, i, 0x00000417);
     }
   
   crmSetTLB(CRM_SETUP_TLB_B);

   printf("\n\n\nFlat Panel 1600SW Manufacturing Process Diagnostics, V1.1\n\n\n");

   MainMenuSelect();

   crmDeinitGraphics();
}


void
BlackScreen(void)
{
  crmRESetFGColor (0x0); /* Black Background */
  crmREDrawNRect(0, 0, Width, Height);  
}  




