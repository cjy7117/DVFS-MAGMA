/*
 *  -- MAGMA (version 1.3.0) --
 *     Univ. of Tennessee, Knoxville
 *     Univ. of California, Berkeley
 *     Univ. of Colorado, Denver
 *     November 2012
 *
 *  @generated d Wed Nov 14 22:54:12 2012
 *
 **/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <cuda_runtime_api.h>
#include <cublas.h>
#include <cblas.h>

#include "flops.h"
#include "magma.h"
#include "magmablas.h"
#include "magma_lapack.h"
#include "testings.h"

#define PRECISION_d

#if defined(PRECISION_z) || defined(PRECISION_c)
#define FLOPS(n) ( 6. * FMULS_SYMV(n) + 2. * FADDS_SYMV(n))
#else
#define FLOPS(n) (      FMULS_SYMV(n) +      FADDS_SYMV(n))
#endif

#define MultiGPUs
#define validate


#if (GPUSHMEM >= 200)

void fillZero(double *A, int size);
extern "C"
magma_int_t
magmablas_dsymv2( char uplo, magma_int_t n,
                      double alpha,
                      double *A, magma_int_t lda,
                      double *X, magma_int_t incx,
                      double beta,
                      double *Y, magma_int_t incy,
                      double *work, magma_int_t lwork);
extern "C"
magma_int_t
magmablas_dsymv2_mgpu_offset( char uplo, magma_int_t n,
                      double alpha,
                      double **A, magma_int_t lda,
                      double **X, magma_int_t incx,
                      double beta,
                      double **Y, magma_int_t incy,
                      double **work, magma_int_t lwork,
              magma_int_t num_gpus, 
              magma_int_t nb,
              magma_int_t offset);


extern "C"
magma_int_t
magmablas_dsymv2_mgpu_32_offset( char uplo, magma_int_t n,
                      double alpha,
                      double **A, magma_int_t lda,
                      double **X, magma_int_t incx,
                      double beta,
                      double **Y, magma_int_t incy,
                      double **work, magma_int_t lwork,
              magma_int_t num_gpus, 
              magma_int_t nb,
              magma_int_t offset);



#endif

