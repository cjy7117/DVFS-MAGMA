/*
    -- MAGMA (version 1.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2012

       @generated d Wed Nov 14 22:53:15 2012

*/
#include "common_magma.h"

extern "C" magma_int_t
magma_dgeqrf(magma_int_t m, magma_int_t n, 
             double *a,    magma_int_t lda, double *tau, 
             double *work, magma_int_t lwork,
             magma_int_t *info )
{
/*  -- MAGMA (version 1.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2012

    Purpose
    =======
    DGEQRF computes a QR factorization of a DOUBLE_PRECISION M-by-N matrix A:
    A = Q * R. This version does not require work space on the GPU
    passed as input. GPU memory is allocated in the routine.

    Arguments
    =========
    M       (input) INTEGER
            The number of rows of the matrix A.  M >= 0.

    N       (input) INTEGER
            The number of columns of the matrix A.  N >= 0.

    A       (input/output) DOUBLE_PRECISION array, dimension (LDA,N)
            On entry, the M-by-N matrix A.
            On exit, the elements on and above the diagonal of the array
            contain the min(M,N)-by-N upper trapezoidal matrix R (R is
            upper triangular if m >= n); the elements below the diagonal,
            with the array TAU, represent the orthogonal matrix Q as a
            product of min(m,n) elementary reflectors (see Further
            Details).

            Higher performance is achieved if A is in pinned memory, e.g.
            allocated using magma_malloc_pinned.

    LDA     (input) INTEGER
            The leading dimension of the array A.  LDA >= max(1,M).

    TAU     (output) DOUBLE_PRECISION array, dimension (min(M,N))
            The scalar factors of the elementary reflectors (see Further
            Details).

    WORK    (workspace/output) DOUBLE_PRECISION array, dimension (MAX(1,LWORK))
            On exit, if INFO = 0, WORK(1) returns the optimal LWORK.

            Higher performance is achieved if WORK is in pinned memory, e.g.
            allocated using magma_malloc_pinned.

    LWORK   (input) INTEGER
            The dimension of the array WORK.  LWORK >= N*NB,
            where NB can be obtained through magma_get_dgeqrf_nb(M).

            If LWORK = -1, then a workspace query is assumed; the routine
            only calculates the optimal size of the WORK array, returns
            this value as the first entry of the WORK array, and no error
            message related to LWORK is issued.

    INFO    (output) INTEGER
            = 0:  successful exit
            < 0:  if INFO = -i, the i-th argument had an illegal value
                  or another error occured, such as memory allocation failed.

    Further Details
    ===============
    The matrix Q is represented as a product of elementary reflectors

       Q = H(1) H(2) . . . H(k), where k = min(m,n).

    Each H(i) has the form

       H(i) = I - tau * v * v'

    where tau is a real scalar, and v is a real vector with
    v(1:i-1) = 0 and v(i) = 1; v(i+1:m) is stored on exit in A(i+1:m,i),
    and tau in TAU(i).
    =====================================================================    */

    #define  a_ref(a_1,a_2) ( a+(a_2)*(lda) + (a_1))
    #define da_ref(a_1,a_2) (da+(a_2)*ldda  + (a_1))

    double *da, *dwork;
    double c_one = MAGMA_D_ONE;

    magma_int_t i, k, lddwork, old_i, old_ib;
    magma_int_t ib, ldda;

    /* Function Body */
    *info = 0;
    magma_int_t nb = magma_get_dgeqrf_nb(min(m, n));

    magma_int_t lwkopt = n * nb;
    work[0] = MAGMA_D_MAKE( (double)lwkopt, 0 );
    int lquery = (lwork == -1);
    if (m < 0) {
        *info = -1;
    } else if (n < 0) {
        *info = -2;
    } else if (lda < max(1,m)) {
        *info = -4;
    } else if (lwork < max(1,n) && ! lquery) {
        *info = -7;
    }
    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }
    else if (lquery)
        return *info;

    k = min(m,n);
    if (k == 0) {
        work[0] = c_one;
        return *info;
    }

    lddwork = ((n+31)/32)*32;
    ldda    = ((m+31)/32)*32;

    magma_int_t num_gpus = magma_num_gpus();
    if( num_gpus > 1 ) {
        /* call multiple-GPU interface  */
        return magma_dgeqrf4(num_gpus, m, n, a, lda, tau, work, lwork, info);
    }

    if (MAGMA_SUCCESS != magma_dmalloc( &da, (n)*ldda + nb*lddwork )) {
        /* Switch to the "out-of-core" (out of GPU-memory) version */
        return magma_dgeqrf_ooc(m, n, a, lda, tau, work, lwork, info);
    }

    cudaStream_t stream[2];
    magma_queue_create( &stream[0] );
    magma_queue_create( &stream[1] );

    dwork = da + ldda*(n);

    if ( (nb > 1) && (nb < k) ) {
        /* Use blocked code initially */
        magma_dsetmatrix_async( (m), (n-nb),
                                a_ref(0,nb),  lda,
                                da_ref(0,nb), ldda, stream[0] );

        old_i = 0; old_ib = nb;
        for (i = 0; i < k-nb; i += nb) {
            ib = min(k-i, nb);
            if (i>0){
                magma_dgetmatrix_async( (m-i), ib,
                                        da_ref(i,i), ldda,
                                        a_ref(i,i),  lda, stream[1] );

                magma_dgetmatrix_async( i, ib,
                                        da_ref(0,i), ldda,
                                        a_ref(0,i),  lda, stream[0] );

                /* Apply H' to A(i:m,i+2*ib:n) from the left */
                magma_dlarfb_gpu( MagmaLeft, MagmaTrans, MagmaForward, MagmaColumnwise, 
                                  m-old_i, n-old_i-2*old_ib, old_ib,
                                  da_ref(old_i, old_i),          ldda, dwork,        lddwork,
                                  da_ref(old_i, old_i+2*old_ib), ldda, dwork+old_ib, lddwork);
            }

            magma_queue_sync( stream[1] );
            magma_int_t rows = m-i;
            lapackf77_dgeqrf(&rows, &ib, a_ref(i,i), &lda, tau+i, work, &lwork, info);
            /* Form the triangular factor of the block reflector
               H = H(i) H(i+1) . . . H(i+ib-1) */
            lapackf77_dlarft( MagmaForwardStr, MagmaColumnwiseStr, 
                              &rows, &ib, a_ref(i,i), &lda, tau+i, work, &ib);
            dpanel_to_q(MagmaUpper, ib, a_ref(i,i), lda, work+ib*ib);
            magma_dsetmatrix( rows, ib, a_ref(i,i), lda, da_ref(i,i), ldda );
            dq_to_panel(MagmaUpper, ib, a_ref(i,i), lda, work+ib*ib);

            if (i + ib < n) {
                magma_dsetmatrix( ib, ib, work, ib, dwork, lddwork );

                if (i+ib < k-nb)
                    /* Apply H' to A(i:m,i+ib:i+2*ib) from the left */
                    magma_dlarfb_gpu( MagmaLeft, MagmaTrans, MagmaForward, MagmaColumnwise, 
                                      rows, ib, ib, 
                                      da_ref(i, i   ), ldda, dwork,    lddwork, 
                                      da_ref(i, i+ib), ldda, dwork+ib, lddwork);
                else
                    magma_dlarfb_gpu( MagmaLeft, MagmaTrans, MagmaForward, MagmaColumnwise, 
                                      rows, n-i-ib, ib, 
                                      da_ref(i, i   ), ldda, dwork,    lddwork, 
                                      da_ref(i, i+ib), ldda, dwork+ib, lddwork);

                old_i  = i;
                old_ib = ib;
            }
        }
    } else {
        i = 0;
    }
    
    /* Use unblocked code to factor the last or only block. */
    if (i < k) {
        ib = n-i;
        if (i!=0)
            magma_dgetmatrix( m, ib, da_ref(0,i), ldda, a_ref(0,i), lda );
        magma_int_t rows = m-i;
        lapackf77_dgeqrf(&rows, &ib, a_ref(i,i), &lda, tau+i, work, &lwork, info);
    }

    magma_queue_destroy( stream[0] );
    magma_queue_destroy( stream[1] );
    magma_free( da );
    return *info;
} /* magma_dgeqrf */

