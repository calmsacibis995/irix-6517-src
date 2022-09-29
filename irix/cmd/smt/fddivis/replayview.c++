/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Table class and superclasses
 *
 *	$Revision: 1.4 $
 */

#include <bstring.h>
#include <tkTypes.h>
#include <stdParts.h>
#include <tkValuator.h>
#include <tkExec.h>
#include <events.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include "empty.h"
#include "replayview.h"

const float charactersPerInch = 10.0;
const float defaultColumnWidth = 1.0;	// Default width of columns in table

const char selectFlag = 0x01;
const char selectRowFlag = 0x02;
const char overlapFlag = 0x04;
const char leftJustifyFlag = 0x00;
const char rightJustifyFlag = 0x01;
const char canSelectFlag = 0x00;
const char noSelectFlag = 0x01;


Table::Table(int rows, int columns)
{
    tbl = 0;
    rowAllocSize = rows;
    allocatedRows = 0;
    numberOfColumns = columns;
    numberOfRows = 0;
    flags = 0;

    columnFlags = (columnFlag*) malloc(numberOfColumns *
						sizeof(columnFlag));
    if (columnFlags == 0) {
	perror("malloc failed");
	exit(-1);
    }
    for (int column = 0; column < numberOfColumns; column++) {
	columnFlags[column].printWidth = -1.0;
	columnFlags[column].justify = leftJustifyFlag;
    }
	
}


/*
 * Free all entries before freeing table
 */
Table::~Table()
{
    clear();
    free(columnFlags);
}


/*
 * Clear all entries from the table
 */
void
Table::clear()
{
    if (tbl != 0) {
	char** entry = tbl;
	char** last = tbl + numberOfRows * numberOfColumns;
	for ( ; entry < last; entry++) {
	    if (*entry != 0)
		free(*entry);
	}

	free(tbl);
	free(flags);

	tbl = 0;
	flags = 0;
	allocatedRows = 0;
	numberOfRows = 0;
    }
}


/*
 * Grow the table by rowAllocSize rows
 */
void
Table::grow()
{
    if (tbl == 0) {
	tbl = (char **) calloc(rowAllocSize * numberOfColumns, sizeof(char *));
	flags = (char *) calloc(rowAllocSize * numberOfColumns, sizeof(char));
    } else {
	tbl = (char **) realloc((char *) tbl, (allocatedRows + rowAllocSize) *
					numberOfColumns * sizeof(char *));
	bzero(tbl + allocatedRows * numberOfColumns,
			rowAllocSize * numberOfColumns * sizeof(char *));
	flags = (char *) realloc(flags, (allocatedRows + rowAllocSize) *
					numberOfColumns * sizeof(char));
	bzero(flags + allocatedRows * numberOfColumns,
			rowAllocSize * numberOfColumns * sizeof(char));
    }

    if ((tbl == 0) || (flags == 0)) {
	perror("malloc failed");
	exit(-1);
    }

    allocatedRows += rowAllocSize;
}


/*
 * Set the entry tbl[row][column] equal to the string
 */
void
Table::setEntry(int row, int column, char *entry)
{
    while (row >= allocatedRows)
	grow();

    if (row >= numberOfRows)
	numberOfRows = row + 1;

    char **e = tbl + row * numberOfColumns + column;

    if (*e != 0)
	free(*e);
	
    if (entry != 0)
	*e = strdup(entry);
    else
	*e = entry;
}


void
Table::setEntry(int row, int column, int entry)
{
    while (row >= allocatedRows)
	grow();

    if (row >= numberOfRows)
	numberOfRows = row + 1;

    char **e = tbl + row * numberOfColumns + column;

    if (*e != 0)
	free(*e);

    char buf[256];
    sprintf(buf, "%d", entry);
    *e = strdup(buf);
}


/*
 * Return the entry tbl[row][column]
 */
void
Table::getEntry(int row, int column, char** entry)
{
    if (row < numberOfRows && column < numberOfColumns)
	*entry = *(tbl + row * numberOfColumns + column);
    else
	*entry = 0;
}


