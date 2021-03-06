/*
   -- MAGMA (version 1.7.0) --
   Univ. of Tennessee, Knoxville
   Univ. of California, Berkeley
   Univ. of Colorado, Denver
   @date September 2015

   @author Azzam Haidar
   @author Adrien Remy

   @generated from zgetrf_nopiv_batched.cpp normal z -> s, Fri Sep 11 18:29:32 2015
 */

#include "common_magma.h"
#include "batched_kernel_param.h"
#include "cublas_v2.h"

///////////////////////////////////////////////////////////////////////////////////////
/**
    Purpose
    -------
    SGETRF computes an LU factorization of a general M-by-N matrix A without pivoting

    The factorization has the form
        A = L * U
    where L is lower triangular with unit
    diagonal elements (lower trapezoidal if m > n), and U is upper
    triangular (upper trapezoidal if m < n).

    This is the right-looking Level 3 BLAS version of the algorithm.

    This is a batched version that factors batchCount M-by-N matrices in parallel.
    dA, and info become arrays with one entry per matrix.

    Arguments
    ---------
    @param[in]
    m       INTEGER
            The number of rows of each matrix A.  M >= 0.

    @param[in]
    n       INTEGER
            The number of columns of each matrix A.  N >= 0.

    @param[in,out]
    dA_array    Array of pointers, dimension (batchCount).
            Each is a REAL array on the GPU, dimension (LDDA,N).
            On entry, each pointer is an M-by-N matrix to be factored.
            On exit, the factors L and U from the factorization
            A = P*L*U; the unit diagonal elements of L are not stored.

    @param[in]
    ldda    INTEGER
            The leading dimension of each array A.  LDDA >= max(1,M).

    @param[out]
    info_array  Array of INTEGERs, dimension (batchCount), for corresponding matrices.
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value
                  or another error occured, such as memory allocation failed.
      -     > 0:  if INFO = i, U(i,i) is exactly zero. The factorization
                  has been completed, but the factor U is exactly
                  singular, and division by zero will occur if it is used
                  to solve a system of equations.

    @param[in]
    batchCount  INTEGER
                The number of matrices to operate on.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_sgesv_comp
    ********************************************************************/
