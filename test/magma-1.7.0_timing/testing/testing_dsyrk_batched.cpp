/*
    -- MAGMA (version 1.7.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date September 2015

       @generated from testing_zherk_batched.cpp normal z -> d, Fri Sep 11 18:29:39 2015
       @author Chongxiao Cao
       @author Tingxing Dong
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

#include "magma_threadsetting.h"  // to work around MKL bug

#define PRECISION_d

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing dsyrk_batched
*/
int main( int argc, char** argv)
{
    TESTING_INIT();

    real_Double_t   gflops, magma_perf, magma_time, cpu_perf=0., cpu_time=0.;
    double          magma_error, Cnorm, work[1];
    magma_int_t N, K;
    magma_int_t Ak, An;
    magma_int_t sizeA, sizeC;
    magma_int_t lda, ldc, ldda, lddc;
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    magma_int_t NN;
    magma_int_t batchCount;
 
    double *h_A, *h_C, *h_Cmagma;
    double *d_A, *d_C;
    double c_neg_one = MAGMA_D_NEG_ONE;
    double alpha = 0.29;
    double beta  = -0.48;
    double **A_array = NULL;
    double **C_array = NULL;
    magma_int_t status = 0;

    magma_opts opts( MagmaOptsBatched );
    opts.parse_opts( argc, argv );
    opts.lapack |= opts.check;  // check (-c) implies lapack (-l)
    batchCount = opts.batchcount;

    double tol = opts.tolerance * lapackf77_dlamch("E");
   
    printf("%% If running lapack (option --lapack), MAGMA error is computed\n"
           "%% relative to CPU BLAS result.\n\n");
    printf("%% uplo = %s, transA = %s\n",
           lapack_uplo_const(opts.uplo), lapack_trans_const(opts.transA) );
    #if defined(PRECISION_c) || defined (PRECISION_z)
    if(opts.transA == MagmaTrans)
    {
        opts.transA = MagmaConjTrans; 
        printf("%% WARNING: transA = MagmaTrans changed to MagmaConjTrans\n");
    }
    #endif
    
    printf("%% BatchCount   N     K   MAGMA Gflop/s (ms)    CPU Gflop/s (ms)   MAGMA error\n");
    printf("%%============================================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            N = opts.nsize[itest];
            K = opts.ksize[itest];
            gflops = FLOPS_DSYRK( K, N ) / 1e9 * batchCount;

            if ( opts.transA == MagmaNoTrans ) {
                lda = An = N;
                Ak = K;
            } else {
                lda = An = K;
                Ak = N;
            }

            ldc = N;

            ldda = magma_roundup( lda, opts.align );  // multiple of 32 by default
            lddc = magma_roundup( ldc, opts.align );  // multiple of 32 by default
            
            NN = N * batchCount;

            sizeA = lda*Ak*batchCount;
            sizeC = ldc*N*batchCount;
            
            TESTING_MALLOC_CPU( h_A,  double, sizeA );
            TESTING_MALLOC_CPU( h_C,  double, sizeC );
            TESTING_MALLOC_CPU( h_Cmagma,  double, sizeC  );
            
            TESTING_MALLOC_DEV( d_A, double, ldda*Ak*batchCount );
            TESTING_MALLOC_DEV( d_C, double, lddc*N*batchCount );

            magma_malloc((void**)&A_array, batchCount * sizeof(*A_array));
            magma_malloc((void**)&C_array, batchCount * sizeof(*C_array));

            /* Initialize the matrices */
            lapackf77_dlarnv( &ione, ISEED, &sizeA, h_A );
            lapackf77_dlarnv( &ione, ISEED, &sizeC, h_C );
            for (int i=0; i < batchCount; i++)
            {
               magma_dmake_hpd( N, h_C + i * ldc * N, ldc ); // need modification
            }
            
            /* =====================================================================
               Performs operation using MAGMABLAS
               =================================================================== */
            magma_dsetmatrix( An, Ak*batchCount, h_A, lda, d_A, ldda );
            magma_dsetmatrix( N, N*batchCount, h_C, ldc, d_C, lddc );
            
            dset_pointer(A_array, d_A, lda, 0, 0, ldda*Ak, batchCount, opts.queue);
            dset_pointer(C_array, d_C, ldc, 0, 0, lddc*N,  batchCount, opts.queue);

            magma_time = magma_sync_wtime( opts.queue );
            magmablas_dsyrk_batched(opts.uplo, opts.transA, N, K,
                             alpha, A_array, ldda,
                             beta,  C_array, lddc, batchCount, opts.queue);
                             
            magma_time = magma_sync_wtime( opts.queue ) - magma_time;
            magma_perf = gflops / magma_time;
            
            magma_dgetmatrix( N, NN, d_C, lddc, h_Cmagma, ldc );
            
            /* =====================================================================
               Performs operation using CPU BLAS
               =================================================================== */
            if ( opts.lapack ) {
                cpu_time = magma_wtime();
                for (int i=0; i < batchCount; i++)
                {
                   blasf77_dsyrk(
                               lapack_uplo_const(opts.uplo), lapack_trans_const(opts.transA),
                               &N, &K,
                               &alpha, h_A + i*lda*Ak, &lda,
                               &beta,  h_C + i*ldc*N, &ldc );
                }
                cpu_time = magma_wtime() - cpu_time;
                cpu_perf = gflops / cpu_time;
            }
            
            /* =====================================================================
               Check the result
               =================================================================== */
            if ( opts.lapack ) {
                #ifdef MAGMA_WITH_MKL
                // MKL (11.1.2) has bug in multi-threaded dlansy; use single thread to work around
                int threads = magma_get_lapack_numthreads();
                magma_set_lapack_numthreads( 1 );
                #endif
                
                // compute relative error for magma, relative to lapack,
                // |C_magma - C_lapack| / |C_lapack|
                sizeC = ldc*N;
                magma_error = 0;
                for (int i=0; i < batchCount; i++)
                {
                    Cnorm = lapackf77_dlansy("fro", lapack_uplo_const(opts.uplo), &N, h_C+i*ldc*N, &ldc, work);
                    blasf77_daxpy( &sizeC, &c_neg_one, h_C+i*ldc*N, &ione, h_Cmagma+i*ldc*N, &ione );
                    double err = lapackf77_dlansy( "fro", lapack_uplo_const(opts.uplo), &N, h_Cmagma+i*ldc*N, &ldc, work ) / Cnorm;
                    if ( isnan(err) || isinf(err) ) {
                        magma_error = err;
                        break;
                    }
                    magma_error = max( err, magma_error );
                }

                #ifdef MAGMA_WITH_MKL
                // end single thread to work around MKL bug
                magma_set_lapack_numthreads( threads );
                #endif
                
                bool okay = (magma_error < tol);
                status += ! okay;
                printf("%10d %5d %5d    %7.2f (%7.2f)   %7.2f (%7.2f)   %8.2e   %s\n",
                       (int) batchCount, (int) N, (int) K,
                       magma_perf, 1000.*magma_time,
                       cpu_perf,   1000.*cpu_time,
                       magma_error, (okay ? "ok" : "failed"));
            }
            else {
                printf("%10d %5d %5d    %7.2f (%7.2f)     ---   (  ---  )     ---\n",
                       (int) batchCount, (int) N, (int) K,
                       magma_perf, 1000.*magma_time);
            }
            
            TESTING_FREE_CPU( h_A  );
            TESTING_FREE_CPU( h_C  );
            TESTING_FREE_CPU( h_Cmagma  );

            TESTING_FREE_DEV( d_A );
            TESTING_FREE_DEV( d_C );
            TESTING_FREE_DEV( A_array );
            TESTING_FREE_DEV( C_array );
            fflush( stdout);
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }

    TESTING_FINALIZE();
    return status;
}
