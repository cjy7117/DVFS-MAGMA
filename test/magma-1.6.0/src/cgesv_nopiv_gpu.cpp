/*
    -- MAGMA (version 1.6.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2014
       @author Adrien REMY

       @generated from zgesv_nopiv_gpu.cpp normal z -> c, Sat Nov 15 19:54:09 2014

*/
#include "common_magma.h"

/**
    Purpose
    -------
    Solves a system of linear equations
       A * X = B
    where A is a general n-by-n matrix and X and B are n-by-nrhs matrices.
    The LU decomposition with no pivoting is
    used to factor A as
       A = L * U,
    where L is unit lower triangular, and U is
    upper triangular.  The factored form of A is then used to solve the
    system of equations A * X = B.

    Arguments
    ---------
    @param[in]
    n       INTEGER
            The order of the matrix A.  n >= 0.

    @param[in]
    nrhs    INTEGER
            The number of right hand sides, i.e., the number of columns
            of the matrix B.  nrhs >= 0.

    @param[in,out]
    dA       COMPLEX array, dimension (ldda,n).
            On entry, the n-by-n matrix to be factored.
            On exit, the factors L and U from the factorization
            A = L*U; the unit diagonal elements of L are not stored.

    @param[in]
    ldda     INTEGER
            The leading dimension of the array A.  ldda >= max(1,n).

    @param[in,out]
    dB       COMPLEX array, dimension (lddb,nrhs)
            On entry, the right hand side matrix B.
            On exit, the solution matrix X.

    @param[in]
    lddb     INTEGER
            The leading dimension of the array B.  ldb >= max(1,n).

    @param[out]
    info    INTEGER
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value

    @ingroup magma_cgesv_driver
    ********************************************************************/




extern "C" magma_int_t
magma_cgesv_nopiv_gpu( magma_int_t n, magma_int_t nrhs, 
                 magmaFloatComplex_ptr dA, magma_int_t ldda,
                 magmaFloatComplex_ptr dB, magma_int_t lddb, 
                 magma_int_t *info)
{
    magma_int_t ret;

    *info = 0;
    if (n < 0) {
        *info = -1;
    } else if (nrhs < 0) {
        *info = -2;
    } else if (ldda < max(1,n)) {
        *info = -4;
    } else if (lddb < max(1,n)) {
        *info = -6;
    }
    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return MAGMA_ERR_ILLEGAL_VALUE;
    }

    /* Quick return if possible */
    if (n == 0 || nrhs == 0) {
        return MAGMA_SUCCESS;
    }

    ret = magma_cgetrf_nopiv_gpu( n, n, dA, ldda, info);
    if ( (ret != MAGMA_SUCCESS) || (*info != 0) ) {
        return ret;
    }
        
    ret = magma_cgetrs_nopiv_gpu( MagmaNoTrans, n, nrhs, dA, ldda, dB, lddb, info );
    
    
    return ret;
}
