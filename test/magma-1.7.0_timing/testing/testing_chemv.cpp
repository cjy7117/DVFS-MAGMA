/*
    -- MAGMA (version 1.7.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date September 2015

       @generated from testing_zhemv.cpp normal z -> c, Fri Sep 11 18:29:37 2015
       
       @author Mark Gates
*/
// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// includes, project
#include "testings.h"  // before magma.h, to include cublas_v2
#include "flops.h"
#include "magma.h"
#include "magma_lapack.h"

#define PRECISION_c

int main(int argc, char **argv)
{
    TESTING_INIT();

    const magmaFloatComplex c_neg_one = MAGMA_C_NEG_ONE;
    const magma_int_t        ione      = 1;
    
    real_Double_t   atomics_perf=0, atomics_time=0;
    real_Double_t   gflops, magma_perf=0, magma_time=0, cublas_perf, cublas_time, cpu_perf, cpu_time;
    float          magma_error=0, atomics_error=0, cublas_error, work[1];
    magma_int_t ISEED[4] = {0,0,0,1};
    magma_int_t N, lda, ldda, sizeA, sizeX, sizeY, blocks, ldwork;
    magma_int_t incx = 1;
    magma_int_t incy = 1;
    magma_int_t nb   = 64;
    magmaFloatComplex alpha = MAGMA_C_MAKE(  1.5, -2.3 );
    magmaFloatComplex beta  = MAGMA_C_MAKE( -0.6,  0.8 );
    magmaFloatComplex *A, *X, *Y, *Yatomics, *Ycublas, *Ymagma;
    magmaFloatComplex_ptr dA, dX, dY, dwork;
    magma_int_t status = 0;
    
    magma_opts opts;
    opts.parse_opts( argc, argv );
    
    float tol = opts.tolerance * lapackf77_slamch("E");

    printf("%% uplo = %s\n", lapack_uplo_const(opts.uplo) );
    printf("%%   N   MAGMA Gflop/s (ms)    Atomics Gflop/s      CUBLAS Gflop/s       CPU Gflop/s   MAGMA error  Atomics    CUBLAS\n");
    printf("%%=====================================================================================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            N = opts.nsize[itest];
            lda    = N;
            ldda   = magma_roundup( N, opts.align );  // multiple of 32 by default
            sizeA  = N*lda;
            sizeX  = N*incx;
            sizeY  = N*incy;
            gflops = FLOPS_CHEMV( N ) / 1e9;
            
            TESTING_MALLOC_CPU( A,        magmaFloatComplex, sizeA );
            TESTING_MALLOC_CPU( X,        magmaFloatComplex, sizeX );
            TESTING_MALLOC_CPU( Y,        magmaFloatComplex, sizeY );
            TESTING_MALLOC_CPU( Yatomics, magmaFloatComplex, sizeY );
            TESTING_MALLOC_CPU( Ycublas,  magmaFloatComplex, sizeY );
            TESTING_MALLOC_CPU( Ymagma,   magmaFloatComplex, sizeY );
            
            TESTING_MALLOC_DEV( dA, magmaFloatComplex, ldda*N );
            TESTING_MALLOC_DEV( dX, magmaFloatComplex, sizeX );
            TESTING_MALLOC_DEV( dY, magmaFloatComplex, sizeY );
            
            blocks = magma_ceildiv( N, nb );
            ldwork = ldda*blocks;
            TESTING_MALLOC_DEV( dwork, magmaFloatComplex, ldwork );
            
            magmablas_claset( MagmaFull, ldwork, 1, MAGMA_C_NAN, MAGMA_C_NAN, dwork, ldwork );
            magmablas_claset( MagmaFull, ldda,   N, MAGMA_C_NAN, MAGMA_C_NAN, dA,    ldda   );
            
            /* Initialize the matrix */
            lapackf77_clarnv( &ione, ISEED, &sizeA, A );
            magma_cmake_hermitian( N, A, lda );
            
            // should not use data from the opposite triangle -- fill with NAN to check
            magma_int_t N1 = N-1;
            if ( opts.uplo == MagmaUpper ) {
                lapackf77_claset( "Lower", &N1, &N1, &MAGMA_C_NAN, &MAGMA_C_NAN, &A[1], &lda );
            }
            else {
                lapackf77_claset( "Upper", &N1, &N1, &MAGMA_C_NAN, &MAGMA_C_NAN, &A[lda], &lda );
            }
            
            lapackf77_clarnv( &ione, ISEED, &sizeX, X );
            lapackf77_clarnv( &ione, ISEED, &sizeY, Y );
            
            /* =====================================================================
               Performs operation using CUBLAS
               =================================================================== */
            magma_csetmatrix( N, N, A, lda, dA, ldda );
            magma_csetvector( N, X, incx, dX, incx );
            magma_csetvector( N, Y, incy, dY, incy );
            
            magmablasSetKernelStream( opts.queue );  // opts.handle also uses opts.queue
            cublas_time = magma_sync_wtime( opts.queue );
            #ifdef HAVE_CUBLAS
                cublasChemv( opts.handle, cublas_uplo_const(opts.uplo),
                             N, &alpha, dA, ldda, dX, incx, &beta, dY, incy );
            #else
                magma_chemv( opts.uplo, N, alpha, dA, 0, ldda, dX, 0, incx, beta, dY, 0, incy, opts.queue );
            #endif
            cublas_time = magma_sync_wtime( opts.queue ) - cublas_time;
            cublas_perf = gflops / cublas_time;
            
            magma_cgetvector( N, dY, incy, Ycublas, incy );
            
            /* =====================================================================
               Performs operation using CUBLAS - using atomics
               =================================================================== */
            #ifdef HAVE_CUBLAS
                cublasSetAtomicsMode( opts.handle, CUBLAS_ATOMICS_ALLOWED );
                magma_csetvector( N, Y, incy, dY, incy );
                
                // sync on queue doesn't work -- need device sync or use NULL stream -- bug in CUBLAS?
                atomics_time = magma_sync_wtime( NULL /*opts.queue*/ );
                cublasChemv( opts.handle, cublas_uplo_const(opts.uplo),
                             N, &alpha, dA, ldda, dX, incx, &beta, dY, incy );
                atomics_time = magma_sync_wtime( NULL /*opts.queue*/ ) - atomics_time;
                atomics_perf = gflops / atomics_time;
                
                magma_cgetvector( N, dY, incy, Yatomics, incy );
                cublasSetAtomicsMode( opts.handle, CUBLAS_ATOMICS_NOT_ALLOWED );
            #endif
            
            /* =====================================================================
               Performs operation using MAGMABLAS
               =================================================================== */
            #ifdef HAVE_CUBLAS
                magma_csetvector( N, Y, incy, dY, incy );
                
                magma_time = magma_sync_wtime( opts.queue );
                if ( opts.version == 1 ) {
                    magmablas_chemv_work( opts.uplo, N, alpha, dA, ldda, dX, incx, beta, dY, incy, dwork, ldwork, opts.queue );
                }
                else {
                    // non-work interface (has added overhead)
                    magmablas_chemv( opts.uplo, N, alpha, dA, ldda, dX, incx, beta, dY, incy );
                }
                magma_time = magma_sync_wtime( opts.queue ) - magma_time;
                magma_perf = gflops / magma_time;
                
                magma_cgetvector( N, dY, incy, Ymagma, incy );
            #endif
            
            /* =====================================================================
               Performs operation using CPU BLAS
               =================================================================== */
            cpu_time = magma_wtime();
            blasf77_chemv( lapack_uplo_const(opts.uplo), &N, &alpha, A, &lda, X, &incx, &beta, Y, &incy );
            cpu_time = magma_wtime() - cpu_time;
            cpu_perf = gflops / cpu_time;
            
            /* =====================================================================
               Check the result
               =================================================================== */
            blasf77_caxpy( &N, &c_neg_one, Y, &incy, Ycublas, &incy );
            cublas_error = lapackf77_clange( "M", &N, &ione, Ycublas, &N, work ) / N;
            
            #ifdef HAVE_CUBLAS
                blasf77_caxpy( &N, &c_neg_one, Y, &incy, Yatomics, &incy );
                atomics_error = lapackf77_clange( "M", &N, &ione, Yatomics, &N, work ) / N;
                
                blasf77_caxpy( &N, &c_neg_one, Y, &incy, Ymagma, &incy );
                magma_error = lapackf77_clange( "M", &N, &ione, Ymagma, &N, work ) / N;
            #endif
            
            bool okay = (magma_error < tol && cublas_error < tol && atomics_error < tol);
            status += ! okay;
            printf("%5d   %7.2f (%7.2f)   %7.2f (%7.2f)   %7.2f (%7.2f)   %7.2f (%7.2f)   %8.2e   %8.2e   %8.2e   %s\n",
                   (int) N,
                   magma_perf,   1000.*magma_time,
                   atomics_perf, 1000.*atomics_time,
                   cublas_perf,  1000.*cublas_time,
                   cpu_perf,     1000.*cpu_time,
                   magma_error, cublas_error, atomics_error,
                   (okay ? "ok" : "failed"));
            
            TESTING_FREE_CPU( A );
            TESTING_FREE_CPU( X );
            TESTING_FREE_CPU( Y );
            TESTING_FREE_CPU( Ycublas  );
            TESTING_FREE_CPU( Yatomics );
            TESTING_FREE_CPU( Ymagma   );
            
            TESTING_FREE_DEV( dA );
            TESTING_FREE_DEV( dX );
            TESTING_FREE_DEV( dY );
            TESTING_FREE_DEV( dwork );
            fflush( stdout );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
      }

    TESTING_FINALIZE();
    return status;
}
