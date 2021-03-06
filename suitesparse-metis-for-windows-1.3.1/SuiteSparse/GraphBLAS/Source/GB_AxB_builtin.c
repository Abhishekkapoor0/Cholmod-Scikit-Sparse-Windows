//------------------------------------------------------------------------------
// GB_AxB_builtin:  hard-coded C=A*B for built-in types
//------------------------------------------------------------------------------

// SuiteSparse:GraphBLAS, Timothy A. Davis, (c) 2017-2018, All Rights Reserved.
// http://suitesparse.com   See GraphBLAS/Doc/License.txt for license.

//------------------------------------------------------------------------------

// This function computes C=A*B with hard-coded versions for all 960 unique
// built-in semirings that can be constructed with built-in operators.  It also
// handles all non-unique built-in semirings, by renaming operators to
// equivalent ones; if these are included, this function computes C=A*B for all
// 1712 valid semirings that can be constructed from built-in operators.

// FUTURE: this can be done in parallel.  The computation of each column C(:,j)
// is an independent task.  Each thread would need its own Flag and Work array.

#include "GB.h"

#ifndef GBCOMPACT

#include "GB_AxB_methods.h"

// A semiring is defined by a binary "multiply" operator, and an associative
// "add" monoid.  For a built-in semiring, the multiply op can be any one of
// 256 built-in binary operators.

//------------------------------------------------------------------------------
// Counting all valid multiply operators and all unique ones:
//------------------------------------------------------------------------------

// 17 where z, x, and y are all of the same type (one of 11 types)

//      'first',       z = x
//      'second',      z = y
//      'min',         z = min(x,y)
//      'max',         z = max(x,y)
//      'plus',        z = x + y
//      'minus',       z = x - y
//      'times',       z = x * y
//      'div',         z = x / y
//      'iseq'         z = (x == y)
//      'isne'         z = (x != y)
//      'isgt'         z = (x >  y)
//      'islt'         z = (x <  y)
//      'isge'         z = (x >= y)
//      'isle'         z = (x <= y)
//      'or'           z = x || y
//      'and'          z = x && y
//      'xor'          z = x != y

// 6 where x, and y are the same type (one of 11) but z is boolean:

//      'eq'           z = (x == y)
//      'ne'           z = (x != y)
//      'gt'           z = (x >  y)
//      'lt'           z = (x <  y)
//      'ge'           z = (x >= y)
//      'le'           z = (x <= y)

// All 23 built-in operators have a "_TYPE" suffix in their name and they 
// work on all 11 built-in types (for x and y):

//      GraphBLAS type      C language type
//      GrB_BOOL            bool
//      GrB_INT8            int8_t
//      GrB_UINT8           uint8_t
//      GrB_INT16           int16_t
//      GrB_UINT16          uint16_t
//      GrB_INT32           int32_t
//      GrB_UINT32          uint32_t
//      GrB_INT64           int64_t
//      GrB_UINT64          uint64_t
//      GrB_FP32            float
//      GrB_FP64            double

// There are 3 more where x,y,z are all boolean, but they are the same as their
// GrB_L*BOOL counterparts.  When used here, these three are indistinguishable
// from their *_BOOL counterparts (they have the same type and opcode, and even
// the same struct: GrB_LOR == GxB_LOR_BOOL, GrB_LAND == GxB_LAND_BOOL, and
// GrB_LXOR == GxB_LXOR_BOOL are all true):

//      'or'           z = x || y       GRB_LOR, with no suffix
//      'and'          z = x && y       GRB_LAND, with no suffix
//      'xor'          z = x != y       GRB_LXOR, with no suffix

// There are a total of 17*11 + 6*11 + 3 = 256 named built-in binary operators.

// For boolean x and y, however: some multiply operators are redundant:
// (first=div), (and=min=times), (or=max=plus), (xor=minus=ne=isne) (is=iseq),
// (gt=isgt), (lt=islt), (ge=isge), (le=isle), and the three operators with no
// "_BOOL" suffix.  This removes 16 operators.

// Redundant operators are handled by renaming them internally in this
// function.  The GrB_LOR and GxB_LOR_BOOL operators have the same type
// and opcode, so this function doesn't need to do anything to rename
// the operator.  Likewise GrB_LAND and GrB_XOR.

