/*
    -- MAGMA (version 1.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2012

    @author Stan Tomov

    @generated c Wed Nov 14 22:54:24 2012

*/

// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <cuda_runtime_api.h>
#include <cublas.h>

// includes, project
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"

#define absv(v1) ((v1)>0? (v1): -(v1))

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing cheevd
*/
int main( int argc, char** argv) 
{
    TESTING_CUDA_INIT();

    cuFloatComplex *h_A, *h_R, *h_work;
    float *rwork, *w1, *w2;
    magma_int_t *iwork;
    float gpu_time, cpu_time;

    magma_timestr_t start, end;

    /* Matrix size */
    magma_int_t N=0, n2;
    magma_int_t size[8] = {1024,2048,3072,4032,5184,6016,7040,8064};

    magma_int_t i, info;
    magma_int_t ione     = 1, izero = 0;
    magma_int_t ISEED[4] = {0,0,0,1};

    const char *uplo = MagmaLowerStr;
    const char *jobz = MagmaVectorsStr;

    magma_int_t checkres;
    float result[3], eps = lapackf77_slamch( "E" );

    if (argc != 1){
        for(i = 1; i<argc; i++){
            if (strcmp("-N", argv[i])==0) {
                N = atoi(argv[++i]);
            }
            else if ( strcmp("-JV", argv[i]) == 0 ) {
                jobz = MagmaVectorsStr;
            }
            else if ( strcmp("-JN", argv[i]) == 0 ) {
                jobz = MagmaNoVectorsStr;
            }
        }
        if (N>0)
            printf("  testing_cheevd -N %d [-JV] [-JN]\n\n", (int) N);
        else {
            printf("\nUsage: \n");
            printf("  testing_cheevd -N %d [-JV] [-JN]\n\n", (int) N);
            exit(1);
        }
    }
    else {
        printf("\nUsage: \n");
        printf("  testing_cheevd -N %d [-JV] [-JN]\n\n", 1024);
        N = size[7];
    }

    checkres  = getenv("MAGMA_TESTINGS_CHECK") != NULL;
    if ( checkres && jobz[0] == MagmaNoVectors ) {
        printf( "Cannot check results when vectors are not computed (jobz='N')\n" );
        checkres = false;
    }

    /* Query for workspace sizes */
    cuFloatComplex aux_work[1];
    float          aux_rwork[1];
    magma_int_t     aux_iwork[1];
    magma_cheevd( jobz[0], uplo[0],
                  N, h_R, N, w1,
                  aux_work,  -1,
                  aux_rwork, -1,
                  aux_iwork, -1,
                  &info );
    magma_int_t lwork, lrwork, liwork;
    lwork  = (magma_int_t) MAGMA_C_REAL( aux_work[0] );
    lrwork = (magma_int_t) aux_rwork[0];
    liwork = aux_iwork[0];

    /* Allocate host memory for the matrix */
    TESTING_MALLOC(    h_A, cuFloatComplex, N*N );
    TESTING_MALLOC(    w1,  float         , N   );
    TESTING_MALLOC(    w2,  float         , N   );
    TESTING_HOSTALLOC( h_R, cuFloatComplex, N*N );
    TESTING_HOSTALLOC( h_work, cuFloatComplex, lwork  );
    TESTING_MALLOC(    rwork,  float,          lrwork );
    TESTING_MALLOC(    iwork,  magma_int_t,     liwork );
    
    printf("  N     CPU Time(s)    GPU Time(s) \n");
    printf("===================================\n");
    for(i=0; i<8; i++){
        if (argc==1){
            N = size[i];
        }
        n2 = N*N;

        /* Initialize the matrix */
        lapackf77_clarnv( &ione, ISEED, &n2, h_A );
        for( int i=0; i<N; i++) {
            MAGMA_C_SET2REAL( h_A[i*N+i], MAGMA_C_REAL(h_A[i*N+i]) );
        }
        lapackf77_clacpy( MagmaUpperLowerStr, &N, &N, h_A, &N, h_R, &N );

        /* warm up run */
        magma_cheevd(jobz[0], uplo[0],
                     N, h_R, N, w1,
                     h_work, lwork, 
                     rwork, lrwork, 
                     iwork, liwork, 
                     &info);
        
        lapackf77_clacpy( MagmaUpperLowerStr, &N, &N, h_A, &N, h_R, &N );

        /* query for optimal workspace sizes */
        magma_cheevd(jobz[0], uplo[0],
                     N, h_R, N, w1,
                     h_work, -1,
                     rwork,  -1,
                     iwork,  -1,
                     &info);
        int lwork_save  = lwork;
        int lrwork_save = lrwork;
        int liwork_save = liwork;
        lwork  = min( lwork,  (magma_int_t) MAGMA_C_REAL( h_work[0] ));
        lrwork = min( lrwork, (magma_int_t) rwork[0] );
        liwork = min( liwork, iwork[0] );
        //printf( "lwork %d, query %d, used %d; liwork %d, query %d, used %d\n",
        //        lwork_save,  (magma_int_t) h_work[0], lwork,
        //        liwork_save, iwork[0], liwork );

        /* ====================================================================
           Performs operation using MAGMA
           =================================================================== */
        start = get_current_time();
        magma_cheevd(jobz[0], uplo[0],
                     N, h_R, N, w1,
                     h_work, lwork,
                     rwork, lrwork,
                     iwork, liwork,
                     &info);
        end = get_current_time();

        gpu_time = GetTimerValue(start,end)/1000.;

        lwork  = lwork_save;
        lrwork = lrwork_save;
        liwork = liwork_save;
        
        if ( checkres ) {
          /* =====================================================================
             Check the results following the LAPACK's [zcds]drvst routine.
             A is factored as A = U S U' and the following 3 tests computed:
             (1)    | A - U S U' | / ( |A| N )
             (2)    | I - U'U | / ( N )
             (3)    | S(with U) - S(w/o U) | / | S |
             =================================================================== */
          float temp1, temp2;
          cuFloatComplex *tau;

          lapackf77_chet21(&ione, uplo, &N, &izero,
                           h_A, &N,
                           w1, w1,
                           h_R, &N,
                           h_R, &N,
                           tau, h_work, rwork, &result[0]);
          
          lapackf77_clacpy( MagmaUpperLowerStr, &N, &N, h_A, &N, h_R, &N );
          magma_cheevd('N', uplo[0],
                       N, h_R, N, w2,
                       h_work, lwork,
                       rwork, lrwork,
                       iwork, liwork,
                       &info);

          temp1 = temp2 = 0;
          for(int j=0; j<N; j++){
            temp1 = max(temp1, absv(w1[j]));
            temp1 = max(temp1, absv(w2[j]));
            temp2 = max(temp2, absv(w1[j]-w2[j]));
          }
          result[2] = temp2 / temp1;
        }

        /* =====================================================================
           Performs operation using LAPACK
           =================================================================== */
        start = get_current_time();
        lapackf77_cheevd(jobz, uplo,
                         &N, h_A, &N, w2,
                         h_work, &lwork,
                         rwork, &lrwork,
                         iwork, &liwork,
                         &info);
        end = get_current_time();
        if (info < 0)
          printf("Argument %d of cheevd had an illegal value.\n", (int) -info);

        cpu_time = GetTimerValue(start,end)/1000.;

        /* =====================================================================
           Print execution time
           =================================================================== */
        printf("%5d     %6.2f         %6.2f\n",
               (int) N, cpu_time, gpu_time);
        if ( checkres ){
          printf("Testing the factorization A = U S U' for correctness:\n");
          printf("(1)    | A - U S U' | / (|A| N) = %e\n", result[0]*eps);
          printf("(2)    | I -   U'U  | /  N      = %e\n", result[1]*eps);
          printf("(3)    | S(w/ U)-S(w/o U)|/ |S| = %e\n\n", result[2]);
        }

        if (argc != 1)
            break;
    }
 
    /* Memory clean up */
    TESTING_FREE(       h_A);
    TESTING_FREE(        w1);
    TESTING_FREE(        w2);
    TESTING_FREE(     rwork);
    TESTING_FREE(     iwork);
    TESTING_HOSTFREE(h_work);
    TESTING_HOSTFREE(   h_R);

    /* Shutdown */
    TESTING_CUDA_FINALIZE();
}
