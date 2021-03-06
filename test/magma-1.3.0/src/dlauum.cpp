/*
    -- MAGMA (version 1.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2012

       @generated d Wed Nov 14 22:53:02 2012

*/
#include "common_magma.h"

// === Define what BLAS to use ============================================
#define PRECISION_d
#if (defined(PRECISION_s) || defined(PRECISION_d))
        #define magma_dgemm magmablas_dgemm
        #define magma_dtrsm magmablas_dtrsm
#endif

#if (GPUSHMEM >= 200)
        #if (defined(PRECISION_s))
                #undef  magma_sgemm
                #define magma_sgemm magmablas_sgemm_fermi80
        #endif
#endif
// === End defining what BLAS to use ======================================

#define A(i, j)  (a   +(j)*lda  + (i))
#define dA(i, j) (work+(j)*ldda + (i))

 
extern "C" magma_int_t
magma_dlauum(char uplo, magma_int_t n,
             double *a, magma_int_t lda, magma_int_t *info)
{
/*  -- MAGMA (version 1.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2012

        Purpose
        =======

        DLAUUM computes the product U * U' or L' * L, where the triangular
        factor U or L is stored in the upper or lower triangular part of
        the array A.

        If UPLO = 'U' or 'u' then the upper triangle of the result is stored,
        overwriting the factor U in A.
        If UPLO = 'L' or 'l' then the lower triangle of the result is stored,
        overwriting the factor L in A.
        This is the blocked form of the algorithm, calling Level 3 BLAS.

        Arguments
        =========

        UPLO    (input) CHARACTER*1
                        Specifies whether the triangular factor stored in the array A
                        is upper or lower triangular:
                        = 'U':  Upper triangular
                        = 'L':  Lower triangular

        N       (input) INTEGER
                        The order of the triangular factor U or L.  N >= 0.

        A       (input/output) COPLEX_16 array, dimension (LDA,N)
                        On entry, the triangular factor U or L.
                        On exit, if UPLO = 'U', the upper triangle of A is
                        overwritten with the upper triangle of the product U * U';
                        if UPLO = 'L', the lower triangle of A is overwritten with
                        the lower triangle of the product L' * L.

        LDA     (input) INTEGER
                        The leading dimension of the array A.  LDA >= max(1,N).

        INFO    (output) INTEGER
                        = 0: successful exit
                        < 0: if INFO = -k, the k-th argument had an illegal value

        ===================================================================== */


        /* Local variables */
        char uplo_[2] = {uplo, 0};
        magma_int_t     ldda, nb;
        magma_int_t i, ib;
        double    c_one = MAGMA_D_ONE;
        double             d_one = MAGMA_D_ONE;
        double    *work;
        int upper = lapackf77_lsame(uplo_, "U");

        *info = 0;
        if ((! upper) && (! lapackf77_lsame(uplo_, "L")))
                *info = -1;
        else if (n < 0)
                *info = -2;
        else if (lda < max(1,n))
                *info = -4;

        if (*info != 0) {
                magma_xerbla( __func__, -(*info) );
                return *info;
        }

        /* Quick return */
        if ( n == 0 )
                return *info;

        ldda = ((n+31)/32)*32;

        if (MAGMA_SUCCESS != magma_dmalloc( &work, (n)*ldda )) {
                *info = MAGMA_ERR_DEVICE_ALLOC;
                return *info;
        }

        cudaStream_t stream[2];
        magma_queue_create( &stream[0] );
        magma_queue_create( &stream[1] );

        nb = magma_get_dpotrf_nb(n);

        if (nb <= 1 || nb >= n)
                lapackf77_dlauum(uplo_, &n, a, &lda, info);
        else
        {
                if (upper)
                {
                        /* Compute the product U * U'. */
                        for (i=0; i<n; i=i+nb)
                        {
                                ib=min(nb,n-i);

                                //cublasSetMatrix(ib, (n-i), sizeof(double), A(i, i), lda, dA(i, i), ldda);
                                
                                magma_dsetmatrix_async( ib, ib,
                                                        A(i,i),   lda,
                                                        dA(i, i), ldda, stream[1] );

                                magma_dsetmatrix_async( ib, (n-i-ib),
                                                        A(i,i+ib),  lda,
                                                        dA(i,i+ib), ldda, stream[0] );

                                magma_queue_sync( stream[1] );

                                magma_dtrmm( MagmaRight, MagmaUpper,
                                             MagmaTrans, MagmaNonUnit, i, ib,
                                             c_one, dA(i,i), ldda, dA(0, i),ldda);


                                lapackf77_dlauum(MagmaUpperStr, &ib, A(i,i), &lda, info);

                                magma_dsetmatrix_async( ib, ib,
                                                        A(i, i),  lda,
                                                        dA(i, i), ldda, stream[0] );

                                if (i+ib < n)
                                {
                                        magma_dgemm( MagmaNoTrans, MagmaTrans,
                                                     i, ib, (n-i-ib), c_one, dA(0,i+ib),
                                                     ldda, dA(i, i+ib),ldda, c_one,
                                                     dA(0,i), ldda);

                                        magma_queue_sync( stream[0] );

                                        magma_dsyrk( MagmaUpper, MagmaNoTrans, ib,(n-i-ib),
                                                     d_one, dA(i, i+ib), ldda,
                                                     d_one, dA(i, i), ldda);
                                }
                                
                                magma_dgetmatrix( i+ib, ib,
                                                  dA(0, i), ldda,
                                                  A(0, i),  lda );
                        }
                }
                else
                {
                        /* Compute the product L' * L. */
                        for(i=0; i<n; i=i+nb)
                        {
                                ib=min(nb,n-i);
                                //cublasSetMatrix((n-i), ib, sizeof(double),
                                //                A(i, i), lda, dA(i, i), ldda);

                                magma_dsetmatrix_async( ib, ib,
                                                        A(i,i),   lda,
                                                        dA(i, i), ldda, stream[1] );

                                magma_dsetmatrix_async( (n-i-ib), ib,
                                                        A(i+ib, i),  lda,
                                                        dA(i+ib, i), ldda, stream[0] );

                                magma_queue_sync( stream[1] );

                                magma_dtrmm( MagmaLeft, MagmaLower,
                                             MagmaTrans, MagmaNonUnit, ib,
                                             i, c_one, dA(i,i), ldda,
                                             dA(i, 0),ldda);


                                lapackf77_dlauum(MagmaLowerStr, &ib, A(i,i), &lda, info);

                                //cublasSetMatrix(ib, ib, sizeof(double),
                                //                A(i, i), lda, dA(i, i), ldda);

                                magma_dsetmatrix_async( ib, ib,
                                                        A(i, i),  lda,
                                                        dA(i, i), ldda, stream[0] );

                                if (i+ib < n)
                                {
                                        magma_dgemm(MagmaTrans, MagmaNoTrans,
                                                        ib, i, (n-i-ib), c_one, dA( i+ib,i),
                                                        ldda, dA(i+ib, 0),ldda, c_one,
                                                        dA(i,0), ldda);

                                        magma_queue_sync( stream[0] );
                                        
                                        magma_dsyrk(MagmaLower, MagmaTrans, ib, (n-i-ib),
                                                        d_one, dA(i+ib, i), ldda,
                                                        d_one, dA(i, i), ldda);
                                }
                                magma_dgetmatrix( ib, i+ib,
                                                  dA(i, 0), ldda,
                                                  A(i, 0),  lda );
                        }
                }
        }
        magma_queue_destroy( stream[0] );
        magma_queue_destroy( stream[1] );

        magma_free( work );

        return *info;

}
