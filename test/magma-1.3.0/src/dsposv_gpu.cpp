/*
    -- MAGMA (version 1.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2012

       @generated ds Wed Nov 14 22:52:56 2012

*/
#include "common_magma.h"

#define ITERMAX 30
#define BWDMAX 1.0

// === Define what BLAS to use ============================================
#define magma_dsymv magmablas_dsymv
// === End defining what BLAS to use ======================================

extern "C" magma_int_t
magma_dsposv_gpu(char uplo, magma_int_t n, magma_int_t nrhs, 
                 double *dA, magma_int_t ldda, 
                 double *dB, magma_int_t lddb, 
                 double *dX, magma_int_t lddx, 
                 double *dworkd, float *dworks,
                 magma_int_t *iter, magma_int_t *info)
{
/*  -- MAGMA (version 1.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2012

    Purpose
    =======

    DSPOSV computes the solution to a real system of linear equations
       A * X = B,
    where A is an N-by-N symmetric positive definite matrix and X and B
    are N-by-NRHS matrices.

    DSPOSV first attempts to factorize the matrix in real SINGLE PRECISION
    and use this factorization within an iterative refinement procedure
    to produce a solution with real DOUBLE PRECISION norm-wise backward error
    quality (see below). If the approach fails the method switches to a
    real DOUBLE PRECISION factorization and solve.

    The iterative refinement is not going to be a winning strategy if
    the ratio real SINGLE PRECISION performance over DOUBLE PRECISION
    performance is too small. A reasonable strategy should take the
    number of right-hand sides and the size of the matrix into account.
    This might be done with a call to ILAENV in the future. Up to now, we
    always try iterative refinement.

    The iterative refinement process is stopped if
        ITER > ITERMAX
    or for all the RHS we have:
        RNRM < SQRT(N)*XNRM*ANRM*EPS*BWDMAX
    where
        o ITER is the number of the current iteration in the iterative
          refinement process
        o RNRM is the infinity-norm of the residual
        o XNRM is the infinity-norm of the solution
        o ANRM is the infinity-operator-norm of the matrix A
        o EPS is the machine epsilon returned by DLAMCH('Epsilon')
    The value ITERMAX and BWDMAX are fixed to 30 and 1.0D+00 respectively.

    Arguments
    =========

    UPLO    (input) CHARACTER
            = 'U':  Upper triangle of A is stored;
            = 'L':  Lower triangle of A is stored.

    N       (input) INTEGER
            The number of linear equations, i.e., the order of the
            matrix A.  N >= 0.

    NRHS    (input) INTEGER
            The number of right hand sides, i.e., the number of columns
            of the matrix B.  NRHS >= 0.

    A       (input or input/output) DOUBLE PRECISION array, dimension (LDA,N)
            On entry, the symmetric matrix A.  If UPLO = 'U', the leading
            N-by-N upper triangular part of A contains the upper
            triangular part of the matrix A, and the strictly lower
            triangular part of A is not referenced.  If UPLO = 'L', the
            leading N-by-N lower triangular part of A contains the lower
            triangular part of the matrix A, and the strictly upper
            triangular part of A is not referenced.
            On exit, if iterative refinement has been successfully used
            (INFO.EQ.0 and ITER.GE.0, see description below), then A is
            unchanged, if double factorization has been used
            (INFO.EQ.0 and ITER.LT.0, see description below), then the
            array A contains the factor U or L from the Cholesky
            factorization A = U**T*U or A = L*L**T.

    LDA     (input) INTEGER
            The leading dimension of the array A.  LDA >= max(1,N).

    B       (input) DOUBLE PRECISION array, dimension (LDB,NRHS)
            The N-by-NRHS right hand side matrix B.

    LDB     (input) INTEGER
            The leading dimension of the array B.  LDB >= max(1,N).

    X       (output) DOUBLE PRECISION array, dimension (LDX,NRHS)
            If INFO = 0, the N-by-NRHS solution matrix X.

    LDX     (input) INTEGER
            The leading dimension of the array X.  LDX >= max(1,N).

    dworkd  (workspace) DOUBLE PRECISION array, dimension (N*NRHS)
            This array is used to hold the residual vectors.

    dworks  (workspace) SINGLE PRECISION array, dimension (N*(N+NRHS))
            This array is used to use the real single precision matrix 
            and the right-hand sides or solutions in single precision.

    ITER    (output) INTEGER
            < 0: iterative refinement has failed, double precision
                 factorization has been performed
                 -1 : the routine fell back to full precision for
                      implementation- or machine-specific reasons
                 -2 : narrowing the precision induced an overflow,
                      the routine fell back to full precision
                 -3 : failure of SPOTRF
                 -31: stop the iterative refinement after the 30th
                      iterations
            > 0: iterative refinement has been successfully used.
                 Returns the number of iterations

    INFO    (output) INTEGER
            = 0:  successful exit
            < 0:  if INFO = -i, the i-th argument had an illegal value
            > 0:  if INFO = i, the leading minor of order i of (DOUBLE
                  PRECISION) A is not positive definite, so the
                  factorization could not be completed, and the solution
                  has not been computed.

    =====================================================================    */


    double c_neg_one = MAGMA_D_NEG_ONE;
    double c_one     = MAGMA_D_ONE;
    magma_int_t     ione  = 1;
    float *dSA, *dSX;
    double Xnrmv, Rnrmv; 
    double          Xnrm, Rnrm, Anrm, cte, eps;
    magma_int_t     i, j, iiter;

    *iter = 0 ;
    *info = 0 ; 

    if ( n <0)
        *info = -1;
    else if( nrhs<0 )
        *info =-2;
    else if( ldda < max(1,n) )
        *info =-4;
    else if( lddb < max(1,n) )
        *info =-7;
    else if( lddx < max(1,n) )
        *info =-9;
   
    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }

    if( n == 0 || nrhs == 0 ) 
        return *info;

    eps = lapackf77_dlamch("Epsilon");

    //ANRM = magmablas_dlansy(  'I',  uplo , N ,A, LDA, (double*)dworkd);
    //cte  = ANRM * EPS *  pow((double)N,0.5) * BWDMAX ;  

    dSX = dworks;
    dSA = dworks + n*nrhs;
 
    magmablas_dlag2s(n, nrhs, dB, lddb, dSX, n, info );
    if( *info !=0 ){
        *iter = -2;
        goto L40;
    } 
  
    magmablas_dlat2s(uplo, n, dA, ldda, dSA, n, info ); 
    if( *info !=0 ){
        *iter = -2 ;
        goto L40;
    }
 
    Anrm = magmablas_slansy( 'I', uplo, n, dSA, n, (float *)dworkd);
    cte  = Anrm * eps * pow((double)n,0.5) * BWDMAX ;

    magma_spotrf_gpu(uplo, n, dSA, ldda, info);
    if( *info !=0 ){
        *iter = -3 ;
        goto L40;
    }
    magma_spotrs_gpu(uplo, n, nrhs, dSA, ldda, dSX, lddb, info);
    magmablas_slag2d(n, nrhs, dSX, n, dX, lddx, info);
  
    magmablas_dlacpy( MagmaUpperLower, n, nrhs, dB, lddb, dworkd, n);
    
    if( nrhs == 1 )
        magma_dsymv(uplo, n, c_neg_one, dA, ldda, dX, 1, c_one, dworkd, 1);
    else
        magma_dsymm(MagmaLeft, uplo, n, nrhs, c_neg_one, dA, ldda, dX, lddx, c_one, dworkd, n);
  
    for(i=0; i<nrhs; i++){
        j = magma_idamax( n, dX+i*n, 1) ;
        magma_dgetmatrix( 1, 1, dX+i*n+j-1, 1, &Xnrmv, 1 );
        Xnrm = lapackf77_dlange( "F", &ione, &ione, &Xnrmv, &ione, NULL );
      
        j = magma_idamax ( n, dworkd+i*n, 1 );
        magma_dgetmatrix( 1, 1, dworkd+i*n+j-1, 1, &Rnrmv, 1 );
        Rnrm = lapackf77_dlange( "F", &ione, &ione, &Rnrmv, &ione, NULL );
      
        if( Rnrm >  Xnrm *cte ){
            goto L10;
        }
    }
    *iter =0; 
    return *info;
  
  L10:
    ;

    for(iiter=1;iiter<ITERMAX;){
        *info = 0 ; 
        magmablas_dlag2s(n, nrhs, dworkd, n, dSX, n, info );
        if(*info !=0){
            *iter = -2 ;
            goto L40;
        } 
        magma_spotrs_gpu(uplo, n, nrhs, dSA, ldda, dSX, lddb, info);
      
        for(i=0;i<nrhs;i++){
            magmablas_dsaxpycp(dworks+i*n, dX+i*n, n, dB+i*n,dworkd+i*n) ;
        }
      
        if( nrhs == 1 )
            magma_dsymv(uplo, n, c_neg_one, dA, ldda, dX, 1, c_one, dworkd, 1);
        else 
            magma_dsymm(MagmaLeft, uplo, n, nrhs, c_neg_one, dA, ldda, dX, lddx, c_one, dworkd, n);
      
        for(i=0; i<nrhs; i++){
            j = magma_idamax( n, dX+i*n, 1) ;
            magma_dgetmatrix( 1, 1, dX+i*n+j-1, 1, &Xnrmv, 1 );
            Xnrm = lapackf77_dlange( "F", &ione, &ione, &Xnrmv, &ione, NULL );
          
            j = magma_idamax ( n, dworkd+i*n, 1 );
            magma_dgetmatrix( 1, 1, dworkd+i*n+j-1, 1, &Rnrmv, 1 );
            Rnrm = lapackf77_dlange( "F", &ione, &ione, &Rnrmv, &ione, NULL );
          
            if( Rnrm >  Xnrm *cte ){
                goto L20;
            }
        }  

        *iter = iiter;
        return *info;
      L20:
        iiter++ ;
    }
    *iter = -ITERMAX - 1; 
  
  L40:
    magma_dpotrf_gpu( uplo, n, dA, ldda, info );
    if( *info == 0 ){
        magmablas_dlacpy( MagmaUpperLower, n, nrhs, dB, lddb, dX, lddx );
        magma_dpotrs_gpu( uplo, n, nrhs, dA, ldda, dX, lddx, info );
    }
    return *info;
}