void
Table::getEntry(int row, int column, int* entry)
{
    if (row < numberOfRows && column < numberOfColumns) {
	char *e = *(tbl + row * numberOfColumns + column);
	if (e != 0) {
	    *entry = atoi(e);
	    return;
	}
    }

    *entry = 0;
}


/*
 * Set the print width of each column
 */
void
Table::setColumnWidths(float widths[])
{
    for (int i = 0; i < numberOfColumns; i++)
	columnFlags[i].printWidth = widths[i];
}


/*
 * Select the row if column is < 0.
 */
void
Table::select(int row, int column)
{
    if ((row < 0) || (row >= numberOfRows))
	return;

    if (column < 0) {
	char *flag = flags + (row * numberOfColumns);
	for (int i = 0; i < numberOfColumns; i++, flag++)
	    *flag |= selectRowFlag;

	return;
    }

    if (columnFlags[column].select == noSelectFlag)
	return;

    char *flag = flags + (row * numberOfColumns) + column;
    *flag |= selectFlag;
}


/*
 * Select the row if column is < 0.
 */
Bool
Table::isSelected(int row, int column)
{
    if ((row < 0) || (row >= numberOfRows))
	return FALSE;

    if (column < 0) {
	char *flag = flags + (row * numberOfColumns);
	return *flag & selectRowFlag;
    }

    char *flag = flags + (row * numberOfColumns) + column;
    return *flag & selectFlag;
}


/*
 * Deselect the row if column is < 0.
 */
void
Table::deselect(int row, int column)
{
    if ((row < 0) || (row >= numberOfRows))
	return;

    if (column < 0) {
	char *flag = flags + (row * numberOfColumns);
	for (int i = 0; i < numberOfColumns; i++, flag++)
	    *flag &= ~selectRowFlag;

	return;
    }

    char *flag = flags + (row * numberOfColumns) + column;
    *flag &= ~selectFlag;
}


/*
 * Left justify the column (the default)
 */
void
Table::leftJustify(int column)
{
    if ((column >= 0) && (column < numberOfColumns))
	columnFlags[column].justify = leftJustifyFlag; 
}


/*
 * Right justify the column
 */
void
Table::rightJustify(int column)
{
    if ((column >= 0) && (column < numberOfColumns))
	columnFlags[column].justify = rightJustifyFlag; 
}


/*
 * Enable select on a column
 */
void
Table::enableSelect(int column)
{
    if ((column >= 0) && (column < numberOfColumns))
	columnFlags[column].select = canSelectFlag; 
}


/*
 * Enable select on a column
 */
void
Table::disableSelect(int column)
{
    if ((column >= 0) && (column < numberOfColumns))
	columnFlags[column].select = noSelectFlag; 
}


/*
 * Allow an entry to overstep its column width
 */
void
Table::enableOverlap(int row, int column)
{
    if (row < 0 || row >= numberOfRows ||
				column < 0 || column >= numberOfColumns)
	return;

    char *flag = flags + (row * numberOfColumns) + column;
    *flag |= overlapFlag;
}


/*
 * Disallow an entry from overstepping its column width
 */
void
Table::disableOverlap(int row, int column)
{
    if (row < 0 || row >= numberOfRows ||
				column < 0 || column >= numberOfColumns)
	return;

    char *flag = flags + (row * numberOfColumns) + column;
    *flag &= ~overlapFlag;
}


/*
 * Print the contents of the table to stdout
 */
void
Table::print()
{
    char buf[8];
    char** entry = tbl;
    char** last = tbl + numberOfRows * numberOfColumns;

    for (int column = 0; entry < last; entry++, column++) {
	if (column == numberOfColumns) {
	    putchar('\n');
	    column = 0;
	}
	if (columnFlags[column].printWidth == -1.0)
	    sprintf(buf, "%%%ds",
		(int) ((columnFlags[column].justify == leftJustifyFlag) ?
			defaultColumnWidth : -defaultColumnWidth) * 
				charactersPerInch);
	else
	    sprintf(buf, "%%%ds",
		(int) ((columnFlags[column].justify == leftJustifyFlag) ?
				columnFlags[column].printWidth :
				-columnFlags[column].printWidth) *
							charactersPerInch);
	if (*entry == 0)
	    printf(buf, " ");
	else
	    printf(buf, *entry);
    }
    putchar('\n');
}


