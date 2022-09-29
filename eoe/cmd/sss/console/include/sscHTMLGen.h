#ifndef _HTMLGEN_H_
#define _HTMLGEN_H_

#include "sscStreams.h"

#define GETHTMLGENINFO_ALLOC 0x01955001
#define GETHTMLGENINFO_BUSY  0x01955002
#define GETHTMLGENINFO_FREE  0x01955003

#ifdef __cplusplus
extern "C" {
#endif

/*   Info function */
unsigned long getHTMLGeneratorInfo(int idx);


/*   Create/Delete functions   */
/*   =======================   */

int  createMyHTMLGenerator(streamHandle output);
int  deleteMyHTMLGenerator(void);

/*   HTML generation functions   */
/*   =========================   */


/*   General functions   */

void Body(const char *body);
void FormatedBody(const char *format, ...);

/*   Headings           */

void Heading1(const char *design);
void Heading2(const char *design);
void Heading3(const char *design);
void Heading4(const char *design);
void Heading5(const char *design);
void Heading6(const char *design);

void Heading1End(void);
void Heading2End(void);
void Heading3End(void);
void Heading4End(void);
void Heading5End(void);
void Heading6End(void);

/*   Table functions   */

void TableBegin(const char *tabledesign);
void TableEnd(void);
void RowBegin(const char *rowdesign);
void RowEnd(void);
void CellBegin(const char *celldesign);
void CellEnd(void);
void Cell(const char *cellbody);
void FormatedCell(const char *format, ...);
void LinkBegin(const char *url);
void LinkEnd(void);

/*   List functions   */

void OListBegin(const char *oldesign);
void OListEnd(void);
void UListBegin(const char *uldesign);
void UListEnd(void);
void DirListBegin(const char *dldesign);
void DirListEnd(void);
void MenuListBegin(const char *mldesign);
void MenuListEnd(void);
void List(const char *listbody);
void DefListBegin(const char *dfldesign);
void DefTerm(const char *term);
void DefDef(const char *definition);
void DefListEnd(void);

#ifdef __cplusplus
}
#endif
#endif
