#ifndef __TABLE__
#define __TABLE__
/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Table class and superclasses
 *
 *	$Revision: 1.3 $
 */

class textFont;
class emptyView;
class BorderPntView;

typedef struct {
	float		printWidth;	// Width of column in inches
	short		justify;
	short		select;
} columnFlag;


class Table {
	friend class	tableView;
	friend class	scrollTableView;
	friend class	borderedTable;
	friend class	titledTable;

protected:
	char**		tbl;
	char*		flags;
	columnFlag*	columnFlags;
	int		rowAllocSize;
	int		allocatedRows;
	int		numberOfRows;
	int		numberOfColumns;

	void		grow();

public:
	// Construct with allocation sizes

	Table(int row, int column);
	~Table();

	// Set the print widths of each column.  Default is 1 inch.

	void		setColumnWidths(float widths[]);

	// Get the size of the table

	int		getSize(void) { return numberOfRows; }

	// Set/Get entry table[row][column].  Set makes a copy.

	void		setEntry(int row, int column, char* entry);
	void		setEntry(int row, int column, int entry);
	void		getEntry(int row, int column, char** entry);
	void		getEntry(int row, int column, int* entry);

	// Clear all entries in the table

	void		clear();

	// Select and deselect:  No column argument means whole row

	void		select(int row, int column = -1);
	void		deselect(int row, int column = -1);
	Bool		isSelected(int row, int column = -1);

	// Justify columns

	void		leftJustify(int column);
	void		rightJustify(int column);

	// Enable and disable select on a column

	void		enableSelect(int column);
	void		disableSelect(int column);

	// Enable and disable an entry to overlap its column width

	void		enableOverlap(int row, int column);
	void		disableOverlap(int row, int column);

	// Print the table to stdout

	void		print();
};


class tableView : public tkView {
	friend class	scrollTableView;
	friend class	borderedTable;
	friend class	titledTable;

protected:
	int		topRow;
	int		rowOffset;
	textFont*	font;
	int		selectedRow;
	int		selectedColumn;
	Bool		selection;
	Bool		selectWholeRow;
	Bool		selectOneOnly;

	short		stdBackgroundColor;
	short		stdTextColor;
	short		selectBackgroundColor;
	short		selectTextColor;

	tkEventManager* eventManager;
	tkObject*	client;
	tkObject*	sender;

	void		sendEvent(long eventName);
	void		doSelect(int row, int column = -1,
					Bool postIt = FALSE, Bool cont = FALSE);

	int		round(float x) { return (int) (x + 0.5); }

public:
	Table*		table;

	tableView(int row, int column);
	~tableView();

	// Set the font to use

	void		setFont(char *name);

	// Select an entry

	void		select(int row, int column = -1)
				{ doSelect(row, column); }
	void		deselect(int row, int column = -1);
	Bool		isSelected(int row, int column = -1)
				{ return table->isSelected(row, column); }
	void		enableSelect(int column)
				{ table->enableSelect(column); }
	void		disableSelect(int column)
				{ table->disableSelect(column); }

	// Selection is enable if TRUE (default), and disabled if FALSE.

	void		setSelection(Bool select)
				{ selection = select; }

	// Select whole row if TRUE, just entry if FALSE (default).

	void		setRowSelection(Bool wholeRow)
				{ selectWholeRow = wholeRow; }

	// Select one entry or row at a time if TRUE, any if FALSE (default).

	void		setOneSelection(Bool oneSelect)
				{ selectOneOnly =  oneSelect; }

	// Send event on select / deselect

	void		setClient(tkObject* cl)
				{ client = cl; }

	void		setSender(tkObject* s)
				{ sender = s; }

	// Get the size of the table

	int		getSize(void)
				{ return table->getSize(); }

	// Set the colors

	void		setStdBackgroundColor(short c)
				{ stdBackgroundColor = c; }
	void		setStdTextColor(short c)
				{ stdTextColor = c; }
	void		setSelectBackgroundColor(short c)
				{ selectBackgroundColor = c; }
	void		setSelectTextColor(short c)
				{ selectTextColor = c; }

	// Justify columns

	void		leftJustify(int column);
	void		rightJustify(int column);

	// Enable and disable an entry to overlap its column width

	void		enableOverlap(int row, int column)
				{ table->enableOverlap(row, column); }
	void		disableOverlap(int row, int column)
				{ table->enableOverlap(row, column); }