/*
 * tableView
 */

const float pixelsPerInch = 96.0;

/* Margin in pixels of view */
const float viewLeftMargin = 1.0;
const float viewRightMargin = 1.0;
const float viewBottomMargin = 1.0;
const float viewTopMargin = 1.0;

tableView::tableView(int rows, int columns)
{
    table = new Table(rows, columns);
    topRow = 0;
    rowOffset = 0;
    font = tkGetDefaultFont();
    client = 0;
    sender = this;
    selection = TRUE;
    selectWholeRow = FALSE;
    selectOneOnly = FALSE;
    selectedRow = -1;
    selectedColumn = -1;
    stdBackgroundColor = 7;
    stdTextColor = 0;
    selectBackgroundColor = 0;
    selectTextColor = 15;
    eventManager = tkGetEventManager();
}


tableView::~tableView()
{
    delete table;
    delete font;
}


void
tableView::setFont(char* name)
{
    delete font;
    font = tkGetFont(name);
    if (font == 0)
	font = tkGetDefaultFont();
    resize();
}


void
tableView::resize()
{
}


void
tableView::paint()
{
    pushmatrix();
    pushviewport();
    localTransform();

	Box2 clipBox;
	int ox,oy;
	getClipBox(&clipBox,&ox,&oy);

        // viewport and ortho map entire bounds
        ::viewport(ox - 64, ox + getiExtentX() + 63,
				oy - 64, oy + getiExtentY() + 63);
        ::ortho2(-64.5, getExtentX() + 63.5,
				-64.5, getExtentY() + 63.5);

	withStdColor(BG_ICOLOR) {
	    sboxfi(0, 0, getiExtentX(), getiExtentY());
	}

	// set scrmask to map just the visible clip region
	::scrmask((short) (clipBox.xorg + viewLeftMargin),
		(short) (clipBox.xorg + clipBox.xlen - viewRightMargin),
		(short) (clipBox.yorg + viewTopMargin),
		(short) (clipBox.yorg + clipBox.ylen - viewBottomMargin) );

	::fmsetfont(font->getFontHandle());

	int fontHeight = font->getMaxHeight();
	int fontWidth = font->getMaxWidth();
	int fontDescent = font->getMaxDescender();
	float cy = getExtentY() - viewTopMargin + rowOffset;
	float ey = viewBottomMargin - fontHeight;

	for (int row = topRow; row < table->numberOfRows; row++) {
	    cy -= fontHeight;
	    float cx = viewRightMargin;
	    float ix = cx;
	    float ex = getExtentX() - viewLeftMargin;
	    int offset;
	    char* flag;
	    char** entry;

	    for (int column = 0; column < table->numberOfColumns; column++) {
		offset = row * table->numberOfColumns + column;
		flag = table->flags + offset;
		entry = table->tbl + offset;
		float width = table->columnFlags[column].printWidth *
								pixelsPerInch;
		int stringLength = 0;
		float justify = 0.0;
		float x = MAX(cx, ix);

		cx += width;
		if (cx < ix)
			continue;

		if (*entry != 0) {
		    stringLength = strlen(*entry);

		    /*
		     * If the string is wider than the print width, only image
		     * the characters that fit if the entry is not allowed to
		     * overlap.
		     */
		    if (fontWidth * stringLength > width) {
			if (*flag & overlapFlag)
			    width = stringLength * fontWidth;
			else
			    stringLength = (int) (width / fontWidth);
		    }

		    /*
		     * If the column is right justified, subtract the width of
		     * the entry from the width of the column to get the offset.
		     */
		    if (table->columnFlags[column].justify == rightJustifyFlag)
			justify = width - (stringLength * fontWidth);
		}

		if (*flag & selectRowFlag) {
		    color(selectBackgroundColor);
		    sboxfi(round(x), round(cy),
				round(x + width), round(cy + fontHeight));
		    color(selectTextColor);
		} else if (*flag & selectFlag) {
		    if (stringLength > 0) {
			color(selectBackgroundColor);
			sboxfi(round(x + justify), round(cy),
				round(x + stringLength * fontWidth),
						round(cy + fontHeight));
			color(selectTextColor);
		    }
		} else
		    color(stdTextColor);

		if (*entry != 0)
		    font->imageSpan(*entry, stringLength,
				round(x + justify), round(cy + fontDescent));

		ix += width;
		if (ix >= ex)
		    break;
	    }

	    if ((cx < ex) && (*flag & selectRowFlag)) {
		color(selectBackgroundColor);
		sboxfi(round(cx), round(cy), round(ex), round(cy + fontHeight));
	    }

	    if (cy <= ey)
		break;
	}

    popviewport();
    popmatrix();
}


