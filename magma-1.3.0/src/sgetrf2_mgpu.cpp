/*
    -- MAGMA (version 1.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2010

       @generated s Wed Nov 14 22:53:05 2012

*/
#include <math.h>
#include "common_magma.h"
#include "trace.h"

void magmaSetDevice( int id ) {
    switch( id ) {
        case 0:
            magma_setdevice( 0 );
            break;
        case 1:
            magma_setdevice( 1 );
            break;
        case 2:
            magma_setdevice( 2 );
            break;
        case 3:
            magma_setdevice( 3 );
            break;
        default:
            magma_setdevice( id );
            break;
    }
}


// === Define what BLAS to use ============================================
#define PRECISION_s
#if (defined(PRECISION_s) || defined(PRECISION_d))
  //#define magma_sgemm magmablas_sgemm
  //#define cublasSgemm magmablas_sgemm
  /* =========================== A BUG TO BE FIXED ====================== */
  /* note: magma_blas_dtrsm seems to have a bug with not-multiple-of-32 N *
   * and rhs > N on Fermi (Pluto)?                                        */
  //#define cublasStrsm magmablas_strsm
#else
  //#define magmablas_strsm magma_strsm
#endif
// === End defining what BLAS to use =======================================


//#define PANEL_FACT_MC
#ifdef  PANEL_FACT_MC

/* ============ definitions required for sgetrf_mc ====================== */
#include <quark.h>

typedef struct context
{
      /* Number of CPU core in this context */
      magma_int_t num_cores;

      /* Number of GPUs in this context */
      magma_int_t num_gpus;

      /* GPU contexts */
      CUcontext *gpu_context;

      /* QUARK scheduler */
      Quark *quark;

      /* Block size, internally used for some algorithms */
      magma_int_t nb;

      /* Pointer to other global algorithm-dependent parameters */
      void *params;

} magma_context;
extern "C" magma_int_t
magma_sgetrf_mc(magma_context *cntxt,
                int *m, int *n,
                float *a, int *lda,
                int *ipiv, int *info);
/* ==================================================================== */

extern "C" magma_int_t
magma_sgetrf2_mgpu(magma_context *cntxt, magma_int_t num_gpus, 
                   magma_int_t m, magma_int_t n, magma_int_t nb, magma_int_t offset,
                   float **d_lAT, magma_int_t lddat, magma_int_t *ipiv,
                   float *d_lAP[4], float *a, magma_int_t lda,
                   cudaStream_t stream[][2], magma_int_t *info)
#else
extern "C" magma_int_t
magma_sgetrf2_mgpu(magma_int_t num_gpus, 
         magma_int_t m, magma_int_t n, magma_int_t nb, magma_int_t offset,
         float **d_lAT, magma_int_t lddat, magma_int_t *ipiv,
         float **d_lAP, float *w, magma_int_t ldw,
         cudaStream_t streaml[][2], magma_int_t *info)
