/*
    -- MAGMA (version 1.6.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2014

       @generated from testing_zgeqrf_mgpu.cpp normal z -> s, Sat Nov 15 19:54:18 2014
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

#define PRECISION_s

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing sgeqrf_mgpu
*/
int main( int argc, char** argv )
{
    TESTING_INIT();

    real_Double_t    gflops, gpu_perf, gpu_time, cpu_perf=0, cpu_time=0;
    float           error, work[1];
    float c_neg_one = MAGMA_S_NEG_ONE;
    float *h_A, *h_R, *tau, *h_work, tmp[1];
    magmaFloat_ptr d_lA[ MagmaMaxGPUs ];
    magma_int_t M, N, n2, lda, ldda, n_local, ngpu;
    magma_int_t info, min_mn, nb, lhwork;
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1}, ISEED2[4];
    
    magma_opts opts;
    parse_opts( argc, argv, &opts );
    opts.lapack |= (opts.check == 2);  // check (-c2) implies lapack (-l)
 
    magma_int_t status = 0;
    float tol, eps = lapackf77_slamch("E");
    tol = opts.tolerance * eps;

    printf("ngpu %d\n", (int) opts.ngpu );
    if ( opts.check == 1 ) {
        printf("    M     N   CPU GFlop/s (sec)   GPU GFlop/s (sec)   ||R-Q'A||_1 / (M*||A||_1) ||I-Q'Q||_1 / M\n");
        printf("================================================================================================\n");

    } else {
        printf("    M     N   CPU GFlop/s (sec)   GPU GFlop/s (sec)   ||R||_F /(M*||A||_F)\n");
        printf("==========================================================================\n");
    }
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            M = opts.msize[itest];
            N = opts.nsize[itest];
            min_mn = min(M, N);
            lda    = M;
            n2     = lda*N;
            ldda   = ((M+31)/32)*32;
            nb     = magma_get_sgeqrf_nb( M );
            gflops = FLOPS_SGEQRF( M, N ) / 1e9;
            
            // ngpu must be at least the number of blocks
            ngpu = min( opts.ngpu, int((N+nb-1)/nb) );
            if ( ngpu < opts.ngpu ) {
                printf( " * too many GPUs for the matrix size, using %d GPUs\n", (int) ngpu );
            }
            
            // query for workspace size
            lhwork = -1;
            lapackf77_sgeqrf( &M, &N, NULL, &M, NULL, tmp, &lhwork, &info );
            lhwork = (magma_int_t) MAGMA_S_REAL( tmp[0] );
            
            // Allocate host memory for the matrix
            TESTING_MALLOC_CPU( tau,    float, min_mn );
            TESTING_MALLOC_CPU( h_A,    float, n2     );
            TESTING_MALLOC_CPU( h_work, float, lhwork );
            
            TESTING_MALLOC_PIN( h_R,    float, n2     );
            
            // Allocate device memory
            for( int dev = 0; dev < ngpu; dev++ ) {
                n_local = ((N/nb)/ngpu)*nb;
                if (dev < (N/nb) % ngpu)
                    n_local += nb;
                else if (dev == (N/nb) % ngpu)
                    n_local += N % nb;
                magma_setdevice( dev );
                TESTING_MALLOC_DEV( d_lA[dev], float, ldda*n_local );
            }
            
            /* Initialize the matrix */
            for ( int j=0; j<4; j++ )
                ISEED2[j] = ISEED[j]; // save seeds
            lapackf77_slarnv( &ione, ISEED, &n2, h_A );
            lapackf77_slacpy( MagmaUpperLowerStr, &M, &N, h_A, &lda, h_R, &lda );
            
            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            if ( opts.lapack ) {
                float *tau2;
                TESTING_MALLOC_CPU( tau2, float, min_mn );
                cpu_time = magma_wtime();
                lapackf77_sgeqrf( &M, &N, h_A, &M, tau2, h_work, &lhwork, &info );
                cpu_time = magma_wtime() - cpu_time;
                cpu_perf = gflops / cpu_time;
                if (info != 0)
                    printf("lapack_sgeqrf returned error %d: %s.\n",
                           (int) info, magma_strerror( info ));
                TESTING_FREE_CPU( tau2 );
            }
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            magma_ssetmatrix_1D_col_bcyclic( M, N, h_R, lda, d_lA, ldda, ngpu, nb );
            
            gpu_time = magma_wtime();
            magma_sgeqrf2_mgpu( ngpu, M, N, d_lA, ldda, tau, &info );
            gpu_time = magma_wtime() - gpu_time;
            gpu_perf = gflops / gpu_time;
            if (info != 0)
                printf("magma_sgeqrf2 returned error %d: %s.\n",
                       (int) info, magma_strerror( info ));
            
            magma_sgetmatrix_1D_col_bcyclic( M, N, d_lA, ldda, h_R, lda, ngpu, nb );
            magma_queue_sync( NULL );
            
            if ( opts.check == 1 && M >= N ) {
                /* =====================================================================
                   Check the result -- sqrt02 requires M >= N
                   =================================================================== */
                magma_int_t lwork = n2+N;
                float *h_W1, *h_W2, *h_W3;
                float *h_RW, results[2];
            
                TESTING_MALLOC_CPU( h_W1, float, n2    ); // Q
                TESTING_MALLOC_CPU( h_W2, float, n2    ); // R
                TESTING_MALLOC_CPU( h_W3, float, lwork ); // WORK
                TESTING_MALLOC_CPU( h_RW, float, M );  // RWORK
                lapackf77_slarnv( &ione, ISEED2, &n2, h_A );
                lapackf77_sqrt02( &M, &N, &min_mn, h_A, h_R, h_W1, h_W2, &lda, tau, h_W3, &lwork,
                                  h_RW, results );
                results[0] *= eps;
                results[1] *= eps;

                if ( opts.lapack ) {
                    printf("%5d %5d   %7.2f (%7.2f)   %7.2f (%7.2f)   %8.2e                 %8.2e",
                           (int) M, (int) N, cpu_perf, cpu_time, gpu_perf, gpu_time, results[0], results[1] );
                } else {
                    printf("%5d %5d     ---   (  ---  )   %7.2f (%7.2f)    %8.2e                 %8.2e",
                           (int) M, (int) N, gpu_perf, gpu_time, results[0], results[1] );
                }
                // todo also check results[1] < tol?
                printf("   %s\n", (results[0] < tol ? "ok" : "failed"));
                status += ! (results[0] < tol);

                TESTING_FREE_CPU( h_W1 );
                TESTING_FREE_CPU( h_W2 );
                TESTING_FREE_CPU( h_W3 );
                TESTING_FREE_CPU( h_RW );
            }
            else if ( opts.check == 2 ) {
                /* =====================================================================
                   Check the result compared to LAPACK
                   =================================================================== */
                error = lapackf77_slange("f", &M, &N, h_A, &lda, work );
                blasf77_saxpy( &n2, &c_neg_one, h_A, &ione, h_R, &ione );
                error = lapackf77_slange("f", &M, &N, h_R, &lda, work ) / (min_mn*error);
                
                printf("%5d %5d   %7.2f (%7.2f)   %7.2f (%7.2f)   %8.2e   %s\n",
                       (int) M, (int) N, cpu_perf, cpu_time, gpu_perf, gpu_time,
                       error, (error < tol ? "ok" : "failed"));
                status += ! (error < tol);
            }
            else {
                if ( opts.lapack ) {
                    printf("%5d %5d   %7.2f (%7.2f)   %7.2f (%7.2f)   ---",
                           (int) M, (int) N, cpu_perf, cpu_time, gpu_perf, gpu_time );
                } else {
                    printf("%5d %5d     ---   (  ---  )   %7.2f (%7.2f)     ---",
                           (int) M, (int) N, gpu_perf, gpu_time);
                }
                printf("%s\n", (opts.check != 0 ? "  (error check only for M >= N)" : ""));
            }
            
            TESTING_FREE_CPU( tau    );
            TESTING_FREE_CPU( h_A    );
            TESTING_FREE_CPU( h_work );
            
            TESTING_FREE_PIN( h_R    );
            
            for( int dev=0; dev < ngpu; dev++ ){
                magma_setdevice( dev );
                TESTING_FREE_DEV( d_lA[dev] );
            }
            fflush( stdout );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }
    
    TESTING_FINALIZE();
    return status;
}