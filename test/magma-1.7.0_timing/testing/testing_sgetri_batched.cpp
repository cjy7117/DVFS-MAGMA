/*
   -- MAGMA (version 1.7.0) --
   Univ. of Tennessee, Knoxville
   Univ. of California, Berkeley
   Univ. of Colorado, Denver
   @date September 2015

   @author Azzam Haidar
   @author Mark Gates

   @generated from testing_zgetri_batched.cpp normal z -> s, Fri Sep 11 18:29:39 2015
 */
// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// includes, project
#include "flops.h"
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"

#if defined(_OPENMP)
#include <omp.h>
#include "magma_threadsetting.h"
#endif

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing sgetri_batched
*/
int main( int argc, char** argv)
{
    TESTING_INIT();

    // constants
    const float c_zero    = MAGMA_S_ZERO;
    const float c_one     = MAGMA_S_ONE;
    const float c_neg_one = MAGMA_S_NEG_ONE;
    
    real_Double_t   gflops, gpu_perf, gpu_time, cpu_perf, cpu_time;
    float *h_A, *h_Ainv, *h_R, *work;
    magmaFloat_ptr d_A, d_invA;
    magmaFloat_ptr *dA_array;
    magmaFloat_ptr *dinvA_array;
    magma_int_t **dipiv_array;
    magma_int_t *dinfo_array;
    magma_int_t *ipiv, *cpu_info;
    magma_int_t *d_ipiv, *d_info;
    magma_int_t N, n2, lda, ldda, info, info1, info2, lwork;
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    float tmp;
    float  error, rwork[1];
    magma_int_t columns;
    magma_int_t status = 0;
    
    magma_opts opts( MagmaOptsBatched );
    opts.parse_opts( argc, argv );
    
    magma_int_t batchCount = opts.batchcount;
    float tol = opts.tolerance * lapackf77_slamch("E");

    printf("%% batchCount   N    CPU GFlop/s (ms)    GPU GFlop/s (ms)   ||I - A*A^{-1}||_1 / (N*cond(A))\n");
    printf("%%===============================================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {    
        for( int iter = 0; iter < opts.niter; ++iter ) {
            N = opts.nsize[itest];
            lda    = N;
            n2     = lda*N * batchCount;
            ldda   = magma_roundup( N, opts.align );  // multiple of 32 by default
            // This is the correct flops but since this getri_batched is based on
            // 2 trsm = getrs and to know the real flops I am using the getrs one
            //gflops = (FLOPS_SGETRF( N, N ) + FLOPS_SGETRI( N ))/ 1e9 * batchCount;
            gflops = (FLOPS_SGETRF( N, N ) + FLOPS_SGETRS( N, N ))/ 1e9 * batchCount;

            // query for workspace size
            lwork = -1;
            lapackf77_sgetri( &N, NULL, &lda, NULL, &tmp, &lwork, &info );
            if (info != 0)
                printf("lapackf77_sgetri returned error %d: %s.\n",
                       (int) info, magma_strerror( info ));
            lwork = magma_int_t( MAGMA_S_REAL( tmp ));
            
            TESTING_MALLOC_CPU( cpu_info, magma_int_t,        batchCount );
            TESTING_MALLOC_CPU( ipiv,     magma_int_t,        N * batchCount );
            TESTING_MALLOC_CPU( work,     float, lwork*batchCount );
            TESTING_MALLOC_CPU( h_A,      float, n2     );
            TESTING_MALLOC_CPU( h_Ainv,   float, n2     );
            TESTING_MALLOC_CPU( h_R,      float, n2     );
            
            TESTING_MALLOC_DEV( d_A,      float, ldda*N * batchCount );
            TESTING_MALLOC_DEV( d_invA,   float, ldda*N * batchCount );
            TESTING_MALLOC_DEV( d_ipiv,   magma_int_t,        N * batchCount );
            TESTING_MALLOC_DEV( d_info,   magma_int_t,        batchCount );

            TESTING_MALLOC_DEV( dA_array,    float*, batchCount );
            TESTING_MALLOC_DEV( dinvA_array, float*, batchCount );
            TESTING_MALLOC_DEV( dinfo_array, magma_int_t,         batchCount );
            TESTING_MALLOC_DEV( dipiv_array, magma_int_t*,        batchCount );
            
            /* Initialize the matrix */
            lapackf77_slarnv( &ione, ISEED, &n2, h_A );
            columns = N * batchCount;
            lapackf77_slacpy( MagmaFullStr, &N, &columns, h_A, &lda, h_R,  &lda );
            lapackf77_slacpy( MagmaFullStr, &N, &columns, h_A, &lda, h_Ainv, &lda );
            magma_ssetmatrix( N, columns, h_R, lda, d_A, ldda );

            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            sset_pointer(dA_array, d_A, ldda, 0, 0, ldda * N, batchCount, opts.queue);
            sset_pointer(dinvA_array, d_invA, ldda, 0, 0, ldda * N, batchCount, opts.queue);
            set_ipointer(dipiv_array, d_ipiv, 1, 0, 0, N, batchCount, opts.queue);

            gpu_time = magma_sync_wtime( opts.queue );
            info1 = magma_sgetrf_batched( N, N, dA_array, ldda, dipiv_array, dinfo_array, batchCount, opts.queue);
            info2 = magma_sgetri_outofplace_batched( N, dA_array, ldda, dipiv_array, dinvA_array, ldda, dinfo_array, batchCount, opts.queue);
            gpu_time = magma_sync_wtime( opts.queue ) - gpu_time;
            gpu_perf = gflops / gpu_time;

            // check correctness of results throught "dinfo_magma" and correctness of argument throught "info"
            magma_getvector( batchCount, sizeof(magma_int_t), dinfo_array, 1, cpu_info, 1);
            for (magma_int_t i=0; i < batchCount; i++)
            {
                if (cpu_info[i] != 0 ) {
                    printf("magma_sgetrf_batched matrix %d returned error %d\n", (int) i, (int)cpu_info[i] );
                }
            }
            if (info1 != 0) printf("magma_sgetrf_batched returned argument error %d: %s.\n", (int) info1, magma_strerror( info1 ));
            if (info2 != 0) printf("magma_sgetri_batched returned argument error %d: %s.\n", (int) info2, magma_strerror( info2 ));
            
            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            if ( opts.lapack ) {
                cpu_time = magma_wtime();
                #if !defined (BATCHED_DISABLE_PARCPU) && defined(_OPENMP)
                magma_int_t nthreads = magma_get_lapack_numthreads();
                magma_set_lapack_numthreads(1);
                magma_set_omp_numthreads(nthreads);
                #pragma omp parallel for schedule(dynamic)
                #endif
                for (int i=0; i < batchCount; i++)
                {
                    magma_int_t locinfo;
                    lapackf77_sgetrf(&N, &N, h_Ainv + i*lda*N, &lda, ipiv + i*N, &locinfo);
                    if (locinfo != 0)
                        printf("lapackf77_sgetrf returned error %d: %s.\n",
                               (int) locinfo, magma_strerror( locinfo ));
                    lapackf77_sgetri(&N, h_Ainv + i*lda*N, &lda, ipiv + i*N, work + i*lwork, &lwork, &locinfo );
                    if (locinfo != 0)
                        printf("lapackf77_sgetri returned error %d: %s.\n",
                               (int) locinfo, magma_strerror( locinfo ));
                }
                #if !defined (BATCHED_DISABLE_PARCPU) && defined(_OPENMP)
                    magma_set_lapack_numthreads(nthreads);
                #endif
                cpu_time = magma_wtime() - cpu_time;
                cpu_perf = gflops / cpu_time;
                
                printf("%10d %5d   %7.2f (%7.2f)   %7.2f (%7.2f)",
                       (int) batchCount, (int) N, cpu_perf, cpu_time*1000., gpu_perf, gpu_time*1000. );
            }
            else {
                printf("%10d %5d     ---   (  ---  )   %7.2f (%7.2f)",
                       (int) batchCount, (int) N, gpu_perf, gpu_time*1000. );
            }
            
            /* =====================================================================
               Check the result
               =================================================================== */
            if ( opts.check ) {
                magma_igetvector( N*batchCount, d_ipiv, 1, ipiv, 1 );
                magma_sgetmatrix( N, N*batchCount, d_invA, ldda, h_Ainv, lda );
                error = 0;
                for (magma_int_t i=0; i < batchCount; i++)
                {
                    for (magma_int_t k=0; k < N; k++) {
                        if (ipiv[i*N+k] < 1 || ipiv[i*N+k] > N )
                        {
                            printf("error for matrix %d ipiv @ %d = %d\n", (int) i, (int) k, (int) ipiv[i*N+k]);
                            error = -1;
                        }
                    }
                    if (error == -1) {
                        break;
                    }
                    
                    // compute 1-norm condition number estimate, following LAPACK's zget03
                    float normA, normAinv, rcond, err;
                    normA    = lapackf77_slange( "1", &N, &N, h_A    + i*lda*N, &lda, rwork );
                    normAinv = lapackf77_slange( "1", &N, &N, h_Ainv + i*lda*N, &lda, rwork );
                    if ( normA <= 0 || normAinv <= 0 ) {
                        rcond = 0;
                        err = 1 / (tol/opts.tolerance);  // == 1/eps
                    }
                    else {
                        rcond = (1 / normA) / normAinv;
                        // R = I
                        // R -= A*A^{-1}
                        // err = ||I - A*A^{-1}|| / ( N ||A||*||A^{-1}|| ) = ||R|| * rcond / N, using 1-norm
                        lapackf77_slaset( "full", &N, &N, &c_zero, &c_one, h_R + i*lda*N, &lda );
                        blasf77_sgemm( "no", "no", &N, &N, &N, &c_neg_one,
                                       h_A    + i*lda*N, &lda,
                                       h_Ainv + i*lda*N, &lda, &c_one,
                                       h_R    + i*lda*N, &lda );
                        err = lapackf77_slange( "1", &N, &N, h_R + i*lda*N, &lda, rwork );
                        err = err * rcond / N;
                    }
                    if ( isnan(err) || isinf(err) ) {
                        error = err;
                        break;
                    }
                    error = max( err, error );
                }
                bool okay = (error < tol);
                status += ! okay;
                printf("   %8.2e   %s\n", error, (okay ? "ok" : "failed") );
            }
            else {
                printf("\n");
            }

            TESTING_FREE_CPU( cpu_info );
            TESTING_FREE_CPU( ipiv   );
            TESTING_FREE_CPU( work   );
            TESTING_FREE_CPU( h_A    );
            TESTING_FREE_CPU( h_Ainv );
            TESTING_FREE_CPU( h_R    );
            
            TESTING_FREE_DEV( d_A );
            TESTING_FREE_DEV( d_invA );
            TESTING_FREE_DEV( d_ipiv );
            TESTING_FREE_DEV( d_info );
            
            TESTING_FREE_DEV( dA_array );
            TESTING_FREE_DEV( dinvA_array );
            TESTING_FREE_DEV( dinfo_array );
            TESTING_FREE_DEV( dipiv_array );
            fflush( stdout );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }
    
    TESTING_FINALIZE();
    return status;
}
