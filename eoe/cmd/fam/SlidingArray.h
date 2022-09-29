#ifndef SlidingArray_included
#define SlidingArray_included

//  SlidingArray is like an unbounded array.  New elements are
//  automatically appended to the array, and zero elements are
//  automatically reclaimed off the left end.
//
//  A SlidingArray is most useful where a small, contiguous segment of
//  a large space has nonzero elements.
//
//  SlidingArray is used by ServerHost to implement the queue of deferred
//  scans.

template <class T> class SlidingArray {

public:

    SlidingArray(unsigned = 0);
    ~SlidingArray();

    T& operator [] (signed);
    const T& operator [] (signed i) const
					{ i -= offset;
					  if (i < size) return p[i];
					  else return zero; }
    signed int low_index()		{ return offset; }
    signed int high_index()		{ return offset + size; }

private:

    T *p;
    signed int offset;
    unsigned int size;
    unsigned int nalloc;
    T zero;

    SlidingArray(const SlidingArray&);	// Not implemented.
    operator = (const SlidingArray&);	// Not implemented.

};

#endif /* !SlidingArray_included */
