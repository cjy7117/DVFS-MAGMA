/*
    -- MAGMA (version 1.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2012

       @generated d Wed Nov 14 22:54:19 2012

*/

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

// Flops formula
#define PRECISION_d
#if defined(PRECISION_z) || defined(PRECISION_c)
#define FLOPS_GEQRF(m, n      ) ( 6.*FMULS_GEQRF(m, n      ) + 2.*FADDS_GEQRF(m, n      ) )
#define FLOPS_GEQRS(m, n, nrhs) ( 6.*FMULS_GEQRS(m, n, nrhs) + 2.*FADDS_GEQRS(m, n, nrhs) )
#else
#define FLOPS_GEQRF(m, n      ) (    FMULS_GEQRF(m, n      ) +    FADDS_GEQRF(m, n      ) )
#define FLOPS_GEQRS(m, n, nrhs) (    FMULS_GEQRS(m, n, nrhs) +    FADDS_GEQRS(m, n, nrhs) )
#endif

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing dgeqrs
*/
int main( int argc, char** argv)
{
    TESTING_CUDA_INIT();
   
    magma_timestr_t       start, end;
    double           flops, gpu_perf, cpu_perf;
    double           matnorm, work[1];
    double  c_one     = MAGMA_D_ONE;
    double  c_neg_one = MAGMA_D_NEG_ONE;
    double *h_A, *h_A2, *h_B, *h_X, *h_R, *tau, *h_work, tmp[1];
    double *d_A, *d_B;

    /* Matrix size */
    magma_int_t M = 0, N = 0, n2;
    magma_int_t lda, ldb, ldda, lddb, lworkgpu, lhwork;
    magma_int_t size[10] = {1024,2048,3072,4032,5184,6016,7040,8064,9088,10112};

    magma_int_t i, info, min_mn, nb, l1, l2;
    magma_int_t ione     = 1;
    magma_int_t nrhs     = 3;
    magma_int_t ISEED[4] = {0,0,0,1};

    if (argc != 1){
        for(i = 1; i<argc; i++){
            if (strcmp("-N", argv[i])==0)
                N = atoi(argv[++i]);
            else if (strcmp("-M", argv[i])==0)
                M = atoi(argv[++i]);
            else if (strcmp("-nrhs", argv[i])==0)
                nrhs = atoi(argv[++i]);
        }
        if (N>0 && M>0 && M >= N)
            printf("  testing_dgeqrs_gpu -nrhs %d -M %d -N %d\n\n", (int) nrhs, (int) M, (int) N);
        else
            {
                printf("\nUsage: \n");
                printf("  testing_dgeqrs_gpu -nrhs %d  -M %d  -N %d\n\n", (int) nrhs, (int) M, (int) N);
                printf("  M has to be >= N, exit.\n");
                exit(1);
            }
    }
    else {
        printf("\nUsage: \n");
        printf("  testing_dgeqrs_gpu -nrhs %d  -M %d  -N %d\n\n", (int) nrhs, 1024, 1024);
        M = N = size[9];
    }

    ldda   = ((M+31)/32)*32;
    lddb   = ldda;
    n2     = M * N;
    min_mn = min(M, N);
    nb     = magma_get_dgeqrf_nb(M);
    lda = ldb = M;
    lworkgpu = (M-N + nb)*(nrhs+2*nb);

    /* Allocate host memory for the matrix */
    TESTING_MALLOC( tau,  double, min_mn   );
    TESTING_MALLOC( h_A,  double, lda*N    );
    TESTING_MALLOC( h_A2, double, lda*N    );
    TESTING_MALLOC( h_B,  double, ldb*nrhs );
    TESTING_MALLOC( h_X,  double, ldb*nrhs );
    TESTING_MALLOC( h_R,  double, ldb*nrhs );

    TESTING_DEVALLOC( d_A, double, ldda*N      );
    TESTING_DEVALLOC( d_B, double, lddb*nrhs   );

    /*
     * Get size for host workspace
     */
    lhwork = -1;
    lapackf77_dgeqrf(&M, &N, h_A, &M, tau, tmp, &lhwork, &info);
    l1 = (magma_int_t)MAGMA_D_REAL( tmp[0] );
    lhwork = -1;
    lapackf77_dormqr( MagmaLeftStr, MagmaTransStr, 
                      &M, &nrhs, &min_mn, h_A, &lda, tau,
                      h_X, &ldb, tmp, &lhwork, &info);
    l2 = (magma_int_t)MAGMA_D_REAL( tmp[0] );
    lhwork = max( max( l1, l2 ), lworkgpu );

    TESTING_MALLOC( h_work, double, lhwork );

    printf("                                         ||b-Ax|| / (N||A||)\n");
    printf("  M     N    CPU GFlop/s   GPU GFlop/s      CPU      GPU    \n");
    printf("============================================================\n");
    for(i=0; i<10; i++){
        if (argc == 1){
            M = N = size[i];
        }
        min_mn= min(M, N);
        ldb = lda = M;
        n2    = lda*N;
        ldda  = ((M+31)/32)*32;
        flops = (FLOPS_GEQRF( (double)M, (double)N ) 
                 + FLOPS_GEQRS( (double)M, (double)N, (double)nrhs )) / 1000000;

        /* Initialize the matrices */
        lapackf77_dlarnv( &ione, ISEED, &n2, h_A );
        lapackf77_dlacpy( MagmaUpperLowerStr, &M, &N, h_A, &lda, h_A2, &lda );

        n2 = M*nrhs;
        lapackf77_dlarnv( &ione, ISEED, &n2, h_B );
        lapackf77_dlacpy( MagmaUpperLowerStr, &M, &nrhs, h_B, &ldb, h_R, &ldb );

        /* ====================================================================
           Performs operation using MAGMA
           =================================================================== */
        magma_dsetmatrix( M, N,    h_A, lda, d_A, ldda );
        magma_dsetmatrix( M, nrhs, h_B, ldb, d_B, lddb );

        start = get_current_time();
        magma_dgels_gpu( MagmaNoTrans, M, N, nrhs, d_A, ldda, 
                         d_B, lddb, h_work, lworkgpu, &info);
        end = get_current_time();
        if (info < 0)
            printf("Argument %d of magma_dgels had an illegal value.\n", (int) -info);
        
        gpu_perf = flops / GetTimerValue(start, end);

        // Get the solution in h_X
        magma_dgetmatrix( N, nrhs, d_B, lddb, h_X, ldb );

        // compute the residual
        blasf77_dgemm( MagmaNoTransStr, MagmaNoTransStr, &M, &nrhs, &N, 
                       &c_neg_one, h_A, &lda, 
                                   h_X, &ldb, 
                       &c_one,     h_R, &ldb);
        matnorm = lapackf77_dlange("f", &M, &N, h_A, &lda, work);

        /* =====================================================================
           Performs operation using LAPACK
           =================================================================== */
        lapackf77_dlacpy( MagmaUpperLowerStr, &M, &nrhs, h_B, &ldb, h_X, &ldb );

        start = get_current_time();
        lapackf77_dgels( MagmaNoTransStr, &M, &N, &nrhs,
                         h_A, &lda, h_X, &ldb, h_work, &lhwork, &info);
        end = get_current_time();
        cpu_perf = flops / GetTimerValue(start, end);
        if (info < 0)
          printf("Argument %d of lapackf77_dgels had an illegal value.\n", (int) -info);

        blasf77_dgemm( MagmaNoTransStr, MagmaNoTransStr, &M, &nrhs, &N, 
                       &c_neg_one, h_A2, &lda, 
                                   h_X,  &ldb, 
                       &c_one,     h_B,  &ldb);

        printf("%5d %5d   %6.1f       %6.1f       %7.2e   %7.2e\n",
               (int) M, (int) N, cpu_perf, gpu_perf,
               lapackf77_dlange("f", &M, &nrhs, h_B, &M, work)/(min_mn*matnorm),
               lapackf77_dlange("f", &M, &nrhs, h_R, &M, work)/(min_mn*matnorm) );

        if (argc != 1)
            break;
    }

    /* Memory clean up */
    TESTING_FREE( tau );
    TESTING_FREE( h_A );
    TESTING_FREE( h_A2 );
    TESTING_FREE( h_B );
    TESTING_FREE( h_X );
    TESTING_FREE( h_R );
    TESTING_FREE( h_work );
    TESTING_DEVFREE( d_A );
    TESTING_DEVFREE( d_B );

    /* Shutdown */
    TESTING_CUDA_FINALIZE();
}
