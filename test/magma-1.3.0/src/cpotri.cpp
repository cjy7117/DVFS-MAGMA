/*
    -- MAGMA (version 1.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2012

       @generated c Wed Nov 14 22:53:01 2012

*/
#include "common_magma.h"

// === Define what BLAS to use ============================================
#define PRECISION_c
#if (defined(PRECISION_s) || defined(PRECISION_d))
  #define magma_cgemm magmablas_cgemm
  #define magma_ctrsm magmablas_ctrsm
#endif

#if (GPUSHMEM >= 200)
#if (defined(PRECISION_s))
     #undef  magma_sgemm
     #define magma_sgemm magmablas_sgemm_fermi80
  #endif
#endif
// === End defining what BLAS to use ======================================

#define A(i, j)  (a   +(j)*lda  + (i))

extern "C" magma_int_t
magma_cpotri(char uplo, magma_int_t n,
              cuFloatComplex *a, magma_int_t lda, magma_int_t *info)
{
/*  -- MAGMA (version 1.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2012

    Purpose
    =======

        CPOTRI computes the inverse of a real symmetric positive definite
        matrix A using the Cholesky factorization A = U**T*U or A = L*L**T
        computed by CPOTRF.

    Arguments
    =========

        UPLO    (input) CHARACTER*1
                        = 'U':  Upper triangle of A is stored;
                        = 'L':  Lower triangle of A is stored.

        N       (input) INTEGER
                        The order of the matrix A.  N >= 0.

        A       (input/output) COMPLEX array, dimension (LDA,N)
                        On entry, the triangular factor U or L from the Cholesky
                        factorization A = U**T*U or A = L*L**T, as computed by
                        CPOTRF.
                        On exit, the upper or lower triangle of the (symmetric)
                        inverse of A, overwriting the input factor U or L.

        LDA     (input) INTEGER
                        The leading dimension of the array A.  LDA >= max(1,N).
        INFO    (output) INTEGER
                        = 0:  successful exit
                        < 0:  if INFO = -i, the i-th argument had an illegal value
                        > 0:  if INFO = i, the (i,i) element of the factor U or L is
                                  zero, and the inverse could not be computed.

  ===================================================================== */

    /* Local variables */
    char uplo_[2] = {uplo, 0};

    *info = 0;
    if ((! lapackf77_lsame(uplo_, "U")) && (! lapackf77_lsame(uplo_, "L")))
        *info = -1;
    else if (n < 0)
        *info = -2;
    else if (lda < max(1,n))
        *info = -4;

    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }

    /* Quick return if possible */
    if ( n == 0 )
        return *info;
    
    /* Invert the triangular Cholesky factor U or L */
    magma_ctrtri( uplo, MagmaNonUnit, n, a, lda, info );
    if ( *info == 0 ) {
        /* Form inv(U) * inv(U)**T or inv(L)**T * inv(L) */
        magma_clauum( uplo, n, a, lda, info );
    }
    
    return *info;
} /* magma_cpotri */