#endif
{
/*  -- MAGMA (version 1.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2010

    Purpose
    =======

    SGETRF computes an LU factorization of a general M-by-N matrix A
    using partial pivoting with row interchanges.

    The factorization has the form
       A = P * L * U
    where P is a permutation matrix, L is lower triangular with unit
    diagonal elements (lower trapezoidal if m > n), and U is upper
    triangular (upper trapezoidal if m < n).

    This is the right-looking Level 3 BLAS version of the algorithm.
    Use two buffer to send panels..

    Arguments
    =========

    NUM_GPUS 
            (input) INTEGER
            The number of GPUS to be used for the factorization.

    M       (input) INTEGER
            The number of rows of the matrix A.  M >= 0.

    N       (input) INTEGER
            The number of columns of the matrix A.  N >= 0.

    A       (input/output) REAL array on the GPU, dimension (LDDA,N).
            On entry, the M-by-N matrix to be factored.
            On exit, the factors L and U from the factorization
            A = P*L*U; the unit diagonal elements of L are not stored.

    LDDA     (input) INTEGER
            The leading dimension of the array A.  LDDA >= max(1,M).

    IPIV    (output) INTEGER array, dimension (min(M,N))
            The pivot indices; for 1 <= i <= min(M,N), row i of the
            matrix was interchanged with row IPIV(i).

    INFO    (output) INTEGER
            = 0:  successful exit
            < 0:  if INFO = -i, the i-th argument had an illegal value
                  if INFO = -7, internal GPU memory allocation failed.
            > 0:  if INFO = i, U(i,i) is exactly zero. The factorization
                  has been completed, but the factor U is exactly
                  singular, and division by zero will occur if it is used
                  to solve a system of equations.
    =====================================================================    */

#define inAT(id,i,j)  (d_lAT[(id)] + ((offset)+(i)*nb)*lddat + (j)*nb)
#define W(j) (w+((j)%num_gpus)*nb*ldw)

    float c_one     = MAGMA_S_ONE;
    float c_neg_one = MAGMA_S_NEG_ONE;

    magma_int_t block_size = 32;
    magma_int_t iinfo, n_local[4]; 
    magma_int_t maxm, mindim;
    magma_int_t i, ii, d, dd, rows, cols, s, ldpan[4];
    magma_int_t id, i_local, i_local2, nb0, nb1;
    float *d_panel[4], *panel_local[4];
    //cudaStream_t streaml[4][2];

    /* Check arguments */
    *info = 0;
    if (m < 0)
    *info = -2;
    else if (n < 0)
    *info = -3;
    else if (num_gpus*lddat < max(1,n))
    *info = -5;

    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }

    /* Quick return if possible */
    if (m == 0 || n == 0)
        return *info;

    /* Function Body */
    mindim = min(m, n);
    //nb     = magma_get_sgetrf_nb(m);
    if( num_gpus > ceil((float)n/nb) ) {
      *info = -1;
      return *info;
    }

    {
      /* Use hybrid blocked code. */
      maxm  = ((m + block_size-1)/block_size)*block_size;

      /* some initializations */
      for(i=0; i<num_gpus; i++){
        magmaSetDevice(i);

        n_local[i] = ((n/nb)/num_gpus)*nb;
        if (i < (n/nb)%num_gpus)
           n_local[i] += nb;
        else if (i == (n/nb)%num_gpus)
           n_local[i] += n%nb;

        /* workspaces */
        d_panel[i] = &(d_lAP[i][nb*maxm]);   /* temporary panel storage */

        /* create local streams */
        //magma_queue_create(&streaml[i][0]);
        //magma_queue_create(&streaml[i][1]);
      }
      trace_init( 1, num_gpus, 2, (CUstream_st**)streaml );

      /* start sending the panel to cpu */
      nb0 = min(mindim, nb);
      magmaSetDevice(0);
      magmablasSetKernelStream(streaml[0][1]);
      trace_gpu_start( 0, 1, "comm", "get" );
      if( nb0 == nb )
        magmablas_stranspose(  d_lAP[0], maxm, inAT(0,0,0), lddat, nb0, maxm );
      else
        magmablas_stranspose2( d_lAP[0], maxm, inAT(0,0,0), lddat, nb0, maxm );
      magma_sgetmatrix_async( m, nb0,
                              d_lAP[0], maxm,
                              W(0),     ldw, streaml[0][1] );
      trace_gpu_end( 0, 1 );

      /* ------------------------------------------------------------------------------------- */
#ifdef  PROFILE
      magma_timestr_t start_timer, end_timer;
      start_timer = get_current_time();
#endif
      s = mindim / nb;
      for( i=0; i<s; i++ )
            {
                /* Set the GPU number that holds the current panel */
                id = i%num_gpus;
                magmaSetDevice(id);

                /* Set the local index where the current panel is */
                i_local = i/num_gpus;
                cols  = maxm - i*nb;
                rows  = m - i*nb;

                /* synchrnoize i-th panel from id-th gpu into work */
                magma_queue_sync( streaml[id][1] );

                /* i-th panel factorization */
                trace_cpu_start( 0, "getrf", "getrf" );
#ifdef PANEL_FACT_MC
                cntxt->nb = 12;
                magma_sgetrf_mc(cntxt, &rows, &nb, W(i), &ldw, ipiv+i*nb, &iinfo);
#else
                lapackf77_sgetrf( &rows, &nb, W(i), &ldw, ipiv+i*nb, &iinfo);
#endif
                if ( (*info == 0) && (iinfo > 0) ) {
                    *info = iinfo + i*nb;
                    //break;
                }
                trace_cpu_end( 0 );

                /* start sending the panel to all the gpus */
                d = (i+1)%num_gpus;
                for( dd=0; dd<num_gpus; dd++ ) {
                  magmaSetDevice(d);
                  trace_gpu_start( 0, 1, "comm", "set" );
                  magma_ssetmatrix_async( rows, nb,
                                          W(i),     ldw,
                                          d_lAP[d], cols, streaml[d][1] );
                  trace_gpu_end( 0, 1 );
                  d = (d+1)%num_gpus;
                }

                /* apply the pivoting */
                d = (i+1)%num_gpus;
                for( dd=0; dd<num_gpus; dd++ ) {
                  magmaSetDevice(d);
                  magmablasSetKernelStream(streaml[d][0]);

                  trace_gpu_start( d, 1, "pivot", "pivot" );
                  if( dd == 0 )
                    magmablas_spermute_long2( lddat, inAT(d,0,0), lddat, ipiv, nb, i*nb );
                  else
                    magmablas_spermute_long3(        inAT(d,0,0), lddat, ipiv, nb, i*nb );
                  trace_gpu_end( d, 1 );
                  d = (d+1)%num_gpus;
                }


                /* update the trailing-matrix/look-ahead */
                d = (i+1)%num_gpus;
                for( dd=0; dd<num_gpus; dd++ ) {
                  magmaSetDevice(d);

                  /* storage for panel */
                  if( d == id ) {
                    /* the panel belond to this gpu */
                    panel_local[d] = inAT(d,i,i_local);
                    ldpan[d] = lddat;
                    /* next column */
                    i_local2 = i_local+1;
                  } else {
                    /* the panel belong to another gpu */
                    panel_local[d] = &d_panel[d][(i%2)*nb*maxm];
                    //panel_local[d] = d_panel[d];
                    ldpan[d] = nb;
                    /* next column */
                    i_local2 = i_local;
                    if( d < id ) i_local2 ++;
                  }
                  /* the size of the next column */
                  if ( s > (i+1) ) {
                    nb0 = nb;
                  } else {
                    nb0 = n_local[d]-nb*(s/num_gpus);
                    if( d < s%num_gpus ) nb0 -= nb;
                  }
                  if( d == (i+1)%num_gpus) {
                    /* owns the next column, look-ahead the column */
                    nb1 = nb0;
                    magmablasSetKernelStream(streaml[d][1]);

                    /* make sure all the pivoting has been applied */
                    magma_queue_sync(streaml[d][0]);
                    trace_gpu_start( d, 1, "gemm", "gemm" );
                  } else {
                    /* update the entire trailing matrix */
                    nb1 = n_local[d] - i_local2*nb;
                    magmablasSetKernelStream(streaml[d][0]);

                    /* synchronization to make sure panel arrived on gpu */
                    magma_queue_sync(streaml[d][1]);
                    trace_gpu_start( d, 0, "gemm", "gemm" );
                  }
                  magmablas_stranspose(panel_local[d], ldpan[d], d_lAP[d], cols, cols, nb);

                  /* gpu updating the trailing matrix */
                  //magmablas_strsm(
                  magma_strsm( MagmaRight, MagmaUpper, MagmaNoTrans, MagmaUnit, 
                               nb1, nb, c_one,
                               panel_local[d],       ldpan[d],
                               inAT(d, i, i_local2), lddat);
                  //cublasSgemm
                  magma_sgemm( MagmaNoTrans, MagmaNoTrans, 
                               nb1, m-(i+1)*nb, nb, 
                               c_neg_one, inAT(d, i,   i_local2),         lddat,
                                          &(panel_local[d][nb*ldpan[d]]), ldpan[d], 
                               c_one,     inAT(d, i+1, i_local2),         lddat );

                  if( d == (i+1)%num_gpus ) 
                  {
                    /* Set the local index where the current panel is */
                    int loff    = i+1;
                    int i_local = (i+1)/num_gpus;
                    int ldda    = maxm - (i+1)*nb;
                    int cols    = m - (i+1)*nb;
                    nb0 = min(nb, mindim - (i+1)*nb); /* size of the diagonal block */
                    trace_gpu_end( d, 1 );

                    if( nb0 > 0 ) {
                        /* transpose the panel for sending it to cpu */
                        trace_gpu_start( d, 1, "comm", "get" );
                        if( i+1 < s ) 
                          magmablas_stranspose(  d_lAP[d], ldda, inAT(d,loff,i_local), lddat, nb0, ldda );
                        else
                          magmablas_stranspose2(  d_lAP[d], ldda, inAT(d,loff,i_local), lddat, nb0, ldda );

                        /* send the panel to cpu */
                        magma_sgetmatrix_async( cols, nb0,
                                                d_lAP[d], ldda,
                                                W(i+1),   ldw, streaml[d][1] );
                        trace_gpu_end( d, 1 );
                    }
                  } else {
                    trace_gpu_end( d, 0 );
                  }

                  d = (d+1)%num_gpus;
                }

                /* update the remaining matrix by gpu owning the next panel */
                if( (i+1) < s ) {
                  int i_local = (i+1)/num_gpus;
                  int rows  = m - (i+1)*nb;

                  d = (i+1)%num_gpus;
                  magmaSetDevice(d);
                  magmablasSetKernelStream(streaml[d][0]);
                  trace_gpu_start( d, 0, "gemm", "gemm" );
                  //magmablas_strsm
                  magma_strsm( MagmaRight, MagmaUpper, MagmaNoTrans, MagmaUnit, 
                               n_local[d] - (i_local+1)*nb, nb, 
                               c_one, panel_local[d],       ldpan[d], 
                                      inAT(d,i,i_local+1),  lddat );
                  //cublasSgemm
                  magma_sgemm( MagmaNoTrans, MagmaNoTrans, 
                               n_local[d]-(i_local+1)*nb, rows, nb, 
                               c_neg_one, inAT(d,i,i_local+1),            lddat, 
                                          &(panel_local[d][nb*ldpan[d]]), ldpan[d], 
                               c_one,     inAT(d,i+1,  i_local+1),        lddat );
                  trace_gpu_end( d, 0 );
                }
             } /* end of for i=1..s */
             /* ------------------------------------------------------------------------------ */

            /* Set the GPU number that holds the last panel */
            id = s%num_gpus;

            /* Set the local index where the last panel is */
            i_local = s/num_gpus;

            /* size of the last diagonal-block */
            nb0 = min(m - s*nb, n - s*nb);
            rows = m    - s*nb;
            cols = maxm - s*nb;

            if( nb0 > 0 ) {
              magmaSetDevice(id);

              /* wait for the last panel on cpu */
              magma_queue_sync( streaml[id][1] );

              /* factor on cpu */
              lapackf77_sgetrf( &rows, &nb0, W(s), &ldw, ipiv+s*nb, &iinfo);
              if ( (*info == 0) && (iinfo > 0) )
              *info = iinfo + s*nb;

              /* send the factor to gpus */
              for( d=0; d<num_gpus; d++ ) {
                magmaSetDevice(d);
                i_local2 = i_local;
                if( d < id ) i_local2 ++;

                if( d == id || n_local[d] > i_local2*nb ) {
                  magma_ssetmatrix_async( rows, nb0,
                                          W(s),     ldw,
                                          d_lAP[d], cols, streaml[d][1] );
                }
              }

              for( d=0; d<num_gpus; d++ ) {
                magmaSetDevice(d);
                magmablasSetKernelStream(streaml[d][0]);
                if( d == 0 )
                  magmablas_spermute_long2( lddat, inAT(d,0,0), lddat, ipiv, nb0, s*nb );
                else
                  magmablas_spermute_long3(        inAT(d,0,0), lddat, ipiv, nb0, s*nb );
              }

              for( d=0; d<num_gpus; d++ ) {
                magmaSetDevice(d);
                magmablasSetKernelStream(streaml[d][1]);

                /* wait for the pivoting to be done */
                magma_queue_sync( streaml[d][0] );

                i_local2 = i_local;
                if( d < id ) i_local2++;
                if( d == id ) {
                  /* the panel belond to this gpu */
                  panel_local[d] = inAT(d,s,i_local);

                  /* next column */
                  nb1 = n_local[d] - i_local*nb-nb0;

                  magmablas_stranspose2( panel_local[d], lddat, d_lAP[d], cols, rows, nb0);

                  if( nb1 > 0 )
                  //cublasStrsm
                  magma_strsm( MagmaRight, MagmaUpper, MagmaNoTrans, MagmaUnit, 
                               nb1, nb0, c_one,
                               panel_local[d],        lddat, 
                               inAT(d,s,i_local)+nb0, lddat);
                } else if( n_local[d] > i_local2*nb ) {
                  /* the panel belong to another gpu */
                  panel_local[d] = &d_panel[d][(s%2)*nb*maxm];
                  //panel_local[d] = d_panel[d];

                  /* next column */
                  nb1 = n_local[d] - i_local2*nb;

                  magmablas_stranspose2( panel_local[d], nb, d_lAP[d], cols, rows, nb0);
                  //cublasStrsm
                  magma_strsm( MagmaRight, MagmaUpper, MagmaNoTrans, MagmaUnit, 
                               nb1, nb0, c_one,
                               panel_local[d],     nb, 
                               inAT(d,s,i_local2), lddat);
                }
              }
            } /* if( nb0 > 0 ) */

            /* clean up */
            trace_finalize( "sgetrf_mgpu.svg","trace.css" );
            for( d=0; d<num_gpus; d++ ) {
              magmaSetDevice(d);
              magma_queue_sync( streaml[d][0] );
              magma_queue_sync( streaml[d][1] );
              //magma_queue_destroy(streaml[d][0]);
              //magma_queue_destroy(streaml[d][1]);
              magmablasSetKernelStream(NULL);
            } 
            magmaSetDevice(0);
#ifdef  PROFILE
            end_timer = get_current_time();
            printf("\n Performance %f GFlop/s\n", (2./3.*n*n*n /1000000.) / GetTimerValue(start_timer, end_timer));
#endif
    }

    return *info;
    /* End of MAGMA_SGETRF2_MGPU */
}

#undef inAT
