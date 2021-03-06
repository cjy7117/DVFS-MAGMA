/*
    -- MAGMA (version 1.6.1) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date January 2015

       @generated from testing_zpotrf.cpp normal z -> d, Fri Jan 30 19:00:25 2015
*/
// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <iostream>

// includes, project
#include "flops.h"
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"
#include "../testing/testing_util.cpp"
#include "papi.h"

using namespace std;

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing dpotrf
*/
int main( int argc, char** argv)
{
    #define LIMITED_ONE_CORE 1//Forces the program to run on a given CPU core.

    if(LIMITED_ONE_CORE)
    {
        int affinity = map_cpu(0);
        if(affinity != 0)
        {
            printf("affinity failed\n");
            return -1;
        }
    }

    TESTING_INIT();

    real_Double_t   gflops, gpu_perf, gpu_time, cpu_perf, cpu_time;
    double *h_A, *h_R;
    magma_int_t N, n2, lda, info;
    double c_neg_one = MAGMA_D_NEG_ONE;
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    double      work[1], error;
    magma_int_t status = 0;

    magma_opts opts;
    parse_opts( argc, argv, &opts );
    opts.lapack |= opts.check;  // check (-c) implies lapack (-l)
    
    double tol = opts.tolerance * lapackf77_dlamch("E");
    
    printf("ngpu = %d, uplo = %s\n", (int) opts.ngpu, lapack_uplo_const(opts.uplo) );
    printf("    N   CPU GFlop/s (sec)   GPU GFlop/s (sec)   ||R_magma - R_lapack||_F / ||R_lapack||_F\n");
    printf("========================================================\n");

    double total_time = magma_wtime();

    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            N     = opts.nsize[itest];
            lda   = N;
            n2    = lda*N;
            gflops = FLOPS_DPOTRF( N ) / 1e9;
            
            TESTING_MALLOC_CPU( h_A, double, n2 );
            TESTING_MALLOC_PIN( h_R, double, n2 );

            double init_time = magma_wtime();
            
            /* Initialize the matrix */
            lapackf77_dlarnv( &ione, ISEED, &n2, h_A );
            magma_dmake_hpd( N, h_A, lda );
            lapackf77_dlacpy( MagmaUpperLowerStr, &N, &N, h_A, &lda, h_R, &lda );
            
            init_time = magma_wtime() - init_time;
            printf("init_time = %.6f\n", init_time);

            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            //SetGPUFreq(324, 324);
            SetGPUFreq(2600, 705);

            
            float real_time = 0.0;
			float proc_time = 0.0;
			long long flpins = 0.0;
			float mflops = 0.0;
			
			if (PAPI_flops(&real_time, &proc_time, &flpins, &mflops) < PAPI_OK) {
				cout << "PAPI ERROR" << endl;
				return -1;
			}  
			
//            gpu_time = magma_wtime();
            
            
            magma_dpotrf( opts.uplo, N, h_R, lda, &info );
            
            if (PAPI_flops(&real_time, &proc_time, &flpins, &mflops) < PAPI_OK) {
				cout << "PAPI ERROR" << endl;
				return -1;
			}
            cout<<"N="<<N<<"---time:"<<real_time<<"---gflops:"<<(double)gflops/real_time<<endl;
			PAPI_shutdown();   
//            gpu_time = magma_wtime() - gpu_time;
//            printf("time = %.6f\n", gpu_time);
//            gpu_perf = gflops / gpu_time;
//            if (info != 0)
//                printf("magma_dpotrf returned error %d: %s.\n",
//                       (int) info, magma_strerror( info ));
//            
//            if ( opts.lapack ) {
//                /* =====================================================================
//                   Performs operation using LAPACK
//                   =================================================================== */
//                cpu_time = magma_wtime();
//                lapackf77_dpotrf( lapack_uplo_const(opts.uplo), &N, h_A, &lda, &info );
//                cpu_time = magma_wtime() - cpu_time;
//                cpu_perf = gflops / cpu_time;
//                if (info != 0)
//                    printf("lapackf77_dpotrf returned error %d: %s.\n",
//                           (int) info, magma_strerror( info ));
//                
//                /* =====================================================================
//                   Check the result compared to LAPACK
//                   =================================================================== */
//                error = lapackf77_dlange("f", &N, &N, h_A, &lda, work);
//                blasf77_daxpy(&n2, &c_neg_one, h_A, &ione, h_R, &ione);
//                error = lapackf77_dlange("f", &N, &N, h_R, &lda, work) / error;
//                
//                printf("%5d   %7.2f (%7.2f)   %7.2f (%7.2f)   %8.2e   %s\n",
//                       (int) N, cpu_perf, cpu_time, gpu_perf, gpu_time,
//                       error, (error < tol ? "ok" : "failed") );
//                status += ! (error < tol);
//            }
//            else {
//                printf("%5d     ---   (  ---  )   %7.2f (%7.2f)     ---  \n",
//                       (int) N, gpu_perf, gpu_time );
//            }

//            total_time = () - total_time;
//            printf("total_time = %.6f\n", total_time);

            TESTING_FREE_CPU( h_A );
            TESTING_FREE_PIN( h_R );
            fflush( stdout );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }

    TESTING_FINALIZE();
    return status;
}
