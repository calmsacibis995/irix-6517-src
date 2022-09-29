#ifndef Bag_included
#define Bag_included

//  Bag is a template class for a container.  A Bag has the following
//  properties.
//
//	A bag may contain multiple copies of the same thing (unlike,
//	e.g., a Set).
//
//	Things in a bag are not ordered.
//
//	You can't enumerate the things in a bag.  You can only remove
//	them, one at a time.
//
//  A Bag is implemented as a linked list of small arrays of elements.
//  The small arrays are called Chunks.

template <class T> class Bag {

public:

    Bag(int = 0)			: p(0) { }
    virtual ~Bag();

    Bag& operator = (const Bag&);
    operator int ()			{ return !empty(); }

    void insert(const T&);
    T take();
    int empty() const			{ return !p; }
    
private:

    enum { ENUF = 4 };

    struct Chunk {
	Chunk(Chunk *np = 0)		: n(0), next(np) { }
	int n;
	Chunk *next;
	T them[ENUF];
    };

    //  Instance Variable

    Chunk *p;

    // Private Instance Methods

    Bag(const Bag&);			// Not implemented.

};

#endif /* !Bag_included */
