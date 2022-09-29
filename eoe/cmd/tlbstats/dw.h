typedef unsigned dw[2];

#define ZERO(_dw) \
    {_dw[0] = 0; _dw[1] = 0;}

#if defined(MIPSEB) || defined(MIPSEL)

/* SLTU instruction makes this the best way */

#define INC1(_dw) \
    {_dw[0] += 1; _dw[1] += (_dw[0] == 0);}

#define INC(_dw, _i) \
    {_dw[0] += (_i); _dw[1] += (_dw[0] < (unsigned)(_i));}

#define ADD(_a, _b) \
    {_a[0] += _b[0]; _a[1] = _a[1] + _b[1] + (_a[0] < _b[0]);}

#define DEC1(_dw) \
    {_dw[1] -= (_dw[0] == 0); _dw[0] -= 1;}

#else

#define INC1(_dw) \
    {_dw[0] += 1; if (_dw[0] == 0) _dw[1] += 1;}

#define INC(_dw, _i) \
    {_dw[0] += _i; if (_dw[0] < (unsigned)(_i)) _dw[1] += 1;}

#define ADD(_a, _b) \
    {_a[0] += _b[0]; _a[1] += _b[1]; if (_a[0] < _b[0]) _a[1] += 1;}

#endif

#define DOUBLE(_dw) \
    ((double)_dw[0] + (double)_dw[1] * 4294967296.0)