// Total number of named built-in binary operators:  256
// Total number of unique built-in binary operators: 240

//------------------------------------------------------------------------------
// Counting all valid monoids and all unique ones:
//------------------------------------------------------------------------------

// In a semiring, the add monoid z=f(x,y) must be commutative, associative,
// and have an additive identity, and the types z, x, and y must all be in the
// same domain, so the valid built-in add monoids are:

// 4 monoids where z can be any of the 11 types (4*11 = 44 monoids),
// but in the boolean case they are redundant, giving 4*10 = 40 unique ones:
//      'min',         z = min(x,y) : identity is +inf
//      'max',         z = max(x,y) : identity is -inf
//      'plus',        z = x + y    : identity is 0
//      'times',       z = x * y    : identity is 1

// 8 monoids are valid for only boolean z (4 redundant):
//      'or'           z = x || y   : identity is false (GrB_LOR and LOR_BOOL)
//      'and'          z = x && y   ; identity is true  (GrB_LAND and LAND_BOOL)
//      'xor'          z = x != y   ; identity is false (GrB_LXOR and LXOR_BOOL)
//      'eq'           z = x == y   ; identity is true  (GrB_*EQ_BOOL)

// For non-boolean x,y, z=eq(x,y) fails as a monoid since the types aren't the
// same.  z=iseq(x,y) fails as a monoid for non-boolean x,y,z because it is
// non-associative, and it also has no identity value.  The others (or,and,xor)
// also have no identity value for the non-boolean case.

// For boolean x and y, 4 monoids (min,max,plus,times) are the same as boolean
// monoids (times=min=and), (max=plus=or), and thus the 4 monoids
// (min,max,plus,times) only need to be applied to non-boolean inputs.  If
// given one of these opcodes on boolean inputs, the opcode is renamed to its
// equivalent boolean opcode.

//      Total valid monoids with  (min,max,plus,times): 44: 40 nonbool, 4 bool
//      Total unique monoids with (min,max,plus,times): 40: all non-boolean
//      Total valid monoids with  (or,and,xor,eq):      8:  all boolean
//      Total unique monoids with (or,and,xor,eq):      4:  all boolean

//      Total valid  monoids: 52 (44 + 8): 40 for non-boolean x,y; 12 boolean
//      Total unique monoids: 44 (40 + 4): 40 for non-boolean x,y; 4 boolean

//------------------------------------------------------------------------------
// Counting all valid semirings and all unique ones:
//------------------------------------------------------------------------------

// 17 multiply operators z=f(x,y) where all 3 x,y,z have the same type:

//      10 non-boolean cases: 4 valid monoids
//      boolean case: 12 valid monoids, but only 4 unique
//      When x,y,z are boolean, 12 multiply operators are redundant

//      Total valid semirings:  17 * 52 = 884
//      Total unique semirings: 220 + 480 = 700, see the count below

//          for 5 multiply operators (1st,2nd,or,and,xor):
//              non-boolean x,y,z: 10 types * 4 monoids (min,max,plus,times)
//              boolean x,y,z: 4 monoids (or,and,xor,eq)
//              total: 5 ops * (10*4 + 4) = 220

//          for 12 multiply operators (min,max,plus,minus,times,div,is*):
//              non-boolean x,y,z: 10 types * 4 monoids (min,max,plus,times)
//              total: 12 ops * (10*4) = 480

// 3 multiply operators z=f(x,y) where all three 3 x,y,z are boolean

//      the 3 operators are GrB_LOR, GrB_LAND, and GrB_LXOR, without the _BOOL
//      suffix
//      12 valid monoids:
//      Total valid semirings: 3*12  = 36
//      Total unique semirings: 0, since these 3 ops are redundant with
//          the same three in the set above, with _BOOL suffixes

// 6 multiply ops z=f(x,y) where x,y are any of 11 types, but z is boolean:

//      z is always boolean: 12 valid boolean monoids but only 4 are unique
//      when x,y are boolean, one multiply operator (ne) is redundant

//      Total valid semirings:  6 * 11 * 12 = 792
//      Total unique semirings: 220 + 40 = 260, see the count below

