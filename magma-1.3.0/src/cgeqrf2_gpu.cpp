/*
    -- MAGMA (version 1.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2012

       @generated c Wed Nov 14 22:53:10 2012

*/
#include "common_magma.h"

extern "C" magma_int_t
magma_cgeqrf2_gpu( magma_int_t m, magma_int_t n,
                   cuFloatComplex *dA, magma_int_t ldda,
                   cuFloatComplex *tau, 
                   magma_int_t *info )
{
/*  -- MAGMA (version 1.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2012

    Purpose
    =======

    CGEQRF computes a QR factorization of a complex M-by-N matrix A:
    A = Q * R.

    Arguments
    =========
    M       (input) INTEGER
            The number of rows of the matrix A.  M >= 0.

    N       (input) INTEGER
            The number of columns of the matrix A.  N >= 0.

    dA      (input/output) COMPLEX array on the GPU, dimension (LDDA,N)
            On entry, the M-by-N matrix dA.
            On exit, the elements on and above the diagonal of the array
            contain the min(M,N)-by-N upper trapezoidal matrix R (R is
            upper triangular if m >= n); the elements below the diagonal,
            with the array TAU, represent the orthogonal matrix Q as a
            product of min(m,n) elementary reflectors (see Further
            Details).

    LDDA    (input) INTEGER
            The leading dimension of the array dA.  LDDA >= max(1,M).
            To benefit from coalescent memory accesses LDDA must be
            dividable by 16.

    TAU     (output) COMPLEX array, dimension (min(M,N))
            The scalar factors of the elementary reflectors (see Further
            Details).

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

    where tau is a complex scalar, and v is a complex vector with
    v(1:i-1) = 0 and v(i) = 1; v(i+1:m) is stored on exit in A(i+1:m,i),
    and tau in TAU(i).
    =====================================================================    */

    #define dA(a_1,a_2)    ( dA+(a_2)*(ldda) + (a_1))
    #define work_ref(a_1)  ( work + (a_1))
    #define hwork          ( work + (nb)*(m))

    cuFloatComplex *dwork;
    cuFloatComplex *work;
    magma_int_t i, k, ldwork, lddwork, old_i, old_ib, rows;
    magma_int_t nbmin, nx, ib, nb;
    magma_int_t lhwork, lwork;

    /* Function Body */
    *info = 0;
    if (m < 0) {
        *info = -1;
    } else if (n < 0) {
        *info = -2;
    } else if (ldda < max(1,m)) {
        *info = -4;
    }
    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }

    k = min(m,n);
    if (k == 0)
        return *info;

    nb = magma_get_cgeqrf_nb(m);

    lwork  = (m+n) * nb;
    lhwork = lwork - (m)*nb;

    if (MAGMA_SUCCESS != magma_cmalloc( &dwork, (n)*nb )) {
        *info = MAGMA_ERR_DEVICE_ALLOC;
        return *info;
    }

    if (MAGMA_SUCCESS != magma_cmalloc_pinned( &work, lwork )) {
        magma_free( dwork );
        *info = MAGMA_ERR_HOST_ALLOC;
        return *info;
    }

    cudaStream_t stream[2];
    magma_queue_create( &stream[0] );
    magma_queue_create( &stream[1] );

    nbmin = 2;
    nx    = nb;
    ldwork = m;
    lddwork= n;

    if (nb >= nbmin && nb < k && nx < k) {
        /* Use blocked code initially */
        old_i = 0; old_ib = nb;
        for (i = 0; i < k-nx; i += nb) {
            ib = min(k-i, nb);
            rows = m -i;
            magma_cgetmatrix_async( rows, ib,
                                    dA(i,i),     ldda,
                                    work_ref(i), ldwork, stream[1] );
            if (i>0){
                /* Apply H' to A(i:m,i+2*ib:n) from the left */
                magma_clarfb_gpu( MagmaLeft, MagmaConjTrans, MagmaForward, MagmaColumnwise,
                                  m-old_i, n-old_i-2*old_ib, old_ib,
                                  dA(old_i, old_i         ), ldda, dwork,        lddwork,
                                  dA(old_i, old_i+2*old_ib), ldda, dwork+old_ib, lddwork);
                
                magma_csetmatrix_async( old_ib, old_ib,
                                        work_ref(old_i),  ldwork,
                                        dA(old_i, old_i), ldda, stream[0] );
            }

            magma_queue_sync( stream[1] );
            lapackf77_cgeqrf(&rows, &ib, work_ref(i), &ldwork, tau+i, hwork, &lhwork, info);
            /* Form the triangular factor of the block reflector
               H = H(i) H(i+1) . . . H(i+ib-1) */
            lapackf77_clarft( MagmaForwardStr, MagmaColumnwiseStr, 
                              &rows, &ib, 
                              work_ref(i), &ldwork, tau+i, hwork, &ib);

            cpanel_to_q( MagmaUpper, ib, work_ref(i), ldwork, hwork+ib*ib );
            magma_csetmatrix( rows, ib, work_ref(i), ldwork, dA(i,i), ldda );
            cq_to_panel( MagmaUpper, ib, work_ref(i), ldwork, hwork+ib*ib );

            if (i + ib < n) {
                magma_csetmatrix( ib, ib, hwork, ib, dwork, lddwork );

                if (i+nb < k-nx)
                    /* Apply H' to A(i:m,i+ib:i+2*ib) from the left */
                    magma_clarfb_gpu( MagmaLeft, MagmaConjTrans, MagmaForward, MagmaColumnwise,
                                      rows, ib, ib, 
                                      dA(i, i   ), ldda, dwork,    lddwork, 
                                      dA(i, i+ib), ldda, dwork+ib, lddwork);
                else {
                    magma_clarfb_gpu( MagmaLeft, MagmaConjTrans, MagmaForward, MagmaColumnwise,
                                      rows, n-i-ib, ib, 
                                      dA(i, i   ), ldda, dwork,    lddwork, 
                                      dA(i, i+ib), ldda, dwork+ib, lddwork);
                    magma_csetmatrix( ib, ib,
                                      work_ref(i), ldwork,
                                      dA(i,i),     ldda );
                }
                old_i  = i;
                old_ib = ib;
            }
        }
    } else {
        i = 0;
    }

    magma_free( dwork );

    /* Use unblocked code to factor the last or only block. */
    if (i < k) {
        ib   = n-i;
        rows = m-i;
        magma_cgetmatrix( rows, ib, dA(i, i), ldda, work, rows );
        lhwork = lwork - rows*ib;
        lapackf77_cgeqrf(&rows, &ib, work, &rows, tau+i, work+ib*rows, &lhwork, info);
        
        magma_csetmatrix( rows, ib, work, rows, dA(i, i), ldda );
    }

    magma_free_pinned( work );
    magma_queue_destroy( stream[0] );
    magma_queue_destroy( stream[1] );
    return *info;
} /* magma_cgeqrf2_gpu */
