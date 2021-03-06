/*
    -- MAGMA (version 1.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2012

       @generated c Wed Nov 14 22:52:58 2012

*/
#include "common_magma.h"

// === Define what BLAS to use ============================================
#define PRECISION_c

#if (defined(PRECISION_s) || defined(PRECISION_d))
  #define magma_cgemm magmablas_cgemm
  #define magma_ctrsm magmablas_ctrsm
#endif

#if (GPUSHMEM >= 200) && defined(PRECISION_s)
  #undef  magma_sgemm
  #define magma_sgemm magmablas_sgemm_fermi80
#endif
// === End defining what BLAS to use ======================================

#define dA(i, j) (dA+(j)*ldda + (i))

extern "C" magma_int_t
magma_ctrtri_gpu(char uplo, char diag, magma_int_t n,
             cuFloatComplex *dA, magma_int_t ldda, magma_int_t *info)
{
/*  -- MAGMA (version 1.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2012

    Purpose
    =======

    CTRTRI computes the inverse of a real upper or lower triangular
    matrix dA.

    This is the Level 3 BLAS version of the algorithm.

    Arguments
    =========

    UPLO    (input) CHARACTER*1
            = 'U':  A is upper triangular;
            = 'L':  A is lower triangular.

    DIAG    (input) CHARACTER*1
            = 'N':  A is non-unit triangular;
            = 'U':  A is unit triangular.

    N       (input) INTEGER
            The order of the matrix A.  N >= 0.

    dA      (input/output) COMPLEX array ON THE GPU, dimension (LDDA,N)
            On entry, the triangular matrix A.  If UPLO = 'U', the
            leading N-by-N upper triangular part of the array dA contains
            the upper triangular matrix, and the strictly lower
            triangular part of A is not referenced.  If UPLO = 'L', the
            leading N-by-N lower triangular part of the array dA contains
            the lower triangular matrix, and the strictly upper
            triangular part of A is not referenced.  If DIAG = 'U', the
            diagonal elements of A are also not referenced and are
            assumed to be 1.
            On exit, the (triangular) inverse of the original matrix, in
            the same storage format.

    LDDA    (input) INTEGER
            The leading dimension of the array dA.  LDDA >= max(1,N).

    INFO    (output) INTEGER
            = 0: successful exit
            < 0: if INFO = -i, the i-th argument had an illegal value
            > 0: if INFO = i, dA(i,i) is exactly zero.  The triangular
                    matrix is singular and its inverse cannot be computed.
                 (Singularity check is currently disabled.)

    ===================================================================== */

    /* Local variables */
    char uplo_[2] = {uplo, 0};
    char diag_[2] = {diag, 0};
    magma_int_t     nb, nn, j, jb;
    cuFloatComplex c_zero     = MAGMA_C_ZERO;
    cuFloatComplex c_one      = MAGMA_C_ONE;
    cuFloatComplex c_neg_one  = MAGMA_C_NEG_ONE;
    cuFloatComplex *work;

    int upper  = lapackf77_lsame(uplo_, "U");
    int nounit = lapackf77_lsame(diag_, "N");

    *info = 0;

    if ((! upper) && (! lapackf77_lsame(uplo_, "L")))
        *info = -1;
    else if ((! nounit) && (! lapackf77_lsame(diag_, "U")))
        *info = -2;
    else if (n < 0)
        *info = -3;
    else if (ldda < max(1,n))
        *info = -5;

    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }

    /* Check for singularity if non-unit */
    /* cannot do here with matrix dA on GPU -- need kernel */
    /*
    if (nounit) {
        for ( j=0; j<n; ++j ) {
            if ( MAGMA_C_EQUAL( *dA(j,j), c_zero )) {
                *info = j+1;  // Fortran index
                return *info;
            }
        }
    }
    */

    /* Determine the block size for this environment */
    nb = magma_get_cpotrf_nb(n);
    
    if (MAGMA_SUCCESS != magma_cmalloc_pinned( &work, nb*nb )) {
        *info = MAGMA_ERR_HOST_ALLOC;
        return *info;
    }
    
    cudaStream_t stream[2];
    magma_queue_create( &stream[0] );
    magma_queue_create( &stream[1] );

    if (nb <= 1 || nb >= n) {
        magma_cgetmatrix( n, n, dA, ldda, work, n );
        lapackf77_ctrtri(uplo_, diag_, &n, work, &n, info);
        magma_csetmatrix( n, n, work, n, dA, ldda );
    }
    else {
        if (upper) {
            /* Compute inverse of upper triangular matrix */
            for (j=0; j<n; j =j+ nb) {
                jb = min(nb, (n-j));

                /* Compute rows 1:j-1 of current block column */
                magma_ctrmm( MagmaLeft, MagmaUpper,
                             MagmaNoTrans, MagmaNonUnit, j, jb,
                             c_one, dA(0,0), ldda, dA(0, j),ldda);

                magma_ctrsm( MagmaRight, MagmaUpper,
                             MagmaNoTrans, MagmaNonUnit, j, jb,
                             c_neg_one, dA(j,j), ldda, dA(0, j),ldda);

        
                //cublasGetMatrix(jb ,jb, sizeof(cuFloatComplex),
                //                dA(j, j), ldda, work, jb);

                magma_cgetmatrix_async( jb, jb,
                                        dA(j, j), ldda,
                                        work,     jb, stream[1] );

                magma_queue_sync( stream[1] );

                /* Compute inverse of current diagonal block */
                lapackf77_ctrtri(MagmaUpperStr, diag_, &jb, work, &jb, info);

                //cublasSetMatrix(jb, jb, sizeof(cuFloatComplex),
                //                work, jb, dA(j, j), ldda);

                magma_csetmatrix_async( jb, jb,
                                        work,     jb,
                                        dA(j, j), ldda, stream[0] );
            }
        }
        else {
            /* Compute inverse of lower triangular matrix */
            nn=((n-1)/nb)*nb+1;

            for(j=nn-1; j>=0; j=j-nb) {
                jb=min(nb,(n-j));

                if((j+jb) < n) {
                    /* Compute rows j+jb:n of current block column */
                    magma_ctrmm( MagmaLeft, MagmaLower,
                                 MagmaNoTrans, MagmaNonUnit, (n-j-jb), jb,
                                 c_one, dA(j+jb,j+jb), ldda, dA(j+jb, j), ldda);

                    magma_ctrsm( MagmaRight, MagmaLower,
                                 MagmaNoTrans, MagmaNonUnit, (n-j-jb), jb,
                                 c_neg_one, dA(j,j), ldda, dA(j+jb, j), ldda);
                }

                //cublasGetMatrix(jb, jb, sizeof(cuFloatComplex),
                //               dA(j, j), ldda, work, jb);
                
                magma_cgetmatrix_async( jb, jb,
                                        dA(j, j), ldda,
                                        work,     jb, stream[1] );

                magma_queue_sync( stream[1] );

                /* Compute inverse of current diagonal block */
                lapackf77_ctrtri(MagmaLowerStr, diag_, &jb, work, &jb, info);
        
                //cublasSetMatrix(jb, jb, sizeof(cuFloatComplex),
                //                work, jb, dA(j, j), ldda);

                magma_csetmatrix_async( jb, jb,
                                        work,     jb,
                                        dA(j, j), ldda, stream[0] );
            }
        }
    }

    magma_queue_destroy( stream[0] );
    magma_queue_destroy( stream[1] );
    magma_free_pinned( work );

    return *info;
}