//          for 5 multiply operators (eq,gt,lt,ge,le):
//              non-boolean x,y: 10 types * 4 monoids (or,and,xor,eq)
//              boolean x,y: 4 monoids (or,and,xor,eq)
//              total: 5 ops * (10*4 + 4) = 220

//          for 1 multiply operator (ne):
//              non-boolean x,y: 10 types * 4 monoids (or,and,xor,eq)
//              total: 1 op * (10*4) = 40

// Total valid semirings:  884 + 3*12 + 792 = 1712
// Total unique semirings: 700 +    0 + 260 =  960

// Splitting the count of unique semirings, for x,y non-boolean:
// (17*40): 680 x,y,z all nonboolean
// (6*40) = 240 x,y non-boolean, z boolean
// (5 + 5)*4 = 40, x,y,z all boolean

// This function handles all 1712 semirings using 960 workers, by renaming
// redundant multiply and monoid operators to their equivalent counterparts.

//------------------------------------------------------------------------------

bool GB_AxB_builtin                 // true if C=A*B is handled
(
    GrB_Matrix C,                   // output matrix
    const GrB_Matrix Mask,          // Mask matrix for C<M> (not complemented)
    const GrB_Matrix A,             // input matrix
    const GrB_Matrix B,             // input matrix
    const GrB_Semiring semiring,    // semiring that defines C=A*B
    const bool flipxy               // if true, do z=fmult(b,a) vs fmult(a,b)
)
{

    //--------------------------------------------------------------------------
    // check inputs
    //--------------------------------------------------------------------------

    if (Mask == NULL)
    {
        // C contains the pattern of C=A*B
        ASSERT_OK (GB_check (C, "C input for builtin AxB", 0)) ;
    }
    else
    {
        // Mask is present.  C->p and C->i are allocated but not initialized
        ASSERT (C != NULL && C->p != NULL && C->i != NULL) ;
        ASSERT (!C->p_shallow && !C->i_shallow) ;
        ASSERT_OK (GB_check (Mask, "Mask input for builtin AxB", 0)) ;
    }
    ASSERT_OK (GB_check (A, "A for builtin AxB", 0)) ;
    ASSERT_OK (GB_check (B, "B for builtin AxB", 0)) ;
    ASSERT (!PENDING (C)) ; ASSERT (!ZOMBIES (C)) ;
    ASSERT (!PENDING (A)) ; ASSERT (!ZOMBIES (A)) ;
    ASSERT (!PENDING (B)) ; ASSERT (!ZOMBIES (B)) ;
    ASSERT_OK (GB_check (semiring, "semiring for builtin", 0)) ;
    ASSERT (C->type == semiring->add->op->ztype) ;

    GB_Opcode mult_opcode, add_opcode ;
    GB_Type_code xycode, zcode ;

    // check if the semiring is builtin, and if so, get opcodes and type codes
    if (!GB_semiring_builtin (A, B, semiring, flipxy,
        &mult_opcode, &add_opcode, &xycode, &zcode))
    {
        return (false) ;
    }

    //--------------------------------------------------------------------------
    // define the worker for the switch factory
    //--------------------------------------------------------------------------

    // When the worker is done, it returns directly to GB_AxB_numeric.  This
    // simplifies the logic of the switch factory, since no breaks are needed.
    // Any "if" conditions only need to consider the cases they handle.  Any
    // cases not handled simply fall-through to the next set of tests.  This
    // allows this function to be easily extended in the future, with
    // additional hard-coded workers, if the set of built-in operators is
    // extended.

    #define GB_AXB(add,mult,xyname) GB_AxB_ ## add ## mult ## xyname

    #define AxB(add,mult,xyname) \
    { \
        GB_AXB (add,mult,xyname) (C, Mask, A, B, flipxy) ; \
        return (true) ; \
    }

    //--------------------------------------------------------------------------
    // launch the switch factory
    //--------------------------------------------------------------------------

    #include "GB_AxB_factory.c"
    #undef AxB
    #undef GB_AXB

    //--------------------------------------------------------------------------
    // no built-in worker for this semiring
    //--------------------------------------------------------------------------

    return (false) ;
}

#endif