int main(int argc, char **argv)
{        
#if (GPUSHMEM >= 200)
    TESTING_CUDA_INIT();
    cudaSetDevice(0);

    magma_timestr_t  start, end;
    double      flops, magma_perf, cuda_perf, error, work[1];
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    double c_neg_one = MAGMA_D_NEG_ONE;
    magma_int_t n_local[4];

    FILE        *fp ; 
    magma_int_t N, m, i, j, lda, LDA, M;
    magma_int_t matsize;
    magma_int_t vecsize;
    magma_int_t istart = 64;
    magma_int_t incx = 1;
    char        uplo = MagmaLower;

    double alpha = MAGMA_D_MAKE(1., 0.); // MAGMA_D_MAKE(  1.5, -2.3 );
    double beta  = MAGMA_D_MAKE(0., 0.); // MAGMA_D_MAKE( -0.6,  0.8 );
    double *A, *X, *Y[4], *Ycublas, *Ymagma;
    double *dA, *dX[4], *dY[4], *d_lA[4], *dYcublas ;

    cudaStream_t stream[4][10];
    double *C_work;
    double *dC_work[4];

    magma_int_t num_gpus = 1, max_num_gpus, nb;
    magma_int_t blocks, workspace;
    magma_int_t offset;
    
//    offset = 257;

    if (argc != 1){
        for(i = 1; i<argc; i++){
            if (strcmp("-N", argv[i])==0)
            {
                N = atoi(argv[++i]);
                istart = N;
            }
            else if (strcmp("-M", argv[i])==0)
                M = atoi(argv[++i]);
            else if (strcmp("-NGPU", argv[i])==0)
              num_gpus = atoi(argv[++i]);
            else if (strcmp("-offset", argv[i])==0)
              offset = atoi(argv[++i]);
        }
        if ( M == 0 ) {
            M = N;
        }
        if ( N == 0 ) {
            N = M;
        }
        if (M>0 && N>0)
        {    printf("  testing_dsymv_mgpu -M %d -N %d -NGPU %d\n\n", M, N, num_gpus);
            printf("  in %c side \n", uplo);
        }
        else
            {
                printf("\nUsage: \n");
                printf("  testing_dsymv_mgpu -M %d -N %d -NGPU %d\n\n", 
                       1024, 1024, 1);
                exit(1);
            }
    }
    else {
#if defined(PRECISION_z)
        M = N = 8000;
      
#else
        M = N = 12480;

#endif 
        num_gpus = 2;
        offset = 0;
        printf("\nUsage: \n");
        printf("  testing_dsymv_mgpu -M %d -N %d -NGPU %d\n\n", M, N, num_gpus);
    }
         

///////////////////////////////////////////////////////////////////////////////////////
    cudaGetDeviceCount(&max_num_gpus);
    if (num_gpus > max_num_gpus){
      printf("More GPUs requested than available. Have to change it.\n");
      num_gpus = max_num_gpus;
    }
    printf("Number of GPUs to be used = %d\n", num_gpus);
    for(int i=0; i< num_gpus; i++)
    {
      cudaStreamCreate(&stream[i][0]);
    }
    

    LDA = ((N+31)/32)*32;
    matsize = N*LDA;
    vecsize = N*incx;
    nb = 32;
    //nb = 64;

    printf("block size = %d\n", nb);
   
    TESTING_MALLOC( A, double, matsize );
    TESTING_MALLOC( X, double, vecsize );
    TESTING_MALLOC( Ycublas, double, vecsize );
    TESTING_MALLOC( Ymagma,  double, vecsize );
    for(i=0; i<num_gpus; i++)
    {     
    TESTING_MALLOC( Y[i], double, vecsize );
    }

    cudaSetDevice(0);
    TESTING_DEVALLOC( dA, double, matsize );
    TESTING_DEVALLOC( dYcublas, double, vecsize );

    for(i=0; i<num_gpus; i++)
    {      
      n_local[i] = ((N/nb)/num_gpus)*nb;
      if (i < (N/nb)%num_gpus)
        n_local[i] += nb;
      else if (i == (N/nb)%num_gpus)
        n_local[i] += N%nb;

      cudaSetDevice(i);

      TESTING_DEVALLOC( d_lA[i], double, LDA*n_local[i] );// potentially bugged 
      TESTING_DEVALLOC( dX[i], double, vecsize );
      TESTING_DEVALLOC( dY[i], double, vecsize );
      
      printf("device %2d n_local = %4d\n", i, n_local[i]); 

    }
    cudaSetDevice(0);

      

///////////////////////////////////////////////////////////////////////

    /* Initialize the matrix */
    lapackf77_dlarnv( &ione, ISEED, &matsize, A );
    /* Make A hermitian */
    { 
        magma_int_t i, j;
        for(i=0; i<N; i++) {
            A[i*LDA+i] = MAGMA_D_MAKE( MAGMA_D_REAL(A[i*LDA+i]), 0. );
            for(j=0; j<i; j++)
                A[i*LDA+j] = (A[j*LDA+i]);
        }
    }
        

      blocks    = N / nb + (N % nb != 0);
      workspace = LDA * (blocks + 1);
      TESTING_MALLOC(    C_work, double, workspace );
      for(i=0; i<num_gpus; i++){
             cudaSetDevice(i);  
             TESTING_DEVALLOC( dC_work[i], double, workspace );
             //fillZero(dC_work[i], workspace);
      }
      
     cudaSetDevice(0);


//////////////////////////////////////////////////////////////////////////////////////////////
   
    fp = fopen ("results_dsymv_mgpu.csv", "w") ;
    if( fp == NULL ){ printf("Couldn't open output file\n"); exit(1);}

    printf("SYMV double Precision\n\n");

    printf( "   n   CUBLAS,Gflop/s   MAGMABLAS,Gflop/s      \"error\"\n" 
            "==============================================================\n");
    fprintf(fp, "   n   CUBLAS,Gflop/s   MAGMABLAS,Gflop/s      \"error\"\n" 
            "==============================================================\n");


//    for( offset = 0; offset< N; offset ++ )
    
    for(int size = istart ; size <= N ; size += 128)
    {
    //    printf("offset = %d ", offset);
        m = size ;
    //    m = N;
        // lda = ((m+31)/32)*32;// 
        lda = LDA; 
        flops = FLOPS( (double)m ) / 1e6;

        printf(      "N %5d ", m );
        fprintf( fp, "%5d, ", m );

        vecsize = m * incx;
        lapackf77_dlarnv( &ione, ISEED, &vecsize, X );
        lapackf77_dlarnv( &ione, ISEED, &vecsize, Y[0] );

        /* =====================================================================
           Performs operation using CUDA-BLAS
           =================================================================== */
        cudaSetDevice(0);
        magmablas_dsetmatrix_1D_bcyclic(m, m, A, LDA, d_lA, lda, num_gpus, nb); 
        cudaSetDevice(0);

    
    
    magma_dsetmatrix( m, m, A, LDA, dA, lda );
        magma_dsetvector( m, Y[0], incx, dYcublas, incx );
        
        for(i=0; i<num_gpus; i++){
            cudaSetDevice(i);
            magma_dsetvector( m, X, incx, dX[i], incx );
            magma_dsetvector( m, Y[0], incx, dY[i], incx );


            blocks    = m / nb + (m % nb != 0);
            magma_dsetmatrix( lda, blocks, C_work, LDA, dC_work[i], lda );
            
            //cudaMemset(dC_work[i], 0, sizeof( double) * lda * blocks);
           
       }

        cudaSetDevice(0);
        start = get_current_time();
        cublasDsymv( uplo, m-offset, alpha, dA + offset + offset * lda, lda, dX[0] + offset, incx, beta, dYcublas + offset, incx );
         
        end = get_current_time();

        magma_dgetvector( m, dYcublas, incx, Ycublas, incx );
                
        
        cuda_perf = flops / GetTimerValue(start,end);
        printf(     "%11.2f", cuda_perf );
        fprintf(fp, "%11.2f,", cuda_perf );
       
        
        cudaSetDevice(0);

        
        start = get_current_time();
        

        if(nb == 32)
       { 

        magmablas_dsymv2_mgpu_32_offset( uplo, m, alpha, d_lA, lda, dX, incx, beta, dY, incx, 
                dC_work, workspace, num_gpus, nb, offset);
 
        }
        else // nb = 64
       { 

        magmablas_dsymv2_mgpu_offset( uplo, m, alpha, d_lA, lda, dX, incx, beta, dY, incx, 
                dC_work, workspace, num_gpus, nb, offset);
 
        }
    
            
        for(i=1; i<num_gpus; i++)
        {
           cudaSetDevice(i);
           cudaDeviceSynchronize();
        }
      
        end = get_current_time();
        magma_perf = flops / GetTimerValue(start,end); 
        printf(     "%11.2f", magma_perf );
        fprintf(fp, "%11.2f,", magma_perf );
       

        for(i=0; i<num_gpus; i++)
        {        
            cudaSetDevice(i);
            magma_dgetvector( m, dY[i], incx, Y[i], incx );
        }
        cudaSetDevice(0);

        
#ifdef validate        

        for( j= offset;j<m;j++)
        {
            for(i=1; i<num_gpus; i++)
            {

//            printf("Y[%d][%d] = %15.14f\n", i, j, Y[i][j].x);
#if defined(PRECISION_z) || defined(PRECISION_c)
            Y[0][j].x = Y[0][j].x + Y[i][j].x;
                        Y[0][j].y = Y[0][j].y + Y[i][j].y;
#else 
            Y[0][j] = Y[0][j] + Y[i][j];
            
#endif 

            }
        }

/*

#if defined(PRECISION_z) || defined(PRECISION_c)
        
        for( j=offset;j<m;j++)
        {
            if(Y[0][j].x != Ycublas[j].x)
            {
                     printf("Y-multi[%d] = %f, %f\n",  j, Y[0][j].x, Y[0][j].y );
                     printf("Ycublas[%d] = %f, %f\n",  j, Ycublas[j].x, Ycublas[j].y);
            }
        }

#else 

        for( j=offset;j<m;j++)
        {
            if(Y[0][j] != Ycublas[j])
            {
                     printf("Y-multi[%d] = %f\n",  j, Y[0][j] );
                     printf("Ycublas[%d] = %f\n",  j, Ycublas[j]);
            }
        }

#endif

*/        
        /* =====================================================================
           Computing the Difference Cublas VS Magma
           =================================================================== */
       
        int nw = m - offset ;
        blasf77_daxpy( &nw, &c_neg_one, Y[0] + offset, &incx, Ycublas + offset, &incx);
        error = lapackf77_dlange( "M", &nw, &ione, Ycublas + offset, &nw, work );
            
#if  0
        printf(      "\t\t %8.6e", error / m );
        fprintf( fp, "\t\t %8.6e", error / m );

        /*
         * Extra check with cblas vs magma
         */
        cblas_dcopy( m, Y, incx, Ycublas, incx );
        cblas_dsymv( CblasColMajor, CblasLower, m, 
                     (alpha), A, LDA, X, incx, 
                     (beta), Ycublas, incx );
 
        blasf77_daxpy( &m, &c_neg_one, Ymagma, &incx, Ycublas, &incx);
        error = lapackf77_dlange( "M", &m, &ione, Ycublas, &m, work );
#endif

        printf(      "\t\t %8.6e", error / m );
        fprintf( fp, "\t\t %8.6e", error / m );
 
#endif 
        printf("\n");        
        fprintf(fp, "\n");        
    }
    
    fclose( fp ) ; 

    /* Free Memory */
    TESTING_FREE( A );
    TESTING_FREE( X );
    TESTING_FREE( Ycublas );
    TESTING_FREE( Ymagma );
    TESTING_FREE( C_work );

    TESTING_DEVFREE( dA );
    TESTING_DEVFREE( dYcublas );
    
    for(i=0; i<num_gpus; i++)
    { 
        TESTING_FREE( Y[i] );
        cudaSetDevice(i);

        TESTING_DEVFREE( d_lA[i] )
        TESTING_DEVFREE( dX[i] );
        TESTING_DEVFREE( dY[i] );


        TESTING_DEVFREE( dC_work[i] );

    }

    cudaSetDevice(0);
 ///////////////////////////////////////////////////////////   
      

    /* Free device */
    TESTING_CUDA_FINALIZE();
#endif
    return 0;
}        