void
tableView::repaint(int row, int column)
{
    if ((row < topRow) || (row >= table->numberOfRows) ||
					(column >= table->numberOfColumns))
	return;

    pushmatrix();
    pushviewport();
    localTransform();

	Box2 clipBox;
	int ox,oy;
	getClipBox(&clipBox,&ox,&oy);

        // viewport and ortho map entire bounds
        ::viewport(ox - 64, ox + getiExtentX() + 63,
				oy - 64, oy + getiExtentY() + 63);
        ::ortho2(-64.5, getExtentX() + 63.5,
				-64.5, getExtentY() + 63.5);

	// set scrmask to map just the visible clip region
	::scrmask((short) (clipBox.xorg + viewLeftMargin),
		(short) (clipBox.xorg + clipBox.xlen - viewRightMargin),
		(short) (clipBox.yorg + viewTopMargin),
		(short) (clipBox.yorg + clipBox.ylen - viewBottomMargin) );

	::fmsetfont(font->getFontHandle());

	int fontHeight = font->getMaxHeight();
	int fontWidth = font->getMaxWidth();
	int fontDescent = font->getMaxDescender();
	float cy = getExtentY() - viewTopMargin + rowOffset -
						(row - topRow) * fontHeight;
	cy -= fontHeight;
	float cx = viewRightMargin;
	float ix = cx;
	float ex = getExtentX() - viewLeftMargin;
	int offset;
	char* flag;
	char** entry;

	if (column < 0) {
	    for (column = 0; column < table->numberOfColumns; column++) {
		offset = row * table->numberOfColumns + column;
		flag = table->flags + offset;
		entry = table->tbl + offset;
		float width = table->columnFlags[column].printWidth *
								pixelsPerInch;
		int stringLength = 0;
		float justify = 0.0;
		float x = MAX(cx, ix);

		cx += width;
		if (cx < ix)
			continue;

		if (*entry != 0) {
		    stringLength = strlen(*entry);
		    
		    if (fontWidth * stringLength > width) {
			if (*flag & overlapFlag)
			    width = stringLength * fontWidth;
			else
			    stringLength = (int) (width / fontWidth);
		    }

		    if (table->columnFlags[column].justify == rightJustifyFlag)
			justify = width - (stringLength * fontWidth);
		}

		if (*flag & selectRowFlag) {
		    color(selectBackgroundColor);
		    sboxfi(round(x), round(cy),
				round(x + width), round(cy + fontHeight));
		    color(selectTextColor);
		} else if (*flag & selectFlag) {
		    if (stringLength > 0) {
			color(selectBackgroundColor);
			sboxfi(round(x + justify), round(cy),
				round(x + stringLength * fontWidth),
						round(cy + fontHeight));
			color(selectTextColor);
		    }
		} else {
		    withStdColor(BG_ICOLOR) {
			sboxfi(round(x), round(cy),
				round(x + width), round(cy + fontHeight));
		    }
		    color(stdTextColor);
		}

		if (*entry != 0)
		    font->imageSpan(*entry, stringLength,
				round(x + justify), round(cy + fontDescent));

		ix += width;
		if (ix >= ex)
		    break;
	    }

	    if (cx < ex) {
		if (*flag & selectRowFlag) {
		    color(selectBackgroundColor);
		    sboxfi(round(cx), round(cy), round(ex),
					round(cy) + round(fontHeight));
		} else {
		    // color(stdBackgroundColor);
		    withStdColor(BG_ICOLOR) {
			sboxfi(round(cx), round(cy), round(ex),
					round(cy + fontHeight));
		    }
		}
	    }
	} else {
	    for (int i = 0; i < column; i++)
		cx += pixelsPerInch * table->columnFlags[i].printWidth;

	    if (cx < ex) {
		offset = row * table->numberOfColumns + column;
		flag = table->flags + offset;
		entry = table->tbl + offset;
		float width = table->columnFlags[column].printWidth *
								pixelsPerInch;
		int stringLength = 0;
		float justify = 0.0;

		if (*entry != 0) {
		    stringLength = strlen(*entry);
		    
		    if (fontWidth * stringLength > width) {
			if (*flag & overlapFlag)
			    width = stringLength * fontWidth;
			else
			    stringLength = (int) (width / fontWidth);
		    }

		    if (table->columnFlags[column].justify == rightJustifyFlag)
			justify = width - (stringLength * fontWidth);
		}

		if (*flag & selectRowFlag) {
		    color(selectBackgroundColor);
		    sboxfi(round(cx), round(cy),
				round(cx + width), round(cy + fontHeight));
		    color(selectTextColor);
		} else if (*flag & selectFlag) {
		    if (stringLength > 0) {
			color(selectBackgroundColor);
			sboxfi(round(cx + justify), round(cy),
				round(cx + stringLength * fontWidth),
						round(cy + fontHeight));
			color(selectTextColor);
		    }
		} else {
		    withStdColor(BG_ICOLOR) {
			sboxfi(round(cx), round(cy),
				round(cx + width), round(cy + fontHeight));
		    }
		    color(stdTextColor);
		}

		if (*entry != 0)
		    font->imageSpan(*entry, stringLength, round(cx + justify),
						round(cy + fontDescent));
	    }
	}

    popviewport();
    popmatrix();
}