	// View functions

	virtual void	resize();
	virtual void	paint();
	virtual void	repaint(int row, int column = -1);
	virtual void	repaintInContext(int row, int column = -1);
	virtual void	beginSelect(Point& p);
	virtual void	continueSelect(Point& p);
	virtual void	endSelect(Point& p);
};


class scrollTableView : public tkParentView {
	friend class	titledTable;
	friend class	borderedTable;

protected:
	tkScrollBar*	scrollBar;
	tableView*	tv;

public:
	scrollTableView(int row, int column);
	~scrollTableView();

	// Public functions from hidden layers (no multiple inheritance!)

	void		setFont(char* name)
				{ tv->setFont(name); }
	void		setRowSelection(Bool wholeRow)
				{ tv->setRowSelection(wholeRow); }
	void		setOneSelection(Bool oneSelect)
				{ tv->setOneSelection(oneSelect); }
	void		setStdBackgoundColor(short c)
				{ tv->setStdBackgroundColor(c); }
	void		setStdTextColor(short c)
				{ tv->setStdTextColor(c); }
	void		setSelectBackgroundColor(short c)
				{ tv->setSelectBackgroundColor(c); }
	void		setSelectTextColor(short c)
				{ tv->setSelectTextColor(c); }
	void		setColumnWidths(float widths[])
				{ tv->table->setColumnWidths(widths); }
	int		getSize(void)
				{ return tv->table->getSize(); }
	void		setEntry(int row, int column, char* entry)
				{ tv->table->setEntry(row, column, entry); }
	void		setEntry(int row, int column, int entry)
				{ tv->table->setEntry(row, column, entry); }
	void		getEntry(int row, int column, char** entry)
				{ tv->table->getEntry(row, column, entry); }
	void		getEntry(int row, int column, int* entry)
				{ tv->table->getEntry(row, column, entry); }
	void		leftJustify(int column)
				{ tv->table->leftJustify(column); }
	void		rightJustify(int column)
				{ tv->table->rightJustify(column); }
	void		select(int row, int column = -1)
				{ tv->select(row, column); }
	void		deselect(int row, int column = -1)
				{ tv->deselect(row, column); }
	Bool		isSelected(int row, int column = -1)
				{ return tv->table->isSelected(row, column); }
	void		enableSelect(int column)
				{ tv->enableSelect(column); }
	void		disableSelect(int column)
				{ tv->disableSelect(column); }
	void		enableOverlap(int row, int column)
				{ tv->enableOverlap(row, column); }
	void		disableOverlap(int row, int column)
				{ tv->disableOverlap(row, column); }

	// Clear and resize

	void		clear();

	// Send event on select / deselect

	void		setClient(tkObject* cl)
				{ tv->setClient(cl); tv->setSender(this); }

	// View functions

	virtual void	resize();
	virtual void	rcvEvent(tkEvent *e);
};


class borderedTable : public BorderPntView {
	friend class	titledTable;

protected:
	scrollTableView* stv;

public:
	borderedTable(int row, int column);
	~borderedTable();

	// Public functions from hidden layers (no multiple inheritance!)

	void		setFont(char* name)
				{ stv->tv->setFont(name); }
	void		setRowSelection(Bool wholeRow)
				{ stv->tv->setRowSelection(wholeRow); }
	void		setOneSelection(Bool oneSelect)
				{ stv->tv->setOneSelection(oneSelect); }
	void		setStdBackgoundColor(short c)
				{ stv->tv->setStdBackgroundColor(c); }
	void		setStdTextColor(short c)
				{ stv->tv->setStdTextColor(c); }
	void		setSelectBackgroundColor(short c)
				{ stv->tv->setSelectBackgroundColor(c); }
	void		setSelectTextColor(short c)
				{ stv->tv->setSelectTextColor(c); }
	void		setColumnWidths(float widths[])
				{ stv->tv->table->setColumnWidths(widths); }
	int		getSize(void)
				{ return stv->tv->table->getSize(); }
	void		setEntry(int row, int column, char* entry)
			    { stv->tv->table->setEntry(row, column, entry); }
	void		setEntry(int row, int column, int entry)
			    { stv->tv->table->setEntry(row, column, entry); }
	void		getEntry(int row, int column, char** entry)
			    { stv->tv->table->getEntry(row, column, entry); }
	void		getEntry(int row, int column, int* entry)
			    { stv->tv->table->getEntry(row, column, entry); }
	void		clear()
				{ stv->clear(); }
	void		leftJustify(int column)
				{ stv->tv->table->leftJustify(column); }
	void		rightJustify(int column)
				{ stv->tv->table->rightJustify(column); }
	void		select(int row, int column = -1)
				{ stv->tv->select(row, column); }
	void		deselect(int row, int column = -1)
				{ stv->tv->deselect(row, column); }
	Bool		isSelected(int row, int column = -1)
			    { return stv->tv->table->isSelected(row, column); }
	void		enableSelect(int column)
				{ stv->tv->enableSelect(column); }
	void		disableSelect(int column)
				{ stv->tv->disableSelect(column); }
	void		enableOverlap(int row, int column)
				{ stv->tv->enableOverlap(row, column); }
	void		disableOverlap(int row, int column)
				{ stv->tv->disableOverlap(row, column); }