extern "C" magma_int_t
magma_sgetrf_nopiv_batched(
        magma_int_t m, magma_int_t n,
        float **dA_array, 
        magma_int_t ldda,
        magma_int_t *info_array, 
        magma_int_t batchCount, magma_queue_t queue)
{
#define A(i_, j_)  (A + (i_) + (j_)*ldda)   
    magma_int_t min_mn = min(m, n);
    cudaMemset(info_array, 0, batchCount*sizeof(magma_int_t));

    /* Check arguments */
    magma_int_t arginfo = 0;
    if (m < 0)
        arginfo = -1;
    else if (n < 0)
        arginfo = -2;
    else if (ldda < max(1,m))
        arginfo = -4;

    if (arginfo != 0) {
        magma_xerbla( __func__, -(arginfo) );
        return arginfo;
    }

    /* Quick return if possible */
    if (m == 0 || n == 0)
        if (min_mn == 0 ) return arginfo;

    if ( m >  2048 || n > 2048 ) {
        #ifndef MAGMA_NOWARNING
        printf("=========================================================================================\n");
        printf("   WARNING batched routines are designed for small sizes it might be better to use the\n   Native/Hybrid classical routines if you want performance\n");
        printf("=========================================================================================\n");
        #endif
    }


    float neg_one = MAGMA_S_NEG_ONE;
    float one  = MAGMA_S_ONE;
    magma_int_t nb, recnb, ib, i, k, pm, use_stream;
    magma_get_sgetrf_batched_nbparam(n, &nb, &recnb);


    cublasHandle_t myhandle;
    cublasCreate_v2(&myhandle);
    cublasSetStream(myhandle, queue);
 

    float **dA_displ   = NULL;
    float **dW0_displ  = NULL;
    float **dW1_displ  = NULL;
    float **dW2_displ  = NULL;
    float **dW3_displ  = NULL;
    float **dW4_displ  = NULL;
    float **dinvA_array = NULL;
    float **dwork_array = NULL;

    magma_malloc((void**)&dA_displ,   batchCount * sizeof(*dA_displ));
    magma_malloc((void**)&dW0_displ,  batchCount * sizeof(*dW0_displ));
    magma_malloc((void**)&dW1_displ,  batchCount * sizeof(*dW1_displ));
    magma_malloc((void**)&dW2_displ,  batchCount * sizeof(*dW2_displ));
    magma_malloc((void**)&dW3_displ,  batchCount * sizeof(*dW3_displ));
    magma_malloc((void**)&dW4_displ,  batchCount * sizeof(*dW4_displ));
    magma_malloc((void**)&dinvA_array, batchCount * sizeof(*dinvA_array));
    magma_malloc((void**)&dwork_array, batchCount * sizeof(*dwork_array));

    magma_int_t invA_msize = magma_roundup( n, TRI_NB )*TRI_NB;
    magma_int_t dwork_msize = max(m,n)*nb;
    float* dinvA      = NULL;
    float* dwork      = NULL; // dinvA and dwork are workspace in strsm
    float **cpuAarray = NULL;
    magma_smalloc( &dinvA, invA_msize * batchCount);
    magma_smalloc( &dwork, dwork_msize * batchCount );
    magma_malloc_cpu((void**) &cpuAarray, batchCount*sizeof(float*));
   /* check allocation */
    if ( dA_displ  == NULL || dW0_displ == NULL || dW1_displ   == NULL || dW2_displ   == NULL || 
         dW3_displ == NULL || dW4_displ == NULL || dinvA_array == NULL || dwork_array == NULL || 
         dinvA     == NULL || dwork     == NULL || cpuAarray   == NULL ) {
        magma_free(dA_displ);
        magma_free(dW0_displ);
        magma_free(dW1_displ);
        magma_free(dW2_displ);
        magma_free(dW3_displ);
        magma_free(dW4_displ);
        magma_free(dinvA_array);
        magma_free(dwork_array);
        magma_free( dinvA );
        magma_free( dwork );
        magma_free_cpu(cpuAarray);
        magma_int_t info = MAGMA_ERR_DEVICE_ALLOC;
        magma_xerbla( __func__, -(info) );
        return info;
    }

    magmablas_slaset_q(MagmaFull, invA_msize, batchCount, MAGMA_S_ZERO, MAGMA_S_ZERO, dinvA, invA_msize, queue);
    magmablas_slaset_q(MagmaFull, dwork_msize, batchCount, MAGMA_S_ZERO, MAGMA_S_ZERO, dwork, dwork_msize, queue);
    sset_pointer(dwork_array, dwork, n, 0, 0, dwork_msize, batchCount, queue);
    sset_pointer(dinvA_array, dinvA, TRI_NB, 0, 0, invA_msize, batchCount, queue);

    // printf(" I am in sgetrfbatched\n");
    magma_int_t streamid;
    const magma_int_t nbstreams=10;
    magma_queue_t stream[nbstreams];
    for (i=0; i < nbstreams; i++) {
        magma_queue_create( &stream[i] );
    }
    magma_getvector( batchCount, sizeof(float*), dA_array, 1, cpuAarray, 1);


    //printf(" I am after malloc\n");

    for (i = 0; i < min_mn; i += nb) 
    {
        ib = min(nb, min_mn-i);
        pm = m-i;
        magma_sdisplace_pointers(dA_displ, dA_array, ldda, i, i, batchCount, queue);
        sset_pointer(dwork_array, dwork, nb, 0, 0, dwork_msize, batchCount, queue);
#if 0
        /* buggy: TODO */
        arginfo = magma_sgetrf_panel_nopiv_batched(
                pm, ib,
                dA_displ, ldda,
                dwork_array, nb, 
                dinvA_array, invA_msize, 
                dW0_displ, dW1_displ, dW2_displ, 
                dW3_displ, dW4_displ,
                info_array, i,
                batchCount, myhandle, queue); 
 
#else
        arginfo = magma_sgetrf_recpanel_nopiv_batched(
                pm, ib, 32,
                dA_displ, ldda,
                dwork_array, nb, 
                dinvA_array, invA_msize, 
                dW0_displ, dW1_displ, dW2_displ, 
                dW3_displ, dW4_displ,
                info_array, i,
                batchCount, myhandle, queue);   
#endif

        if (arginfo != 0 ) goto fin;

#define RUN_ALL
#ifdef RUN_ALL

        if ( (i + ib) < n)
        {
            // swap right side and trsm     
            //magma_sdisplace_pointers(dA_displ, dA_array, ldda, i, i+ib, batchCount);
            sset_pointer(dwork_array, dwork, nb, 0, 0, dwork_msize, batchCount, queue); // I don't think it is needed Azzam

            magma_sdisplace_pointers(dA_displ, dA_array, ldda, i, i, batchCount, queue);
            magma_sdisplace_pointers(dW0_displ, dA_array, ldda, i, i+ib, batchCount, queue);
            magmablas_strsm_work_batched( MagmaLeft, MagmaLower, MagmaNoTrans, MagmaUnit, 1,
                    ib, n-i-ib,
                    MAGMA_S_ONE,
                    dA_displ,    ldda, // dA
                    dW0_displ,   ldda, // dB
                    dwork_array,  nb, // dX
                    dinvA_array,  invA_msize, 
                    dW1_displ,   dW2_displ, 
                    dW3_displ,   dW4_displ,
                    1, batchCount, queue, myhandle);

            if ( (i + ib) < m)
            {    
                // if gemm size is > 160 use a streamed classical cublas gemm since it is faster
                // the batched is faster only when M=N <= 160 for K40c
                //-------------------------------------------
                //          USE STREAM  GEMM
                //-------------------------------------------
                use_stream = magma_srecommend_cublas_gemm_stream(MagmaNoTrans, MagmaNoTrans, m-i-ib, n-i-ib, ib);

                if (use_stream)
                { 
                    //printf("caling streamed dgemm %d %d %d \n", m-i-ib, n-i-ib, ib);

                    // since it use different stream I need to wait the TRSM and swap.
                    // But since the code use the NULL stream everywhere, 
                    // so I don't need it, because the NULL stream do the sync by itself
                    // magma_queue_sync(NULL);
                    magma_queue_sync(queue); 
                    for (k=0; k < batchCount; k++)
                    {
                        streamid = k%nbstreams;                                       
                        magmablasSetKernelStream(stream[streamid]);
                        magma_sgemm(MagmaNoTrans, MagmaNoTrans, 
                                m-i-ib, n-i-ib, ib,
                                neg_one, cpuAarray[k] + (i+ib)+i*ldda, ldda, 
                                         cpuAarray[k] + i+(i+ib)*ldda, ldda,
                                one,     cpuAarray[k] + (i+ib)+(i+ib)*ldda, ldda);
                    }
                    // need to synchronise to be sure that sgetf2 do not start before
                    // finishing the update at least of the next panel
                    // BUT no need for it as soon as the other portion of the code 
                    // use the NULL stream which do the sync by itself 
                    //magma_device_sync(); 
                     if ( queue != NULL ) {
                         for (magma_int_t s=0; s < nbstreams; s++)
                             magma_queue_sync(stream[s]);
                     }
                     magmablasSetKernelStream(queue);
                }
                //-------------------------------------------
                //          USE BATCHED GEMM
                //-------------------------------------------
                else
                {
                    magma_sdisplace_pointers(dA_displ, dA_array,  ldda, i+ib,    i, batchCount, queue);
                    magma_sdisplace_pointers(dW1_displ, dA_array, ldda,    i, i+ib, batchCount, queue);
                    magma_sdisplace_pointers(dW2_displ, dA_array, ldda, i+ib, i+ib, batchCount, queue);
                    //printf("caling batched dgemm %d %d %d \n", m-i-ib, n-i-ib, ib);
                    magma_sgemm_batched( MagmaNoTrans, MagmaNoTrans, m-i-ib, n-i-ib, ib, 
                                         neg_one, dA_displ, ldda, 
                                         dW1_displ, ldda, 
                                         one,  dW2_displ, ldda, 
                                         batchCount, queue, myhandle);
                } // end of batched/stream gemm
            } // end of if ( (i + ib) < m) 
        } // end of if ( (i + ib) < n)
#endif
    }// end of for

fin:
    magmablasSetKernelStream(queue);
    magma_queue_sync(queue);
    for (k=0; k < nbstreams; k++) {
        magma_queue_destroy( stream[k] );
    }
    cublasDestroy_v2(myhandle);

    magma_free(dA_displ);
    magma_free(dW0_displ);
    magma_free(dW1_displ);
    magma_free(dW2_displ);
    magma_free(dW3_displ);
    magma_free(dW4_displ);
    magma_free(dinvA_array);
    magma_free(dwork_array);
    magma_free( dinvA );
    magma_free( dwork );
    magma_free_cpu(cpuAarray);

    return arginfo;
}