void
tableView::repaintInContext(int row, int column)
{
#ifdef CYPRESS_XGL
    tkGID gid = getParentWindow()->getgid();
    if (gid == tkInvalidGID)
	return;
    tkCONTEXT savecx = tkWindow::pushContext(getParentWindow()->getContext());
#else
    long gid = getgid();
    if (gid < 0)
	return;
    long savegid = winget();
    winset(gid);
#endif
    pushmatrix();
    ::pushviewport();
	if (parent)
	    parent->globalTransform();
	frontbuffer(TRUE);
	this->repaint(row, column);
	frontbuffer(FALSE);
    ::popviewport();
    popmatrix();
#ifdef CYPRESS_XGL
    tkWindow::popContext(savecx);
#else
    winset(savegid);
#endif
}


void
tableView::beginSelect(Point& p)
{
    if (!selection)
	return;

    Box2 bounds = getBounds();
    if (!bounds.inside(p))
	return;

    int row = (getiExtentY() + rowOffset - (p.getiY() - getiOriginY())) /
					font->getMaxHeight() + topRow;

    if ((row < topRow) || (row >= table->numberOfRows))
	return;

    if (selectWholeRow) {
	doSelect(row, -1, TRUE, FALSE);
	return;
    }

    float px = (p.getX() - bounds.xorg) / pixelsPerInch;

    for (int column = 0; column < table->numberOfColumns; column++) {
	px -= table->columnFlags[column].printWidth;
	if (px <= 0.0) {
	    doSelect(row, column, TRUE, FALSE);
	    break;
	}
    }
}


void
tableView::continueSelect(Point& p)
{
    if (!selection)
	return;

    Box2 bounds = getBounds();
    if (!bounds.inside(p))
	return;

    int row = (getiExtentY() + rowOffset - (p.getiY() - getiOriginY())) /
					font->getMaxHeight() + topRow;

    if ((row < topRow) || (row >= table->numberOfRows))
	return;

    if (selectWholeRow) {
	doSelect(row, -1, TRUE, TRUE);
	return;
    }

    float px = (p.getX() - bounds.xorg) / pixelsPerInch;

    for (int column = 0; column < table->numberOfColumns; column++) {
	px -= table->columnFlags[column].printWidth;
	if (px <= 0.0) {
	    doSelect(row, column, TRUE, TRUE);
	    break;
	}
    }
}


