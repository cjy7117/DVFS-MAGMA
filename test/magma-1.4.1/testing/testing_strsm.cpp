/*
    -- MAGMA (version 1.4.1) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       December 2013

       @generated s Tue Dec 17 13:18:56 2013
       @author Chongxiao Cao
*/
// make sure that asserts are enabled
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <cuda_runtime_api.h>
#include <cublas.h>

// includes, project
#include "flops.h"
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"

#define PRECISION_s
#if (defined(PRECISION_z) || defined(PRECISION_c))
#define magmablas_strsm cublasStrsm
#endif
/* ////////////////////////////////////////////////////////////////////////////
   -- Testing strsm
*/
int main( int argc, char** argv)
{
    TESTING_INIT();

    real_Double_t   gflops, magma_perf, magma_time, cublas_perf, cublas_time, cpu_perf=0, cpu_time=0;
    float          magma_error, cublas_error, work[1];
    magma_int_t M, N, info;
    magma_int_t Ak;
    magma_int_t sizeA, sizeB;
    magma_int_t lda, ldb, ldda, lddb;
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
   
    magma_int_t *piv;
    magma_err_t err;

    float *h_A, *h_B, *h_Bcublas, *h_Bmagma, *h_B1, *h_X1, *h_X2, *LU, *LUT;
    float *d_A, *d_B;
    float c_neg_one = MAGMA_S_NEG_ONE;
    float c_one = MAGMA_S_ONE;
    float alpha = MAGMA_S_MAKE(  0.29, -0.86 );
    
    magma_opts opts;
    parse_opts( argc, argv, &opts );
    
    printf("If running lapack (option --lapack), MAGMA and CUBLAS error are both computed\n"
           "relative to CPU BLAS result. Else, MAGMA error is computed relative to CUBLAS result.\n\n"
           "side = %c, uplo = %c, transA = %c, diag = %c \n", opts.side, opts.uplo, opts.transA, opts.diag );
    printf("    M     N  MAGMA Gflop/s (ms)  CUBLAS Gflop/s (ms)   CPU Gflop/s (ms)  MAGMA error  CUBLAS error\n");
    printf("==================================================================================================\n");
    for( int i = 0; i < opts.ntest; ++i ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            M = opts.msize[i];
            N = opts.nsize[i];
            gflops = FLOPS_STRSM(opts.side, M, N) / 1e9;

            if ( opts.side == MagmaLeft ) {
                lda = M;
                Ak = M;
            } else {
                lda = N;
                Ak = N;
            }
            
            ldb = M;
            
            ldda = ((lda+31)/32)*32;
            lddb = ((ldb+31)/32)*32;
            
            sizeA = lda*Ak;
            sizeB = ldb*N;
            
            TESTING_MALLOC_CPU( h_A,       float, lda*Ak  );
            TESTING_MALLOC_CPU( LU,        float, lda*Ak  );
            TESTING_MALLOC_CPU( LUT,       float, lda*Ak  );
            TESTING_MALLOC_CPU( h_B,       float, ldb*N   );
            TESTING_MALLOC_CPU( h_B1,      float, ldb*N   );
            TESTING_MALLOC_CPU( h_X1,      float, ldb*N   );
            TESTING_MALLOC_CPU( h_X2,      float, ldb*N   );
            TESTING_MALLOC_CPU( h_Bcublas, float, ldb*N   );
            TESTING_MALLOC_CPU( h_Bmagma,  float, ldb*N   );
            
            TESTING_MALLOC_DEV( d_A,       float, ldda*Ak );
            TESTING_MALLOC_DEV( d_B,       float, lddb*N  );
            
            /* Initialize the matrices */
            lapackf77_slarnv( &ione, ISEED, &sizeA, LU );
            err = magma_malloc_cpu( (void**) &piv, Ak*sizeof(magma_int_t) );  assert( err == 0 );
            lapackf77_sgetrf( &Ak, &Ak, LU, &lda, piv, &info );
        
            int i, j;
            for(i=0;i<Ak;i++){
                for(j=0;j<Ak;j++){
                    LUT[j+i*lda] = LU[i+j*lda];
                }
            }

            lapackf77_slacpy(MagmaUpperStr, &Ak, &Ak, LUT, &lda, LU, &lda);

            if(opts.uplo == MagmaLower){
                lapackf77_slacpy(MagmaLowerStr, &Ak, &Ak, LU, &lda, h_A, &lda);
            }else{
                lapackf77_slacpy(MagmaUpperStr, &Ak, &Ak, LU, &lda, h_A, &lda);
            }
            
            lapackf77_slarnv( &ione, ISEED, &sizeB, h_B );
            memcpy(h_B1, h_B, sizeB*sizeof(float));
            /* =====================================================================
               Performs operation using MAGMA-BLAS
               =================================================================== */
            magma_ssetmatrix( Ak, Ak, h_A, lda, d_A, ldda );
            magma_ssetmatrix( M, N, h_B, ldb, d_B, lddb );
            
            magma_time = magma_sync_wtime( NULL );
            magmablas_strsm( opts.side, opts.uplo, opts.transA, opts.diag, 
                             M, N,
                             alpha, d_A, ldda,
                                    d_B, lddb );
            magma_time = magma_sync_wtime( NULL ) - magma_time;
            magma_perf = gflops / magma_time;
            
            magma_sgetmatrix( M, N, d_B, lddb, h_Bmagma, ldb );
            
            /* =====================================================================
               Performs operation using CUDA-BLAS
               =================================================================== */
            magma_ssetmatrix( M, N, h_B, ldb, d_B, lddb );
            
            cublas_time = magma_sync_wtime( NULL );
            cublasStrsm( opts.side, opts.uplo, opts.transA, opts.diag,
                         M, N, 
                         alpha, d_A, ldda,
                                d_B, lddb );
            cublas_time = magma_sync_wtime( NULL ) - cublas_time;
            cublas_perf = gflops / cublas_time;
            
            magma_sgetmatrix( M, N, d_B, lddb, h_Bcublas, ldb );
            
            /* =====================================================================
               Performs operation using CPU BLAS
               =================================================================== */
            if ( opts.lapack ) {
                cpu_time = magma_wtime();
                blasf77_strsm( &opts.side, &opts.uplo, &opts.transA, &opts.diag, 
                               &M, &N,
                               &alpha, h_A, &lda,
                                       h_B, &ldb );
                cpu_time = magma_wtime() - cpu_time;
                cpu_perf = gflops / cpu_time;
            }
            
            /* =====================================================================
               Check the result
               =================================================================== */
            // ||b - Ax|| / (||A||*||x||)
            memcpy(h_X1, h_Bmagma, sizeB*sizeof(float));
            
            float alpha2 = MAGMA_S_DIV(  c_one, alpha );
            blasf77_strmm( &opts.side, &opts.uplo, &opts.transA, &opts.diag, 
                            &M, &N,
                            &alpha2, h_A, &lda,
                            h_X1, &ldb );

            blasf77_saxpy( &sizeB, &c_neg_one, h_B1, &ione, h_X1, &ione );
            float norm1 =  lapackf77_slange( "M", &M, &N, h_X1, &ldb, work );
            float normx =  lapackf77_slange( "M", &M, &N, h_Bmagma, &ldb, work );
            float normA =  lapackf77_slange( "M", &Ak, &Ak, h_A, &lda, work );


            magma_error = norm1/(normx*normA);

            memcpy(h_X2, h_Bcublas, sizeB*sizeof(float));
            blasf77_strmm( &opts.side, &opts.uplo, &opts.transA, &opts.diag, 
                            &M, &N,
                            &alpha2, h_A, &lda,
                            h_X2, &ldb );

            blasf77_saxpy( &sizeB, &c_neg_one, h_B1, &ione, h_X2, &ione );
            norm1 =  lapackf77_slange( "M", &M, &N, h_X2, &ldb, work );
            normx =  lapackf77_slange( "M", &M, &N, h_Bcublas, &ldb, work );
            normA =  lapackf77_slange( "M", &Ak, &Ak, h_A, &lda, work );
            
            cublas_error = norm1/(normx*normA);
            
            if ( opts.lapack ) {
                printf("%5d %5d   %7.2f (%7.2f)   %7.2f (%7.2f)   %7.2f (%7.2f)   %8.2e     %8.2e\n",
                        (int) M, (int) N,
                        magma_perf,  1000.*magma_time,
                        cublas_perf, 1000.*cublas_time,
                        cpu_perf,    1000.*cpu_time,
                        magma_error, cublas_error );
            }
            else {
                printf("%5d %5d   %7.2f (%7.2f)   %7.2f (%7.2f)     ---   (  ---  )   %8.2e     %8.2e\n",
                        (int) M, (int) N,
                        magma_perf,  1000.*magma_time,
                        cublas_perf, 1000.*cublas_time,
                        magma_error, cublas_error );
            }
            
            TESTING_FREE_CPU( h_A  );
            TESTING_FREE_CPU( LU   );
            TESTING_FREE_CPU( LUT  );
            TESTING_FREE_CPU( h_B  );
            TESTING_FREE_CPU( h_B1 );
            TESTING_FREE_CPU( h_X1 );
            TESTING_FREE_CPU( h_X2 );
            TESTING_FREE_CPU( h_Bcublas );
            TESTING_FREE_CPU( h_Bmagma  );
            
            TESTING_FREE_DEV( d_A );
            TESTING_FREE_DEV( d_B );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }

    TESTING_FINALIZE();
    return 0;
}
