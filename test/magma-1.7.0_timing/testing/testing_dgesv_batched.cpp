/*
   -- MAGMA (version 1.7.0) --
   Univ. of Tennessee, Knoxville
   Univ. of California, Berkeley
   Univ. of Colorado, Denver
   @date September 2015

   @author Mark gates
   @author Azzam Haidar
   @author Tingxing Dong

   @generated from testing_zgesv_batched.cpp normal z -> d, Fri Sep 11 18:29:39 2015
 */
// includes, system
#include <stdio.h>
#include <stdlib.h>
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
   -- Testing dgesv_batched
*/
int main(int argc, char **argv)
{
    TESTING_INIT();

    real_Double_t   gflops, cpu_perf, cpu_time, gpu_perf, gpu_time;
    double          error, Rnorm, Anorm, Xnorm, *work;
    double c_one     = MAGMA_D_ONE;
    double c_neg_one = MAGMA_D_NEG_ONE;
    double *h_A, *h_B, *h_X;
    magmaDouble_ptr d_A, d_B;
    magma_int_t *dipiv, *dinfo_array;
    magma_int_t *ipiv, *cpu_info;
    magma_int_t N, nrhs, lda, ldb, ldda, lddb, info, sizeA, sizeB;
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    magma_int_t status = 0;
    magma_int_t batchCount;

    double **dA_array = NULL;
    double **dB_array = NULL;
    magma_int_t     **dipiv_array = NULL;

    magma_opts opts( MagmaOptsBatched );
    opts.parse_opts( argc, argv );
    
    double tol = opts.tolerance * lapackf77_dlamch("E");

    nrhs = opts.nrhs;
    batchCount = opts.batchcount;

    printf("%% BatchCount   N  NRHS   CPU GFlop/s (sec)   GPU GFlop/s (sec)   ||B - AX|| / N*||A||*||X||\n");
    printf("%%============================================================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            N = opts.nsize[itest];
            lda    = N;
            ldb    = lda;
            ldda   = magma_roundup( N, opts.align );  // multiple of 32 by default
            lddb   = ldda;
            gflops = ( FLOPS_DGETRF( N, N ) + FLOPS_DGETRS( N, nrhs ) ) * batchCount / 1e9;
            
            sizeA = lda*N*batchCount;
            sizeB = ldb*nrhs*batchCount;

            TESTING_MALLOC_CPU( h_A, double, sizeA );
            TESTING_MALLOC_CPU( h_B, double, sizeB );
            TESTING_MALLOC_CPU( h_X, double, sizeB );
            TESTING_MALLOC_CPU( work, double,      N);
            TESTING_MALLOC_CPU( ipiv, magma_int_t, batchCount*N);
            TESTING_MALLOC_CPU( cpu_info, magma_int_t, batchCount);
            
            TESTING_MALLOC_DEV( d_A, double, ldda*N*batchCount    );
            TESTING_MALLOC_DEV( d_B, double, lddb*nrhs*batchCount );
            TESTING_MALLOC_DEV( dipiv, magma_int_t, N * batchCount );
            TESTING_MALLOC_DEV( dinfo_array, magma_int_t, batchCount );

            magma_malloc((void**)&dA_array, batchCount * sizeof(*dA_array));
            magma_malloc((void**)&dB_array, batchCount * sizeof(*dB_array));
            magma_malloc((void**)&dipiv_array, batchCount * sizeof(*dipiv_array));

            /* Initialize the matrices */
            lapackf77_dlarnv( &ione, ISEED, &sizeA, h_A );
            lapackf77_dlarnv( &ione, ISEED, &sizeB, h_B );
            
            magma_dsetmatrix( N, N*batchCount,    h_A, lda, d_A, ldda );
            magma_dsetmatrix( N, nrhs*batchCount, h_B, ldb, d_B, lddb );
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            dset_pointer(dA_array, d_A, ldda, 0, 0, ldda*N, batchCount, opts.queue);
            dset_pointer(dB_array, d_B, lddb, 0, 0, lddb*nrhs, batchCount, opts.queue);
            set_ipointer(dipiv_array, dipiv, 1, 0, 0, N, batchCount, opts.queue);

            gpu_time = magma_sync_wtime( opts.queue );
            //magma_dgesv_gpu( N, nrhs, d_A, ldda, ipiv, d_B, lddb, &info );
            info = magma_dgesv_batched(N, nrhs, dA_array, ldda, dipiv_array, dB_array, lddb, dinfo_array, batchCount, opts.queue);
            gpu_time = magma_sync_wtime( opts.queue ) - gpu_time;
            gpu_perf = gflops / gpu_time;
            // check correctness of results throught "dinfo_magma" and correctness of argument throught "info"
            magma_getvector( batchCount, sizeof(magma_int_t), dinfo_array, 1, cpu_info, 1);
            for (int i=0; i < batchCount; i++)
            {
                if (cpu_info[i] != 0 ) {
                    printf("magma_dgesv_batched matrix %d returned internal error %d\n", i, int(cpu_info[i]) );
                }
            }
            if (info != 0) {
                printf("magma_dgesv_batched returned argument error %d: %s.\n",
                        int(info), magma_strerror( info ));
            }
            
            //=====================================================================
            // Residual
            //=====================================================================
            magma_dgetmatrix( N, nrhs*batchCount, d_B, lddb, h_X, ldb );

            error = 0;
            for (magma_int_t s=0; s < batchCount; s++)
            {
                Anorm = lapackf77_dlange("I", &N, &N,    h_A + s * lda * N, &lda, work);
                Xnorm = lapackf77_dlange("I", &N, &nrhs, h_X + s * ldb * nrhs, &ldb, work);
            
                blasf77_dgemm( MagmaNoTransStr, MagmaNoTransStr, &N, &nrhs, &N,
                           &c_one,     h_A + s * lda * N, &lda,
                                       h_X + s * ldb * nrhs, &ldb,
                           &c_neg_one, h_B + s * ldb * nrhs, &ldb);
            
                Rnorm = lapackf77_dlange("I", &N, &nrhs, h_B + s * ldb * nrhs, &ldb, work);
                double err = Rnorm/(N*Anorm*Xnorm);
                
                if ( isnan(err) || isinf(err) ) {
                    error = err;
                    break;
                }
                error = max( err, error );
            }
            bool okay = (error < tol);
            status += ! okay;

            /* ====================================================================
               Performs operation using LAPACK
               =================================================================== */
            if ( opts.lapack ) {
                cpu_time = magma_wtime();
                // #define BATCHED_DISABLE_PARCPU
                #if !defined (BATCHED_DISABLE_PARCPU) && defined(_OPENMP)
                magma_int_t nthreads = magma_get_lapack_numthreads();
                magma_set_lapack_numthreads(1);
                magma_set_omp_numthreads(nthreads);
                #pragma omp parallel for schedule(dynamic)
                #endif
                for (magma_int_t s=0; s < batchCount; s++)
                {
                    magma_int_t locinfo;
                    lapackf77_dgesv( &N, &nrhs, h_A + s * lda * N, &lda, ipiv + s * N, h_B + s * ldb * nrhs, &ldb, &locinfo );
                    if (locinfo != 0) {
                        printf("lapackf77_dgesv matrix %d returned error %d: %s.\n",
                                int(s), int(locinfo), magma_strerror( locinfo ));
                    }
                }
                #if !defined (BATCHED_DISABLE_PARCPU) && defined(_OPENMP)
                    magma_set_lapack_numthreads(nthreads);
                #endif
                cpu_time = magma_wtime() - cpu_time;
                cpu_perf = gflops / cpu_time;
                printf( "%10d %5d %5d   %7.2f (%7.2f)   %7.2f (%7.2f)   %8.2e   %s\n",
                        int(batchCount), int(N), int(nrhs), cpu_perf, cpu_time, gpu_perf, gpu_time,
                        error, (okay ? "ok" : "failed"));
            }
            else {
                printf( "%10d %5d %5d     ---   (  ---  )   %7.2f (%7.2f)   %8.2e   %s\n",
                        int(batchCount), int(N), int(nrhs), gpu_perf, gpu_time,
                        error, (okay ? "ok" : "failed"));
            }
            
            TESTING_FREE_CPU( h_A );
            TESTING_FREE_CPU( h_B );
            TESTING_FREE_CPU( h_X );
            TESTING_FREE_CPU( work );
            TESTING_FREE_CPU( ipiv );
            TESTING_FREE_CPU( cpu_info );
            
            TESTING_FREE_DEV( d_A );
            TESTING_FREE_DEV( d_B );

            TESTING_FREE_DEV( dipiv );
            TESTING_FREE_DEV( dinfo_array );

            magma_free(dA_array);
            magma_free(dB_array);
            magma_free(dipiv_array);
            fflush( stdout );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }

    TESTING_FINALIZE();
    return status;
}