void
tableView::endSelect(Point& p)
{
    continueSelect(p);
}


void
tableView::doSelect(int row, int column, Bool postIt, Bool cont)
{
    if ((row < 0) || (row >= table->numberOfRows))
	return;

    if (selectWholeRow || (column == -1)) {
	if (table->isSelected(row)) {
	    if (postIt && !cont)
		sendEvent(tkEVENT_SELECTED);
	    return;
	}

	if (selectOneOnly && (selectedRow != -1)) {
	    table->deselect(selectedRow, selectedColumn);
	    if (postIt)
		sendEvent(tkEVENT_DESELECTED);
	    repaintInContext(selectedRow, selectedColumn);
	}

	selectedRow = row;
	selectedColumn = -1;
	table->select(selectedRow);
	if (postIt)
	    sendEvent(tkEVENT_SELECTED);
	repaintInContext(selectedRow);
	return;
    }

    if ((column < 0) || (column >= table->numberOfColumns))
	return;

    if (table->columnFlags[column].select == noSelectFlag)
	return;

    if (table->isSelected(row, column)) {
	if (postIt && !cont)
	    sendEvent(tkEVENT_SELECTED);
	return;
    }

    if (selectOneOnly && (selectedRow != -1)) {
	table->deselect(selectedRow, selectedColumn);
	if (postIt)
	    sendEvent(tkEVENT_DESELECTED);
	repaintInContext(selectedRow, selectedColumn);
    }

    selectedRow = row;
    selectedColumn = column;
    table->select(selectedRow, selectedColumn);
    if (postIt)
	sendEvent(tkEVENT_SELECTED);
    repaintInContext(selectedRow, selectedColumn);
}


void
tableView::deselect(int row, int column)
{
    if ((row < 0) || (row >= table->numberOfRows))
	return;

    if (!table->isSelected(row, column))
	return;

    table->deselect(row, column);
    repaintInContext(row, column);
}


void
tableView::sendEvent(long eventName)
{
    if (client != 0) {
	int value;
	short* svalue = (short*) &value;
	svalue[0] = selectedRow;
	svalue[1] = selectedColumn;
	tkValueEvent* event = new tkValueEvent(eventName, client);
	event->setSender(sender);
	event->setValue(value);
	eventManager->postEvent(event, FALSE);
    }
}


/*
 * scrollTableView
 */
scrollTableView::scrollTableView(int rows, int columns)
{
    tv = new tableView(rows, columns);
    tv->setClient(this);
    addAView(*tv);

    tkPen dummypen(WHITE,345,BLUE,0);
    Box2 sbarBox(0.0,0.0,SCROLLWIDTH,5.0 * SCROLLWIDTH);
    scrollBar = makeStandardScrollBar(FALSE,sbarBox,dummypen);
    scrollBar->getValuator()->setLowerBound(0.0);
    scrollBar->getValuator()->setUpperBound(1.0);
    scrollBar->setIncValue(0.1);
    scrollBar->setDecValue(0.1);
    scrollBar->getValuator()->setCurrentValue(0.0);
    scrollBar->setClient(this);
    addAView(*scrollBar);
}


scrollTableView::~scrollTableView()
{
    delete tv;
    delete scrollBar;
}


void
scrollTableView::resize()
{
    Box2 bounds = getBounds();
    Box2 scrollBounds(-1.0, 0.0, SCROLLWIDTH, bounds.ylen + 2.0);
    resizeStandardScrollBar(scrollBar, scrollBounds);

    float viewHeight = getiExtentY() - viewTopMargin - viewBottomMargin;
    float fontHeight = tv->font->getMaxHeight();
    float contentHeight = tv->table->numberOfRows * fontHeight;

    if (contentHeight > viewHeight) {
	tkValuator *val = scrollBar->getValuator();
	val->setLowerBound(contentHeight - viewHeight);
	val->setUpperBound(0.0);
	val->setPercentageShown(viewHeight, contentHeight);
	scrollBar->setIncValue(-fontHeight);
	scrollBar->setDecValue(-fontHeight);
	scrollBar->setPageIncValue(-viewHeight);
	scrollBar->setPageDecValue(-viewHeight);
	scrollBar->enable();
    } else
	scrollBar->disable();

    Box2 tableBounds(SCROLLWIDTH - 1.0, 0.0,
				bounds.xlen - SCROLLWIDTH, bounds.ylen);
    tv->setBounds(tableBounds);
    tv->resize();
}


