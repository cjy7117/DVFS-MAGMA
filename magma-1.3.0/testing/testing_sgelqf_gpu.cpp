/*
    -- MAGMA (version 1.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2012

       @generated s Wed Nov 14 22:54:18 2012

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
#define PRECISION_s
#if defined(PRECISION_z) || defined(PRECISION_c)
#define FLOPS(m, n) ( 6.*FMULS_GELQF(m, n) + 2.*FADDS_GELQF(m, n) )
#else
#define FLOPS(m, n) (    FMULS_GELQF(m, n) +    FADDS_GELQF(m, n) )
#endif

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing sgelqf_gpu
*/
int main( int argc, char** argv)
{
    TESTING_CUDA_INIT();

    magma_timestr_t       start, end;
    float           flops, gpu_perf, cpu_perf;
    float           matnorm, work[1];
    float  c_neg_one = MAGMA_S_NEG_ONE;
    float *h_A, *h_R, *tau, *h_work, tmp[1];
    float *d_A;

    /* Matrix size */
    magma_int_t M = 0, N = 0, n2, lda, lwork;
    magma_int_t size[7] = {1024,2048,3072,4032,5184,6016,7040};

    magma_int_t i, info, min_mn, nb;
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};

    if (argc != 1){
        for(i = 1; i<argc; i++){
            if (strcmp("-N", argv[i])==0)
                N = atoi(argv[++i]);
            else if (strcmp("-M", argv[i])==0)
                M = atoi(argv[++i]);
        }
        if ( M == 0 ) {
            M = N;
        }
        if ( N == 0 ) {
            N = M;
        }
        if (N>0 && M>0)
            printf("  testing_sgelqf_gpu -M %d -N %d\n\n", (int) M, (int) N);
        else
            {
                printf("\nUsage: \n");
                printf("  testing_sgelqf_gpu -M %d -N %d\n\n", (int) M, (int) N);
                exit(1);
            }
    }
    else {
        printf("\nUsage: \n");
        printf("  testing_sgelqf_gpu -M %d -N %d\n\n", 1024, 1024);
        M = N = size[6];
    }

    n2  = M * N;
    min_mn = min(M, N);
    nb = magma_get_sgeqrf_nb(M);

    lda = M;

    TESTING_MALLOC(    tau, float, min_mn);
    TESTING_MALLOC(    h_A, float, n2    );
    TESTING_HOSTALLOC( h_R, float, n2    );
    TESTING_DEVALLOC(  d_A, float, lda*N );

    lwork = -1;
    lapackf77_sgelqf(&M, &N, h_A, &M, tau, tmp, &lwork, &info);
    lwork = (magma_int_t)MAGMA_S_REAL( tmp[0] );
    lwork = max( lwork, M*nb );

    TESTING_HOSTALLOC( h_work, float, lwork );

    printf("  M     N   CPU GFlop/s   GPU GFlop/s    ||R||_F / ||A||_F\n");
    printf("==========================================================\n");
    for(i=0; i<7; i++){
        if (argc == 1){
            M = N = size[i];
        }
        min_mn= min(M, N);
        lda   = M;
        n2    = lda*N;
        flops = FLOPS( (float)M, (float)N ) / 1000000;

        /* Initialize the matrix */
        lapackf77_slarnv( &ione, ISEED, &n2, h_A );
        lapackf77_slacpy( MagmaUpperLowerStr, &M, &N, h_A, &lda, h_R, &lda );

        /* ====================================================================
           Performs operation using MAGMA
           =================================================================== */
        magma_ssetmatrix( M, N, h_R, lda, d_A, lda );
        start = get_current_time();
        magma_sgelqf_gpu( M, N, d_A, lda, tau, h_work, lwork, &info);
        end = get_current_time();
        if (info < 0)
            printf("Argument %d of magma_sgelqf_gpu had an illegal value.\n", (int) -info);
        
        gpu_perf = flops / GetTimerValue(start, end);

        /* =====================================================================
           Performs operation using LAPACK
           =================================================================== */
        start = get_current_time();
        lapackf77_sgelqf(&M, &N, h_A, &lda, tau, h_work, &lwork, &info);
        end = get_current_time();
        if (info < 0)
            printf("Argument %d of lapack_sgelqf had an illegal value.\n", (int) -info);
        
        cpu_perf = flops / GetTimerValue(start, end);

        /* =====================================================================
           Check the result compared to LAPACK
           =================================================================== */
        magma_sgetmatrix( M, N, d_A, lda, h_R, lda );
        matnorm = lapackf77_slange("f", &M, &N, h_A, &lda, work);
        blasf77_saxpy(&n2, &c_neg_one, h_A, &ione, h_R, &ione);

        printf("%5d %5d  %6.2f         %6.2f        %e\n",
               (int) M, (int) N, cpu_perf, gpu_perf,
               lapackf77_slange("f", &M, &N, h_R, &lda, work) / matnorm);

        if (argc != 1)
            break;
    }

    /* Memory clean up */
    TESTING_FREE( tau );
    TESTING_FREE( h_A );
    TESTING_HOSTFREE( h_R );
    TESTING_HOSTFREE( h_work );

    /* Shutdown */
    TESTING_CUDA_FINALIZE();
}