	// Send event on select / deselect

	void		setClient(tkObject* cl)
				{ stv->tv->setClient(cl);
					stv->tv->setSender(this); }

	// View functions

	virtual void	resize();
};


class titledTable : public tkParentView {
protected:
	borderedTable*	bt;
	tableView*	title;
	emptyView*	leftover;

public:
	titledTable(int row, int column);
	~titledTable();

	// Set up column titles

	void		setColumns(float widths[], char* titles[]);
	void		setColumnTitles(char* titles[]);
	void		leftJustify(int column);
	void		rightJustify(int column);

	// Set table and/or title fonts

	void		setTableFont(char* name)
				{ bt->stv->tv->setFont(name); }
	void		setTitleFont(char* name)
				{ title->setFont(name); }
	void		setFont(char* name)
				{ setTableFont(name); setTitleFont(name); }

	// Public functions from hidden layers (no multiple inheritance!)

	void		repaint(int row, int column)
				{bt->stv->tv->repaint(row, column); }
	void		repaintInContext(int row, int column)
				{bt->stv->tv->repaintInContext(row, column); }

	void		setRowSelection(Bool wholeRow)
				{ bt->stv->tv->setRowSelection(wholeRow); }
	void		setOneSelection(Bool oneSelect)
				{ bt->stv->tv->setOneSelection(oneSelect); }
	void		setStdBackgoundColor(short c)
				{ bt->stv->tv->setStdBackgroundColor(c); }
	void		setStdTextColor(short c)
				{ bt->stv->tv->setStdTextColor(c); }
	void		setSelectBackgroundColor(short c)
				{ bt->stv->tv->setSelectBackgroundColor(c); }
	void		setSelectTextColor(short c)
				{ bt->stv->tv->setSelectTextColor(c); }
	void		setColumnWidths(float widths[])
				{ bt->stv->tv->table->setColumnWidths(widths); }
	int		getSize(void)
				{ return bt->stv->tv->table->getSize(); }
	void		setEntry(int row, int column, char* entry)
			  { bt->stv->tv->table->setEntry(row, column, entry); }
	void		setEntry(int row, int column, int entry)
			  { bt->stv->tv->table->setEntry(row, column, entry); }
	void		getEntry(int row, int column, char** entry)
			  { bt->stv->tv->table->getEntry(row, column, entry); }
	void		getEntry(int row, int column, int* entry)
			  { bt->stv->tv->table->getEntry(row, column, entry); }
	void		clear()
				{ bt->stv->clear(); }
	void		select(int row, int column = -1)
				{ bt->stv->tv->select(row, column); }
	void		deselect(int row, int column = -1)
				{ bt->stv->tv->deselect(row, column); }
	Bool		isSelected(int row, int column = -1)
			{ return bt->stv->tv->table->isSelected(row, column); }
	void		enableSelect(int column)
				{ bt->stv->tv->enableSelect(column); }
	void		disableSelect(int column)
				{ bt->stv->tv->disableSelect(column); }
	void		enableOverlap(int row, int column)
				{ bt->stv->tv->enableOverlap(row, column); }
	void		disableOverlap(int row, int column)
				{ bt->stv->tv->disableOverlap(row, column); }

	// Send event on select / deselect

	void		setClient(tkObject* cl)
				{ bt->stv->tv->setClient(cl);
					bt->stv->tv->setSender(this); }

	// View functions

	virtual void	resize();
};

#endif