void
scrollTableView::clear()
{
    tv->table->clear();
    scrollBar->getValuator()->setCurrentValue(0.0);
    tv->topRow = 0;
    tv->rowOffset = 0;
    resize();
}


void
scrollTableView::rcvEvent(tkEvent *e)
{
    switch(e->name()) {
	case tkEVENT_VALUECHANGE: {
	    int offset = (int) scrollBar->getValuator()->getCurrentValue();
	    tv->topRow = offset / tv->font->getMaxHeight();
	    tv->rowOffset = offset % tv->font->getMaxHeight();
	    tv->paintInContext();
	} break;

	default:
	    tkParentView::rcvEvent(e);
    }
}


/*
 * borderedTable
 */
borderedTable::borderedTable(int rows, int columns)
{
    stv = new scrollTableView(rows, columns);
    stv->setClient(this);
    addView(*stv);
}


borderedTable::~borderedTable()
{
    delete stv;
}


void
borderedTable::resize()
{
    Box2 bounds = getBounds();
    tkPen p(-1, -1, -1, -1);
    tkBorderModel* borderModel = new tkBorderModel(bounds, p);
    setBorderModel(borderModel);

    Box2 tableBounds((float) BORDER_LS, BORDER_BS - 1.0,
				bounds.xlen - BORDER_LS - BORDER_RS - 1.0,
				bounds.ylen - BORDER_TS - BORDER_BS - 1.0);
    stv->setBounds(tableBounds);
    stv->resize();
}


/*
 * titledTable
 */
titledTable::titledTable(int rows, int columns)
{
    leftover = new emptyView();
    addAView(*leftover);

    title = new tableView(1, columns);
    title->setSelection(FALSE);
    addAView(*title);

    bt = new borderedTable(rows, columns);
    bt->stv->setClient(this);
    addAView(*bt);
}


titledTable::~titledTable()
{
    delete bt;
    delete title;
}


void
titledTable::resize()
{
    Box2 bounds = getBounds();
    float fontHeight = bt->stv->tv->font->getMaxHeight();

    float titleHeight = fontHeight + viewTopMargin + viewBottomMargin;
    Box2 titleBounds(SCROLLWIDTH + BORDER_LS,
			bounds.ylen - 1.0 - titleHeight,
			bounds.xlen - SCROLLWIDTH - BORDER_LS + 2.0,
						titleHeight + 2.0);
    title->setBounds(titleBounds);
    title->resize();

    Box2 tableBounds(0.0, 0.0, bounds.xlen, bounds.ylen - titleHeight);
    bt->setBounds(tableBounds);
    bt->resize();

    Box2 leftoverBounds(0.0, bounds.ylen - titleHeight,
				SCROLLWIDTH + BORDER_LS, titleHeight);
    leftover->setBounds(leftoverBounds);
    leftover->resize();
}


void
titledTable::setColumnTitles(char* titles[])
{
    for (int column = 0; column < title->table->numberOfColumns; column++)
	title->table->setEntry(0, column, titles[column]);
}


void
titledTable::setColumns(float widths[], char* titles[])
{
    bt->stv->tv->table->setColumnWidths(widths);
    title->table->setColumnWidths(widths);
    setColumnTitles(titles);
}


void
titledTable::leftJustify(int column)
{
    bt->stv->tv->table->leftJustify(column);
    title->table->leftJustify(column);
}


void
titledTable::rightJustify(int column)
{
    bt->stv->tv->table->rightJustify(column);
    title->table->rightJustify(column);
}
