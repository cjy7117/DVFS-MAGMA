/*
    -- MAGMA (version 1.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2012

       @author Hatem Ltaief
       @author Mathieu Faverge

       @generated c Wed Nov 14 22:53:07 2012

*/
#include "common_magma.h"

#define magma_cgemm magmablas_cgemm
//#define magma_ctrsm magmablas_ctrsm
//#define magma_ctrmm magmablas_ctrmm

extern "C" magma_int_t
magma_cgessm_gpu( char storev, magma_int_t m, magma_int_t n, magma_int_t k, magma_int_t ib, 
                  magma_int_t *ipiv, 
                  cuFloatComplex *dL1, magma_int_t lddl1, 
                  cuFloatComplex *dL,  magma_int_t lddl, 
                  cuFloatComplex *dA,  magma_int_t ldda, 
                  magma_int_t *info)
{
/*  -- MAGMA (version 1.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2012

    Purpose
    =======

    CGESSM applies the factors L computed by CGETRF_INCPIV to
    a complex M-by-N tile A.
    
    Arguments
    =========

    M       (input) INTEGER
            The number of rows of the matrix A.  M >= 0.

    N       (input) INTEGER
            The number of columns of the matrix A.  N >= 0.

    K       (input) INTEGER
            The number of columns of the matrix L.  K >= 0.

    IB      (input) INTEGER
            The inner-blocking size.  IB >= 0.

    IPIV    (input) INTEGER array on the cpu.
            The pivot indices array of size K as returned by
            CGETRF_INCPIV.

    dL1     (input) DOUBLE COMPLEX array, dimension(LDDL1, N) 
            The IB-by-K matrix in which is stored L^(-1) as returned by GETRF_INCPIV
 
    LDDL1   (input) INTEGER
            The leading dimension of the array L1.  LDDL1 >= max(1,2*IB).
 
    dL      (input) DOUBLE COMPLEX array, dimension(LDDL, N) 
            The M-by-K lower triangular tile on the gpu.
 
    LDDL    (input) INTEGER
            The leading dimension of the array L.  LDDL >= max(1,M).

    dA      (input/output) DOUBLE COMPLEX array, dimension (LDDA, N)
            On entry, the M-by-N tile A on the gpu.
            On exit, updated by the application of L on the gpu.

    =====================================================================    */

#define AT(i,j) (dAT + (i)*ldda + (j)      )
#define L(i,j)  (dL  + (i)      + (j)*lddl )
#define dL1(j)  (dL1            + (j)*lddl1)

    cuFloatComplex c_one     = MAGMA_C_ONE;
    cuFloatComplex c_neg_one = MAGMA_C_NEG_ONE;

    int i, s, sb;
    cuFloatComplex *dAT;

    /* Check arguments */
    *info = 0;
    if (m < 0)
        *info = -1;
    else if (n < 0)
        *info = -2;
    else if (ldda < max(1,m))
        *info = -4;

    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }

    /* Quick return if possible */
    if (m == 0 || n == 0)
        return *info;

    if ( (storev == 'C') || (storev == 'c') ) {
        magmablas_cgetmo_in( dA, dAT, ldda, m, n );
    } else {
        dAT = dA;
    }

    s = k / ib;
    for(i = 0; i < k; i += ib) {
        sb = min(ib, k-i);

        magmablas_claswp( n, dAT, ldda, i+1, i+sb, ipiv, 1 );

#ifndef WITHOUTTRTRI
        magma_ctrmm( MagmaRight, MagmaLower, MagmaTrans, MagmaUnit, 
                     n, sb, 
                     c_one, dL1(i),   lddl1,
                            AT(i, 0), ldda);
#else
        magma_ctrsm( MagmaRight, MagmaLower, MagmaTrans, MagmaUnit, 
                     n, sb, 
                     c_one, L( i, i), lddl,
                            AT(i, 0), ldda);
#endif

        if ( (i+sb) < m) {
            magma_cgemm( MagmaNoTrans, MagmaTrans, 
                         n, m-(i+sb), sb, 
                         c_neg_one, AT(i,    0), ldda,
                                    L( i+sb, i), lddl, 
                         c_one,     AT(i+sb, 0), ldda );
        }
    }

    if ( (storev == 'C') || (storev == 'c') ) {
        magmablas_cgetmo_in( dA, dAT, ldda, m, n );
    }

    return *info;
    /* End of MAGMA_CGETRF_GPU */
}
