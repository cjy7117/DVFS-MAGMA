/*
    -- MAGMA (version 1.4.1) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       December 2013

       @generated s Tue Dec 17 13:18:36 2013

*/
#include "common_magma.h"

extern "C" magma_int_t
magma_sorgtr(char uplo, magma_int_t n, float *a,
             magma_int_t lda, float *tau,
             float *work, magma_int_t lwork,
             float *dT, magma_int_t nb,
             magma_int_t *info)
{
/*  -- MAGMA (version 1.4.1) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       December 2013

    Purpose
    =======
    SORGTR generates a real unitary matrix Q which is defined as the
    product of n-1 elementary reflectors of order N, as returned by
    SSYTRD:

    if UPLO = 'U', Q = H(n-1) . . . H(2) H(1),

    if UPLO = 'L', Q = H(1) H(2) . . . H(n-1).

    Arguments
    =========
    UPLO    (input) CHARACTER*1
            = 'U': Upper triangle of A contains elementary reflectors
                   from SSYTRD;
            = 'L': Lower triangle of A contains elementary reflectors
                   from SSYTRD.

    N       (input) INTEGER
            The order of the matrix Q. N >= 0.

    A       (input/output) REAL array, dimension (LDA,N)
            On entry, the vectors which define the elementary reflectors,
            as returned by SSYTRD.
            On exit, the N-by-N unitary matrix Q.

    LDA     (input) INTEGER
            The leading dimension of the array A. LDA >= N.

    TAU     (input) REAL array, dimension (N-1)
            TAU(i) must contain the scalar factor of the elementary
            reflector H(i), as returned by SSYTRD.

    WORK    (workspace/output) REAL array, dimension (LWORK)
            On exit, if INFO = 0, WORK(1) returns the optimal LWORK.

    LWORK   (input) INTEGER
            The dimension of the array WORK. LWORK >= N-1.
            For optimum performance LWORK >= N*NB, where NB is
            the optimal blocksize.

            If LWORK = -1, then a workspace query is assumed; the routine
            only calculates the optimal size of the WORK array, returns
            this value as the first entry of the WORK array, and no error
            message related to LWORK is issued by XERBLA.

    DT      (input) REAL array on the GPU device.
            DT contains the T matrices used in blocking the elementary
            reflectors H(i) as returned by magma_ssytrd.

    NB      (input) INTEGER
            This is the block size used in SSYTRD, and correspondingly
            the size of the T matrices, used in the factorization, and
            stored in DT.

    INFO    (output) INTEGER
            = 0:  successful exit
            < 0:  if INFO = -i, the i-th argument had an illegal value
    =====================================================================    */

#define a_ref(i,j) ( a + (j)*lda+ (i))

    char uplo_[2]  = {uplo, 0};
    
    magma_int_t i__1;
    magma_int_t i, j;
    magma_int_t iinfo;
    magma_int_t upper, lwkopt, lquery;

    *info = 0;
    lquery = lwork == -1;
    upper = lapackf77_lsame(uplo_, "U");
    if (! upper && ! lapackf77_lsame(uplo_, "L")) {
        *info = -1;
    } else if (n < 0) {
        *info = -2;
    } else if (lda < max(1,n)) {
        *info = -4;
    } else /* if(complicated condition) */ {
        /* Computing MAX */
        if (lwork < max(1, n-1) && ! lquery) {
            *info = -7;
        }
    }

    lwkopt = max(1, n) * nb;
    if (*info == 0) {
        work[0] = MAGMA_S_MAKE( lwkopt, 0 );
    }

    if (*info != 0) {
        magma_xerbla( __func__, -(*info));
        return *info;
    } else if (lquery) {
        return *info;
    }

    /* Quick return if possible */
    if (n == 0) {
        work[0] = MAGMA_S_ONE;
        return *info;
    }

    if (upper) {
        /*  Q was determined by a call to SSYTRD with UPLO = 'U'
            Shift the vectors which define the elementary reflectors one
            column to the left, and set the last row and column of Q to
            those of the unit matrix                                    */
        for (j = 0; j < n-1; ++j) {
            for (i = 0; i < j-1; ++i)
                *a_ref(i, j) = *a_ref(i, j + 1);

            *a_ref(n-1, j) = MAGMA_S_ZERO;
        }
        for (i = 0; i < n-1; ++i) {
            *a_ref(i, n-1) = MAGMA_S_ZERO;
        }
        *a_ref(n-1, n-1) = MAGMA_S_ONE;
        
        /* Generate Q(1:n-1,1:n-1) */
        i__1 = n - 1;
        lapackf77_sorgql(&i__1, &i__1, &i__1, a_ref(0,0), &lda, tau, work,
                         &lwork, &iinfo);
    } else {
        
        /*  Q was determined by a call to SSYTRD with UPLO = 'L'.
            Shift the vectors which define the elementary reflectors one
            column to the right, and set the first row and column of Q to
            those of the unit matrix                                      */
        for (j = n-1; j > 0; --j) {
            *a_ref(0, j) = MAGMA_S_ZERO;
            for (i = j; i < n-1; ++i)
                *a_ref(i, j) = *a_ref(i, j - 1);
        }

        *a_ref(0, 0) = MAGMA_S_ONE;
        for (i = 1; i < n-1; ++i)
            *a_ref(i, 0) = MAGMA_S_ZERO;
        
        if (n > 1) {
            /* Generate Q(2:n,2:n) */
            magma_sorgqr(n-1, n-1, n-1, a_ref(1, 1), lda, tau, dT, nb, &iinfo);
        }
    }
    
    work[0] = MAGMA_S_MAKE( lwkopt, 0 );

    return *info;
} /* magma_sorgtr */

#undef a_ref
