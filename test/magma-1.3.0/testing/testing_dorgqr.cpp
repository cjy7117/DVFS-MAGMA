/*
    -- MAGMA (version 1.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2012

       @generated d Wed Nov 14 22:54:22 2012

       @author Stan Tomov
       @author Mathieu Faverge
       @author Mark Gates
*/

// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <cuda_runtime_api.h>
#include <cublas.h>
#include <assert.h>

// includes, project
#include "flops.h"
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing dorgqr
*/
int main( int argc, char** argv )
{
    TESTING_CUDA_INIT();

    real_Double_t    gflops, gpu_perf=0., cpu_perf=0., gpu_time=0., cpu_time=0.;
    double           error=0., work[1];
    double  c_neg_one = MAGMA_D_NEG_ONE;
    double *hA, *hR, *tau, *h_work;
    double *dA, *dT;

    /* Matrix size */
    magma_int_t m=0, n=0, k=0;
    magma_int_t n2, lda, ldda, lwork, min_mn, nb;
    const int MAXTESTS = 10;
    magma_int_t msize[MAXTESTS] = { 1024, 2048, 3072, 4032, 5184, 6016, 7040, 8064, 9088, 10112 };
    magma_int_t nsize[MAXTESTS] = { 1024, 2048, 3072, 4032, 5184, 6016, 7040, 8064, 9088, 10112 };
    magma_int_t ksize[MAXTESTS] = { 1024, 2048, 3072, 4032, 5184, 6016, 7040, 8064, 9088, 10112 };
    
    magma_int_t info;
    magma_int_t ione     = 1;
    magma_int_t iseed[4] = {0,0,0,1};
    
    printf( "Usage: %s -N m,n,k -c\n"
            "    -N can be repeated %d times. m >= n >= k is required.\n"
            "    If only m,n is given, then n=k. If only m is given, then m=n=k.\n"
            "    -c or setting $MAGMA_TESTINGS_CHECK runs LAPACK and checks result.\n\n",
            argv[0], MAXTESTS );

    int checkres = (getenv("MAGMA_TESTINGS_CHECK") != NULL);

    int ntest = 0;
    magma_int_t nmax = 0;
    magma_int_t mmax = 0;
    magma_int_t kmax = 0;
    for( int i = 1; i < argc; i++ ) {
        if ( strcmp("-N", argv[i]) == 0 && i+1 < argc ) {
            magma_assert( ntest < MAXTESTS, "error: -N repeated more than maximum %d tests\n", MAXTESTS );
            info = sscanf( argv[++i], "%d,%d,%d", &m, &n, &k );
            if ( info == 3 && m >= n && n >= k && k > 0 ) {
                msize[ ntest ] = m;
                nsize[ ntest ] = n;
                ksize[ ntest ] = k;
            }
            else if ( info == 2 && m >= n && n > 0 ) {
                msize[ ntest ] = m;
                nsize[ ntest ] = n;
                ksize[ ntest ] = n;  // implicitly
            }
            else if ( info == 1 && m > 0 ) {
                msize[ ntest ] = m;
                nsize[ ntest ] = m;  // implicitly
                ksize[ ntest ] = m;  // implicitly
            }
            else {
                printf( "error: -N %s is invalid; ensure m >= n >= k.\n", argv[i] );
                exit(1);
            }
            mmax = max( mmax, msize[ntest] );
            nmax = max( nmax, nsize[ntest] );
            kmax = max( kmax, ksize[ntest] );
            ntest++;
        }
        else if ( strcmp("-c", argv[i]) == 0 ) {
            checkres = true;
        }
        else {
            printf( "invalid argument: %s\n", argv[i] );
            exit(1);
        }
    }
    if ( ntest == 0 ) {
        ntest = MAXTESTS;
        nmax = nsize[ntest-1];
        mmax = msize[ntest-1];
        kmax = ksize[ntest-1];
    }
    assert( nmax > 0 && mmax > 0 && kmax > 0 );
    
    // allocate memory for largest problem
    lda    = mmax;
    ldda   = ((mmax + 31)/32)*32;
    n2     = lda * nmax;
    min_mn = min(mmax, nmax);
    nb     = magma_get_dgeqrf_nb( mmax );
    lwork  = (mmax + 2*nmax+nb)*nb;

    TESTING_HOSTALLOC( hA,    double, lda*nmax  );
    TESTING_HOSTALLOC( h_work, double, lwork     );
    TESTING_MALLOC(    hR,    double, lda*nmax  );
    TESTING_MALLOC(    tau,   double, min_mn    );
    TESTING_DEVALLOC(  dA,    double, ldda*nmax );
    TESTING_DEVALLOC(  dT,    double, ( 2*min_mn + ((nmax + 31)/32)*32 )*nb );
    
    printf("    m     n     k   CPU GFlop/s (sec)   GPU GFlop/s (sec)   ||R|| / ||A||\n");
    printf("=========================================================================\n");
    for( int i = 0; i < ntest; ++i ){
        m = msize[i];
        n = nsize[i];
        k = ksize[i];
        assert( m >= n && n >= k );
        
        lda  = m;
        ldda = ((m + 31)/32)*32;
        n2 = lda*n;
        nb = magma_get_dgeqrf_nb( m );
        gflops = FLOPS_DORGQR( (double)m, (double)n, (double)k ) / 1e9;

        lapackf77_dlarnv( &ione, iseed, &n2, hA );
        lapackf77_dlacpy( MagmaUpperLowerStr, &m, &n, hA, &lda, hR, &lda );
        
        /* ====================================================================
           Performs operation using MAGMA
           =================================================================== */
        magma_dsetmatrix( m, n, hA, lda, dA, ldda );
        magma_dgeqrf_gpu( m, n, dA, ldda, tau, dT, &info );
        if ( info != 0 )
            printf("magma_dgeqrf_gpu returned error %d\n", info);
        magma_dgetmatrix( m, n, dA, ldda, hR, lda );
        
        gpu_time = magma_wtime();
        magma_dorgqr( m, n, k, hR, lda, tau, dT, nb, &info );
        gpu_time = magma_wtime() - gpu_time;
        if ( info != 0 )
            printf("magma_dorgqr_gpu returned error %d\n", info);
        
        gpu_perf = gflops / gpu_time;
        
        /* =====================================================================
           Performs operation using LAPACK
           =================================================================== */
        if ( checkres ) {
            error = lapackf77_dlange("f", &m, &n, hA, &lda, work );
            
            lapackf77_dgeqrf( &m, &n, hA, &lda, tau, h_work, &lwork, &info );
            if ( info != 0 )
                printf("lapackf77_dgeqrf returned error %d\n", info);
            
            cpu_time = magma_wtime();
            lapackf77_dorgqr( &m, &n, &k, hA, &lda, tau, h_work, &lwork, &info );
            cpu_time = magma_wtime() - cpu_time;
            if ( info != 0 )
                printf("lapackf77_dorgqr returned error %d\n", info);
            
            cpu_perf = gflops / cpu_time;

            // compute relative error |R|/|A| := |Q_magma - Q_lapack|/|A|
            blasf77_daxpy( &n2, &c_neg_one, hA, &ione, hR, &ione );
            error = lapackf77_dlange("f", &m, &n, hR, &lda, work) / error;
        }
        
        if ( checkres ) {
            printf("%5d %5d %5d   %7.1f (%7.2f)   %7.1f (%7.2f)   %8.2e\n",
                   m, n, k, cpu_perf, cpu_time, gpu_perf, gpu_time, error );
        }
        else {
            printf("%5d %5d %5d     ---   (  ---  )   %7.1f (%7.2f)     ---  \n",
                   m, n, k, gpu_perf, gpu_time );
        }
    }
    
    /* Memory clean up */
    TESTING_HOSTFREE( hA );
    TESTING_HOSTFREE( h_work );
    TESTING_FREE( hR );
    TESTING_FREE( tau );

    TESTING_DEVFREE( dA );
    TESTING_DEVFREE( dT );

    /* Shutdown */
    TESTING_CUDA_FINALIZE();
    return 0;
}
