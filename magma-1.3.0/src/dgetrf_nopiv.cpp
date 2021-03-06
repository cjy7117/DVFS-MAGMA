/*
    -- MAGMA (version 1.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2012

       @generated d Wed Nov 14 22:53:07 2012

*/
#include "common_magma.h"

// === Define what BLAS to use ============================================
#define PRECISION_d
#if (defined(PRECISION_s) || defined(PRECISION_d))
  #define magma_dgemm magmablas_dgemm
  #define magma_dtrsm magmablas_dtrsm
#endif
// === End defining what BLAS to use =======================================

extern "C" magma_int_t
magma_dgetf2_nopiv(magma_int_t *m, magma_int_t *n, double *a,
                   magma_int_t *lda, magma_int_t *info);


extern "C" magma_int_t
magma_dgetrf_nopiv(magma_int_t *m, magma_int_t *n, double *a, 
                   magma_int_t *lda, magma_int_t *info)
{
/*  -- MAGMA (version 1.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2012

    Purpose   
    =======   
    DGETRF_NOPIV computes an LU factorization of a general M-by-N
    matrix A without pivoting.

    The factorization has the form   
       A = L * U   
    where L is lower triangular with unit diagonal elements (lower 
    trapezoidal if m > n), and U is upper triangular (upper 
    trapezoidal if m < n).   

    This is the right-looking Level 3 BLAS version of the algorithm.   

    Arguments   
    =========   
    M       (input) INTEGER   
            The number of rows of the matrix A.  M >= 0.   

    N       (input) INTEGER   
            The number of columns of the matrix A.  N >= 0.   

    A       (input/output) COMPLEX*16 array, dimension (LDA,N)   
            On entry, the M-by-N matrix to be factored.   
            On exit, the factors L and U from the factorization   
            A = P*L*U; the unit diagonal elements of L are not stored.   

    LDA     (input) INTEGER   
            The leading dimension of the array A.  LDA >= max(1,M).   

    INFO    (output) INTEGER   
            = 0:  successful exit   
            < 0:  if INFO = -i, the i-th argument had an illegal value   
            > 0:  if INFO = i, U(i,i) is exactly zero. The factorization   
                  has been completed, but the factor U is exactly   
                  singular, and division by zero will occur if it is used   
                  to solve a system of equations.   
    =====================================================================   */
   
    double c_one = MAGMA_D_ONE;
    
    magma_int_t a_dim1, a_offset, min_mn, i__3, i__4;
    double z__1;
    magma_int_t j, jb, nb, iinfo;

    a_dim1 = *lda;
    a_offset = 1 + a_dim1;
    a -= a_offset;

    /* Function Body */
    *info = 0;
    if (*m < 0) {
        *info = -1;
    } else if (*n < 0) {
        *info = -2;
    } else if (*lda < max(1,*m)) {
        *info = -4;
    }
    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }

    /* Quick return if possible */
    if (*m == 0 || *n == 0) {
        return *info;
    }

    /* Determine the block size for this environment. */
    nb = 128;
    min_mn = min(*m,*n);
    if (nb <= 1 || nb >= min_mn) 
      {
        /* Use unblocked code. */
        magma_dgetf2_nopiv(m, n, &a[a_offset], lda, info);
      }
    else 
      {
        /* Use blocked code. */
        for (j = 1; j <= min_mn; j += nb)
          {
            /* Computing MIN */
            i__3 = min_mn - j + 1;
            jb = min(i__3,nb);
            
            /* Factor diagonal and subdiagonal blocks and test for exact   
               singularity. */
            i__3 = *m - j + 1;
            //magma_dgetf2_nopiv(&i__3, &jb, &a[j + j * a_dim1], lda, &iinfo);

            i__3 -= jb; 
            magma_dgetf2_nopiv(&jb, &jb, &a[j + j * a_dim1], lda, &iinfo);
            blasf77_dtrsm("R", "U", "N", "N", &i__3, &jb, &c_one, 
                          &a[j + j * a_dim1], lda,
                          &a[j + jb + j * a_dim1], lda);
            
            /* Adjust INFO */
            if (*info == 0 && iinfo > 0)
              *info = iinfo + j - 1;

            if (j + jb <= *n) 
              {
                /* Compute block row of U. */
                i__3 = *n - j - jb + 1;
                blasf77_dtrsm("Left", "Lower", "No transpose", "Unit", &jb, &i__3,
                       &c_one, &a[j + j * a_dim1], lda, &a[j + (j+jb)*a_dim1], lda);
                if (j + jb <= *m) 
                  {
                    /* Update trailing submatrix. */
                    i__3 = *m - j - jb + 1;
                    i__4 = *n - j - jb + 1;
                    z__1 = MAGMA_D_NEG_ONE;
                    blasf77_dgemm("No transpose", "No transpose", &i__3, &i__4, &jb, 
                           &z__1, &a[j + jb + j * a_dim1], lda, 
                           &a[j + (j + jb) * a_dim1], lda, &c_one, 
                           &a[j + jb + (j + jb) * a_dim1], lda);
                  }
              }
          }
      }
    
    return *info;
} /* magma_dgetrf_nopiv */
