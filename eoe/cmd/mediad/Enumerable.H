#ifndef Enumerable_included
#define Enumerable_included

class Enumerable {

public:

    const Enumerable *next() const	{ return _next; }
    const Enumerable *prev() const	{ return _prev; }
          Enumerable *next()		{ return _next; }
          Enumerable *prev()		{ return _prev; }
    unsigned int index() const;

protected:

    class Set {

    public:
//	constructor unneeded as long as all Sets are static...
//	Set()				: _n(0), _first(0), _last(0) { }
#ifndef NDEBUG
	Set();				// assertions only
	~Set();
#endif

	unsigned int      count()	{ return _n; }
	const Enumerable *first() const	{ return _first; }
	const Enumerable * last() const	{ return  _last; }
	      Enumerable *first()	{ return _first; }
	      Enumerable * last()	{ return  _last; }
	      Enumerable *  nth(unsigned int);

    private:
	unsigned int _n;
	Enumerable *_first, *_last;

    friend class Enumerable;
    };

    Enumerable(Set&);
    virtual ~Enumerable();

private:

    Set& _set;
    Enumerable *_next, *_prev;

    Enumerable(const Enumerable&);	// Do not copy
    void operator = (const Enumerable&);//  or assign.

};

//  Add ENUMERATION_METHODS to a derived class, and declare an enumerable
//  set.  E.g., to make Foo enumerable...
//
//	class Foo : private Enumerable {
//	public:
//	    //...
//	    ENUMERATION_METHODS(Foo, all_foos);
//	private:
//	    //...
//	    Enumerable::Set all_foos;
//	};

#define ENUMERATION_METHODS(type, set)					\
    const  type *next() const	{ return (type *) Enumerable::next(); }	\
    const  type *prev() const	{ return (type *) Enumerable::prev(); }	\
           type *next()		{ return (type *) Enumerable::next(); }	\
           type *prev()		{ return (type *) Enumerable::prev(); }	\
	   unsigned int index() const	{ return Enumerable::index(); } \
    static type *first()	{ return (type *) set.first(); }	\
    static type *last()	 	{ return (type *) set.first(); }	\
    static unsigned int count()	{ return set.count(); }			\
    static type *nth(unsigned int i) { return (type *) set.nth(i); }

#endif /* !Enumerable_included */
