/*
    -- MAGMA (version 1.7.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date September 2015

       @author Azzam Haidar

       @generated from testing_zhetrd_he2hb.cpp normal z -> s, Fri Sep 11 18:29:39 2015

*/

// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// includes, project
#include "flops.h"
#include "magma.h"
#include "magma_lapack.h"
#include "magma_sbulge.h"
#include "testings.h"
#include "magma_threadsetting.h"

// TODO include checkdiag.h if needed.

#define PRECISION_s


/* ////////////////////////////////////////////////////////////////////////////
   -- Testing ssytrd_sy2sb
*/
int main( int argc, char** argv)
{
    TESTING_INIT();

    real_Double_t    gpu_time, gpu_perf, gflops;
    float *h_A, *h_R, *h_work, *dT1;
    float *tau;
    float *D, *E;

    /* Matrix size */
    magma_int_t N, n2, lda, lwork, lwork0;  //ldt

    magma_int_t info;
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};

#if defined(CHECKEIG)
#if defined(PRECISION_z)  || defined(PRECISION_d)
    magma_int_t WANTZ=0;
    magma_int_t THREADS=1;
#endif
#endif

    magma_int_t NE = 0;
    magma_int_t NB = 0;
    magma_int_t ngpu = 1;
    
    magma_opts opts;
    opts.parse_opts( argc, argv );
    
    NB = opts.nb;
    if (NB < 1)
        NB  = 64; //64; //magma_get_ssytrd_sy2sb_nb(N);

    // what is NE ?
    if (NE < 1)
        NE  = 64; //N;  //magma_get_ssytrd_sy2sb_nb(N);  // N not yet initialized

    printf("%% N    GPU GFlop/s   \n");
    printf("%%====================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            N = opts.nsize[itest];
            lda = N;
            //ldt = N;
            n2  = N*lda;
            gflops = FLOPS_SSYTRD( N ) / 1e9;
            
            /* We suppose the magma NB is bigger than lapack NB */
            lwork0 = N*NB;
        
            /* Allocate host memory for the matrix */
            TESTING_MALLOC_CPU( h_A,    float, lda*N  );
            TESTING_MALLOC_CPU( tau,    float, N-1    );
            
            TESTING_MALLOC_PIN( h_R,    float, lda*N  );
            TESTING_MALLOC_PIN( h_work, float, lwork0 );
            TESTING_MALLOC_PIN( D, float, N );
            TESTING_MALLOC_PIN( E, float, N );
            
            //TESTING_MALLOC_DEV( dT1, float, (2*min(N,N) + roundup( N, 32 ))*NB );
            TESTING_MALLOC_DEV( dT1, float, (N*NB) );
        
            // if (WANTZ) gflops = 2.0*gflops;
    
            /* ====================================================================
               Initialize the matrix
               =================================================================== */
            lapackf77_slarnv( &ione, ISEED, &n2, h_A );
            magma_smake_symmetric( N, h_A, lda );
            
            lapackf77_slacpy( MagmaUpperLowerStr, &N, &N, h_A, &lda, h_R, &lda );
    
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            magma_device_t cdev;
            magma_getdevice( &cdev );
    
            gpu_time = magma_wtime();
            /*
            magma_ssytrd_sy2sb( opts.uplo, N, NB, h_R, lda, tau, h_work, lwork0, dT1, THREADS, &info);
            tband = magma_wtime - gpu_time();
            printf("  Finish BAND  N %d  NB %d  ngpu %d timing= %f\n", N, NB, ngpu, tband);
            magma_ssytrd_bsy2trc_v5(THREADS, WANTZ, opts.uplo, NE, N, NB, h_R, lda, D, E, dT1, ldt);
            */
    
            /*
            magma_ssytrd_sy2sb( opts.uplo, N, NB, h_R, lda, tau, h_work, lwork, dT1, THREADS, &info);
            tband = magma_wtime - gpu_time();
            printf("  Finish BAND  N %d  NB %d  ngpu %d timing= %f\n", N, NB, ngpu, tband);
            magma_ssytrd_bsy2trc(THREADS, WANTZ, opts.uplo, NE, N, NB, h_R, lda, D, E, dT1, ldt);
            */

            magma_range_t range = MagmaRangeAll;
            magma_int_t fraction_ev = 100;
            magma_int_t il, iu, m1;
            float vl=0., vu=0.;
    
            if (fraction_ev == 0) {
                il = N / 10;
                iu = N / 5+il;
            }
            else {
                il = 1;
                iu = (int)(fraction_ev*N);
                if (iu < 1) iu = 1;
            }
            float *hh_work;
            magma_int_t *iwork;
            magma_int_t /*nb,*/ /*lwork,*/ liwork;
            magma_int_t threads = magma_get_parallel_numthreads();

            #if defined(PRECISION_z) || defined(PRECISION_c)
            float *rwork;
            magma_int_t lrwork;
            #endif

            magma_ssyevdx_getworksize(N, threads, (opts.jobz == MagmaVec), 
                                     &lwork, 
                                     #if defined(PRECISION_z) || defined(PRECISION_c)
                                     &lrwork, 
                                     #endif
                                     &liwork);

            TESTING_MALLOC_PIN( hh_work, float, lwork  );
            TESTING_MALLOC_CPU( iwork,   magma_int_t,        liwork );
            #if defined(PRECISION_z) || defined(PRECISION_c)
            TESTING_MALLOC_PIN( rwork, float, lrwork );
            #endif
    
            if (ngpu == 1) {
                printf("calling ssyevdx_2stage 1 GPU\n");
                magma_ssyevdx_2stage( opts.jobz, range, opts.uplo, N,
                                h_R, lda,
                                vl, vu, il, iu,
                                &m1, D,
                                hh_work, lwork,
                                #if defined(PRECISION_z) || defined(PRECISION_c)
                                rwork, lrwork,
                                #endif
                                iwork, liwork,
                                &info);
            } else {
                printf("calling ssyevdx_2stage_m %d GPU\n", (int) ngpu);
                magma_ssyevdx_2stage_m(ngpu, opts.jobz, range, opts.uplo, N,
                                h_R, lda,
                                vl, vu, il, iu,
                                &m1, D,
                                hh_work, lwork,
                                #if defined(PRECISION_z) || defined(PRECISION_c)
                                rwork, lrwork,
                                #endif
                                iwork, liwork,
                                &info);
            }
    
            magma_setdevice( cdev );
            gpu_time = magma_wtime() - gpu_time;
            gpu_perf = gflops / gpu_time;
    
            /* =====================================================================
               Check the factorization
               =================================================================== */
            /*
            if ( opts.check ) {
                FILE *fp;
    
                printf("Writing input matrix in matlab_i_mat.txt ...\n");
                fp = fopen ("matlab_i_mat.txt", "w");
                if ( fp == NULL ) {
                    printf("Couldn't open output file\n");
                    return -1;
                }
    
                for (j=0; j < N; j++) {
                    for (k=0; k < N; k++) {
                        #if defined(PRECISION_z) || defined(PRECISION_c)
                        fprintf(fp, "%5d %5d %11.8f %11.8f\n", k+1, j+1,
                                h_A[k+j*lda].x, h_A[k+j*lda].y);
                        #else
                        fprintf(fp, "%5d %5d %11.8f\n", k+1, j+1, h_A[k+j*lda]);
                        #endif
                    }
                }
                fclose( fp );
    
                printf("Writing output matrix in matlab_o_mat.txt ...\n");
                fp = fopen ("matlab_o_mat.txt", "w");
                if ( fp == NULL ) {
                    printf("Couldn't open output file\n");
                    return -1;
                }
    
                for (j=0; j < N; j++) {
                    for (k=0; k < N; k++) {
                        #if defined(PRECISION_z) || defined(PRECISION_c)
                        fprintf(fp, "%5d %5d %11.8f %11.8f\n", k+1, j+1,
                                h_R[k+j*lda].x, h_R[k+j*lda].y);
                        #else
                        fprintf(fp, "%5d %5d %11.8f\n", k+1, j+1, h_R[k+j*lda]);
                        #endif
                    }
                }
                fclose( fp );
            }
            */
    
            /* =====================================================================
               Print performance and error.
               =================================================================== */
#if defined(CHECKEIG)
#if defined(PRECISION_z)  || defined(PRECISION_d)
            if ( opts.check ) {
                printf("  Total N %5d  gflops %6.2f  timing %6.2f seconds\n", (int) N, gpu_perf, gpu_time );
                char JOBZ;
                if (WANTZ == 0)
                    JOBZ = 'N';
                else
                    JOBZ = 'V';
                float nrmI=0.0, nrm1=0.0, nrm2=0.0;
                int    lwork2 = 256*N;
                float *work2, *AINIT;
                float *rwork2, *D2;
                // TODO free this memory !
                magma_smalloc_cpu( &work2, lwork2 );
                magma_smalloc_cpu( &rwork2, N );
                magma_smalloc_cpu( &D2, N );
                magma_smalloc_cpu( &AINIT, N*lda );
                memcpy(AINIT, h_A, N*lda*sizeof(float));
                /* compute the eigenvalues using lapack routine to be able to compare to it and used as ref */
                cpu_time = magma_wtime();
                i= min(12, THREADS);
                
                magma_set_lapack_numthreads( i );
    
                lapackf77_ssyev( "N", "L", &N, h_A, &lda, D2, work2, &lwork2,
                    #if defined(PRECISION_z) || defined (PRECISION_c)
                    rwork2,
                    #endif
                    &info );
                
                ///* call eigensolver for our resulting tridiag [D E] and for Q */
                //dstedc_withZ('V', N, D, E, h_R, lda);
                ////ssterf_( &N, D, E, &info);
                ////
                cpu_time = magma_wtime() - cpu_time;
                printf("  Finish CHECK - EIGEN   timing= %f  threads %d\n", cpu_time, i);
    
                /*
                for (i=0; i < 10; i++)
                    printf(" voici lpk D[%d] %8.2e\n", i, D2[i]);
                */
    
                //float mydz=0.0, mydo=1.0;
                //float *Z;
                // magma_smalloc_cpu( &Z, N*lda );
                // dgemm_("N", "N", &N, &N, &N, &mydo, h_R, &lda, h_A, &lda, &mydz, Z, &lda);
    
                /* compare result */
                cmp_vals(N, D2, D, &nrmI, &nrm1, &nrm2);
    
                float *WORKAJETER;
                float *RWORKAJETER, *RESU;
                // TODO free this memory !
                magma_smalloc_cpu( &WORKAJETER, (2* N * N + N)  );
                magma_smalloc_cpu( &RWORKAJETER, N  );
                magma_smalloc_cpu( &RESU, 10 );
                int MATYPE;
                memset(RESU, 0, 10*sizeof(float));
    
                MATYPE=3;
                float NOTHING=0.0;
                cpu_time = magma_wtime();
                // check results
                scheck_eig_(&JOBZ, &MATYPE, &N, &NB, AINIT, &lda, &NOTHING, &NOTHING, D2, D, h_R, &lda, WORKAJETER, RWORKAJETER, RESU );
                cpu_time = magma_wtime() - cpu_time;
                printf("  Finish CHECK - results timing= %f\n", cpu_time);
                magma_set_lapack_numthreads( 1 );
    
                printf("\n");
                printf(" ================================================================================================================\n");
                printf("   ==> INFO voici  threads=%d    N=%d    NB=%d   WANTZ=%d\n", (int) THREADS, (int) N, (int) NB, (int) WANTZ);
                printf(" ================================================================================================================\n");
                printf("            DSBTRD                : %15s \n", "STATblgv9withQ    ");
                printf(" ================================================================================================================\n");
                if (WANTZ > 0)
                    printf(" | A - U S U' | / ( |A| n ulp )   : %15.3E   \n", RESU[0]);
                if (WANTZ > 0)
                    printf(" | I - U U' | / ( n ulp )         : %15.3E   \n", RESU[1]);
                printf(" | D1 - EVEIGS | / (|D| ulp)      : %15.3E   \n",  RESU[2]);
                printf(" max | D1 - EVEIGS |              : %15.3E   \n",  RESU[6]);
                printf(" ================================================================================================================\n\n\n");
                
                printf(" ****************************************************************************************************************\n");
                printf(" * Hello here are the norm  Infinite (max)=%8.2e  norm one (sum)=%8.2e   norm2(sqrt)=%8.2e *\n", nrmI, nrm1, nrm2);
                printf(" ****************************************************************************************************************\n\n");
            }
#endif
#endif
            
            printf("  Total N %5d  gflops %6.2f        timing %6.2f seconds\n", (int) N, gpu_perf, gpu_time );
            printf("%%===========================================================================\n\n\n");
            
            /* Memory clean up */
            TESTING_FREE_CPU( h_A );
            TESTING_FREE_CPU( tau );
            
            TESTING_FREE_PIN( h_R    );
            TESTING_FREE_PIN( h_work );
            TESTING_FREE_PIN( D      );
            TESTING_FREE_PIN( E      );
            
            TESTING_FREE_DEV( dT1 );
            
            /* TODO - not all memory has been freed inside loop */
            fflush( stdout );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }

    TESTING_FINALIZE();
    return EXIT_SUCCESS;
}
