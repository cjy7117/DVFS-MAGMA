/*
    -- MAGMA (version 1.6.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2014

       @generated from testing_zherk_batched.cpp normal z -> c, Sat Nov 15 19:54:18 2014
       @author Chongxiao Cao
       @author Tingxing Dong
*/


// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <cuda_runtime_api.h>
#include <cublas_v2.h>

// includes, project
#include "flops.h"
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"


/* ////////////////////////////////////////////////////////////////////////////
   -- Testing cherk_batched
*/
int main( int argc, char** argv)
{
    TESTING_INIT();

    real_Double_t   gflops, magma_perf, magma_time, cpu_perf=0., cpu_time=0.;
    float          current_error, magma_error, Cnorm, work[1];
    magma_int_t N, K;
    magma_int_t Ak, An;
    magma_int_t sizeA, sizeC;
    magma_int_t lda, ldc, ldda, lddc;
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    magma_int_t NN;
    magma_int_t batchCount;
 
    magmaFloatComplex *h_A, *h_C, *h_Cmagma;
    magmaFloatComplex *d_A, *d_C;
    magmaFloatComplex c_neg_one = MAGMA_C_NEG_ONE;
    float alpha = 0.29;
    float beta  = -0.48;
    magmaFloatComplex **A_array = NULL;
    magmaFloatComplex **C_array = NULL;
    magma_int_t status = 0;

    magma_opts opts;
    parse_opts( argc, argv, &opts );
    opts.lapack |= opts.check;  // check (-c) implies lapack (-l)
    batchCount = opts.batchcount;

    float tol = opts.tolerance * lapackf77_slamch("E");
    
    printf("If running lapack (option --lapack), MAGMA error is computed\n"
           "relative to CPU BLAS result.\n\n");
    printf("uplo = %s, transA = %s\n",
           lapack_uplo_const(opts.uplo), lapack_trans_const(opts.transA) );
    printf(" BatchCount    N     K   MAGMA Gflop/s (ms)  CPU Gflop/s (ms)  MAGMA error \n");
    printf("=========================================================================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            N = opts.nsize[itest];
            K = opts.ksize[itest];
            gflops = FLOPS_CHERK( K, N ) / 1e9 * batchCount;

            if ( opts.transA == MagmaNoTrans ) {
                lda = An = N;
                Ak = K;
            } else {
                lda = An = K;
                Ak = N;
            }

            ldc = N;

            ldda = ((lda+31)/32)*32;
            lddc = ((ldc+31)/32)*32;
            
            NN = N * batchCount;

            
            sizeA = lda*Ak*batchCount;
            sizeC = ldc*N*batchCount;
            
            TESTING_MALLOC_CPU( h_A,  magmaFloatComplex, sizeA );
            TESTING_MALLOC_CPU( h_C,  magmaFloatComplex, sizeC );
            TESTING_MALLOC_CPU( h_Cmagma,  magmaFloatComplex, sizeC  );
            
            TESTING_MALLOC_DEV( d_A, magmaFloatComplex, sizeA );
            TESTING_MALLOC_DEV( d_C, magmaFloatComplex, sizeC );

            magma_malloc((void**)&A_array, batchCount * sizeof(*A_array));
            magma_malloc((void**)&C_array, batchCount * sizeof(*C_array));

            /* Initialize the matrices */
            lapackf77_clarnv( &ione, ISEED, &sizeA, h_A );
            lapackf77_clarnv( &ione, ISEED, &sizeC, h_C );
            
            /* =====================================================================
               Performs operation using MAGMABLAS
               =================================================================== */
            magma_csetmatrix( An, Ak*batchCount, h_A, lda, d_A, ldda );
            magma_csetmatrix( N, N*batchCount, h_C, ldc, d_C, lddc );
            
            cset_pointer(A_array, d_A, lda, 0, 0, ldda*Ak, batchCount);
            cset_pointer(C_array, d_C, ldc, 0, 0, lddc*N,  batchCount);

            magma_time = magma_sync_wtime( NULL );
            magmablas_cherk_batched(opts.uplo, opts.transA, N, K,
                             alpha, A_array, ldda,
                             beta,  C_array, lddc, batchCount);
                             
            magma_time = magma_sync_wtime( NULL ) - magma_time;
            magma_perf = gflops / magma_time;
            
            magma_cgetmatrix( N, NN, d_C, lddc, h_Cmagma, ldc );
            
            
            /* =====================================================================
               Performs operation using CPU BLAS
               =================================================================== */
            if ( opts.lapack ) {
                cpu_time = magma_wtime();
                for(int i=0; i<batchCount; i++)
                {
                   blasf77_cherk(
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
            if ( opts.check ) {
                // compute relative error for magma, relative to lapack,
                // |C_magma - C_lapack| / |C_lapack|
                sizeC = ldc*N;
                magma_error = MAGMA_D_ZERO;
                for(int i=0; i<batchCount; i++)
                {
                    Cnorm = lapackf77_clanhe("fro", lapack_uplo_const(opts.uplo), &N, h_C+i*ldc*N, &ldc, work);
                    blasf77_caxpy( &sizeC, &c_neg_one, h_C+i*ldc*N, &ione, h_Cmagma+i*ldc*N, &ione );
                    current_error = lapackf77_clanhe( "fro", lapack_uplo_const(opts.uplo), &N, h_Cmagma+i*ldc*N, &ldc, work ) / Cnorm;
                    if ( isnan(current_error) || isinf(current_error) ) {
                        magma_error = current_error;
                        break;
                    }
                    magma_error = max(magma_error, current_error);
                }

                printf("%5d %5d %5d  %7.2f (%7.2f)   %7.2f (%7.2f)    %8.2e   %s\n",
                       (int) batchCount, (int) N, (int) K,
                       magma_perf, 1000.*magma_time,
                       cpu_perf,    1000.*cpu_time,
                       magma_error, (magma_error < tol ? "ok" : "failed"));

                status += ! (magma_error < tol);

            }
            else {
                printf("%5d %5d %5d   %7.2f (%7.2f)    ---   (  ---  )    ---     ---\n",
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
