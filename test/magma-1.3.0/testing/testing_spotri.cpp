/*
 *  -- MAGMA (version 1.3.0) --
 *     Univ. of Tennessee, Knoxville
 *     Univ. of California, Berkeley
 *     Univ. of Colorado, Denver
 *     November 2012
 *
 * @generated s Wed Nov 14 22:54:15 2012
 *
 **/
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

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing spotri
*/
int main( int argc, char** argv)
{
    TESTING_CUDA_INIT();

    magma_timestr_t       start, end;
    float           flops, gpu_perf, cpu_perf;
    float *h_A, *h_R;
    magma_int_t      N=0, n2, lda;
    magma_int_t      size[10] = {1024,2048,3072,4032,5184,6016,7040,8064,9088,10112};

    magma_int_t  i, info;
    const char  *uplo     = MagmaLowerStr;
    float c_neg_one = MAGMA_S_NEG_ONE;
    magma_int_t  ione     = 1;
    magma_int_t  ISEED[4] = {0,0,0,1};
    float       work[1], matnorm;

    if (argc != 1){
        for(i = 1; i<argc; i++){
            if (strcmp("-N", argv[i])==0)
                N = atoi(argv[++i]);
        }
        if (N>0) size[0] = size[9] = N;
        else exit(1);
    }
    else {
        printf("\nUsage: \n");
        printf("  testing_spotri -N %d\n\n", 1024);
    }

    /* Allocate host memory for the matrix */
    n2 = size[9] * size[9];
    TESTING_MALLOC(    h_A, float, n2);
    TESTING_HOSTALLOC( h_R, float, n2);

    printf("  N    CPU GFlop/s    GPU GFlop/s    ||R||_F / ||A||_F\n");
    printf("========================================================\n");
    for(i=0; i<10; i++){
        N     = size[i];
        lda   = N;
        n2    = lda*N;
        flops = FLOPS_SPOTRI( (float)N ) / 1000000;

        /* ====================================================================
           Initialize the matrix
           =================================================================== */
        lapackf77_slarnv( &ione, ISEED, &n2, h_A );
        /* Symmetrize and increase the diagonal */
        {
            magma_int_t i, j;
            for(i=0; i<N; i++) {
                MAGMA_S_SET2REAL( h_A[i*lda+i], ( MAGMA_S_REAL(h_A[i*lda+i]) + 1.*N ) );
                for(j=0; j<i; j++)
                    h_A[i*lda+j] = (h_A[j*lda+i]);
            }
        }
        lapackf77_slacpy( MagmaUpperLowerStr, &N, &N, h_A, &lda, h_R, &lda );

        /* ====================================================================
           Performs operation using MAGMA
           =================================================================== */
        /* warm-up */
        magma_spotrf(uplo[0], N, h_R, lda, &info);
        magma_spotri(uplo[0], N, h_R, lda, &info);
        lapackf77_slacpy( MagmaUpperLowerStr, &N, &N, h_A, &lda, h_R, &lda );

        /* factorize matrix */
        magma_spotrf(uplo[0], N, h_R, lda, &info);
        
        // check for exact singularity
        //h_R[ 10 + 10*lda ] = MAGMA_S_MAKE( 0.0, 0.0 );
        
        start = get_current_time();
        magma_spotri(uplo[0], N, h_R, lda, &info);
        end = get_current_time();
        if (info != 0)
            printf("magma_spotri returned error %d\n", (int) info);

        gpu_perf = flops / GetTimerValue(start, end);

        /* =====================================================================
           Performs operation using LAPACK
           =================================================================== */
        lapackf77_spotrf(uplo, &N, h_A, &lda, &info);
        
        start = get_current_time();
        lapackf77_spotri(uplo, &N, h_A, &lda, &info);
        end = get_current_time();
        if (info != 0)
            printf("lapackf77_spotri returned error %d\n", (int) info);

        cpu_perf = flops / GetTimerValue(start, end);

        /* =====================================================================
           Check the result compared to LAPACK
           =================================================================== */
        matnorm = lapackf77_slange("f", &N, &N, h_A, &N, work);
        blasf77_saxpy(&n2, &c_neg_one, h_A, &ione, h_R, &ione);
        printf("%5d    %6.2f         %6.2f        %e\n",
               (int) size[i], cpu_perf, gpu_perf,
               lapackf77_slange("f", &N, &N, h_R, &N, work) / matnorm );

        if (argc != 1)
            break;
    }

    /* Memory clean up */
    TESTING_FREE( h_A );
    TESTING_HOSTFREE( h_R );

    TESTING_CUDA_FINALIZE();
}
