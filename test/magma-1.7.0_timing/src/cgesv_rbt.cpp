/*
    -- MAGMA (version 1.7.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date September 2015

       @generated from zgesv_rbt.cpp normal z -> c, Fri Sep 11 18:29:27 2015

*/
#include "common_magma.h"

/**
    Purpose
    -------
    CGESV_RBT solves a system of linear equations
       A * X = B
    where A is a general N-by-N matrix and X and B are N-by-NRHS matrices.
    Random Butterfly Tranformation is applied on A and B, then
    the LU decomposition with no pivoting is
    used to factor A as
       A = L * U,
    where L is unit lower triangular, and U is
    upper triangular.  The factored form of A is then used to solve the
    system of equations A * X = B.
    The solution can then be improved using iterative refinement.

    Arguments
    ---------
    @param[in]
    ref     magma_bool_t
            Specifies if iterative refinement have to be applied to improve the solution.
      -     = MagmaTrue:   Iterative refinement is applied.
      -     = MagmaFalse:  Iterative refinement is not applied.

    @param[in]
    n       INTEGER
            The order of the matrix A.  N >= 0.

    @param[in]
    nrhs    INTEGER
            The number of right hand sides, i.e., the number of columns
            of the matrix B.  NRHS >= 0.

    @param[in,out]
    A       COMPLEX array, dimension (LDA,N).
            On entry, the M-by-N matrix to be factored.
            On exit, the factors L and U from the factorization
            A = P*L*U; the unit diagonal elements of L are not stored.

    @param[in]
    lda     INTEGER
            The leading dimension of the array A.  LDA >= max(1,N).

    @param[in,out]
    B       COMPLEX array, dimension (LDB,NRHS)
            On entry, the right hand side matrix B.
            On exit, the solution matrix X.
    
    @param[in]
    ldb     INTEGER
            The leading dimension of the array B.  LDB >= max(1,N).

    @param[out]
    info    INTEGER
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value

    @ingroup magma_cgesv_driver
 ********************************************************************/
extern "C" magma_int_t
magma_cgesv_rbt(
    magma_bool_t ref, magma_int_t n, magma_int_t nrhs,
    magmaFloatComplex *A, magma_int_t lda,
    magmaFloatComplex *B, magma_int_t ldb,
    magma_int_t *info)
{
    /* Function Body */
    *info = 0;
    if ( ! (ref == MagmaTrue) &&
         ! (ref == MagmaFalse) ) {
        *info = -1;
    }
    else if (n < 0) {
        *info = -2;
    } else if (nrhs < 0) {
        *info = -3;
    } else if (lda < max(1,n)) {
        *info = -5;
    } else if (ldb < max(1,n)) {
        *info = -7;
    }
    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }

    /* Quick return if possible */
    if (nrhs == 0 || n == 0)
        return *info;


    magma_int_t nn = n + ((4-(n % 4))%4);
    magmaFloatComplex *dA, *hu, *hv, *db, *dAo, *dBo, *dwork;
    magma_int_t n2;

    magma_int_t iter;
    n2 = nn*nn;

    if (MAGMA_SUCCESS != magma_cmalloc( &dA, n2 )) {
        *info = MAGMA_ERR_DEVICE_ALLOC;
        return *info;
    }
    if (MAGMA_SUCCESS != magma_cmalloc( &db, nn*nrhs )) {
        *info = MAGMA_ERR_DEVICE_ALLOC;
        return *info;
    }

    if (ref == MagmaTrue) {
        if (MAGMA_SUCCESS != magma_cmalloc( &dAo, n2 )) {
            *info = MAGMA_ERR_DEVICE_ALLOC;
            return *info;
        }
        if (MAGMA_SUCCESS != magma_cmalloc( &dwork, nn*nrhs )) {
            *info = MAGMA_ERR_DEVICE_ALLOC;
            return *info;
        }
        if (MAGMA_SUCCESS != magma_cmalloc( &dBo, nn*nrhs )) {
            *info = MAGMA_ERR_DEVICE_ALLOC;
            return *info;
        }
    }

    if (MAGMA_SUCCESS != magma_cmalloc_cpu( &hu, 2*nn )) {
        *info = MAGMA_ERR_HOST_ALLOC;
        return *info;
    }
    if (MAGMA_SUCCESS != magma_cmalloc_cpu( &hv, 2*nn )) {
        *info = MAGMA_ERR_HOST_ALLOC;
        return *info;
    }

    magmablas_claset(MagmaFull, nn, nn, MAGMA_C_ZERO, MAGMA_C_ONE, dA, nn);

    /* Send matrix to the GPU */
    magma_csetmatrix(n, n, A, lda, dA, nn);

    /* Send b to the GPU */
    magma_csetmatrix(n, nrhs, B, ldb, db, nn);

    *info = magma_cgerbt_gpu(MagmaTrue, nn, nrhs, dA, nn, db, nn, hu, hv, info);
    if (*info != MAGMA_SUCCESS)  {
        return *info;
    }

    if (ref == MagmaTrue) {
        magma_ccopymatrix(nn, nn, dA, nn, dAo, nn);
        magma_ccopymatrix(nn, nrhs, db, nn, dBo, nn);
    }
    /* Solve the system U^TAV.y = U^T.b on the GPU */
    magma_cgesv_nopiv_gpu( nn, nrhs, dA, nn, db, nn, info);


    /* Iterative refinement */
    if (ref == MagmaTrue) {
        magma_cgerfs_nopiv_gpu(MagmaNoTrans, nn, nrhs, dAo, nn, dBo, nn, db, nn, dwork, dA, &iter, info);
    }
    //printf("iter = %d\n", iter);

    /* The solution of A.x = b is Vy computed on the GPU */
    magmaFloatComplex *dv;

    if (MAGMA_SUCCESS != magma_cmalloc( &dv, 2*nn )) {
        *info = MAGMA_ERR_DEVICE_ALLOC;
        return *info;
    }

    magma_csetvector(2*nn, hv, 1, dv, 1);
    
    for (int i = 0; i < nrhs; i++) {
        magmablas_cprbt_mv(nn, dv, db+(i*nn));
    }

    magma_cgetmatrix(n, nrhs, db, nn, B, ldb);

    magma_free_cpu( hu);
    magma_free_cpu( hv);

    magma_free( dA );
    magma_free( dv );
    magma_free( db );
    
    if (ref == MagmaTrue) {
        magma_free( dAo );
        magma_free( dBo );
        magma_free( dwork );
    }
    return *info;
}
