/*
    -- MAGMA (version 1.7.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date September 2015

       @generated from testing_zhetrf.cpp normal z -> d, Fri Sep 11 18:29:39 2015
       @author Ichitaro Yamazaki
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
#include "testings.h"
#include "magma_operators.h"  // for MAGMA_D_DIV

/* ================================================================================================== */

// Initialize matrix to random & symmetrize. If nopiv, make positive definite.
// Having this in separate function ensures the same ISEED is always used,
// so we can re-generate the identical matrix.
void init_matrix( int nopiv, int m, int n, double *h_A, magma_int_t lda )
{
    assert( m == n );
    magma_int_t ione = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    magma_int_t n2 = lda*n;
    lapackf77_dlarnv( &ione, ISEED, &n2, h_A );
    if (nopiv) {
        magma_dmake_hpd( n, h_A, lda );
    }
    else {
        magma_dmake_symmetric( n, h_A, lda );
    }
}


// On input, A and ipiv is LU factorization of A. On output, A is overwritten.
// Requires m == n.
// Uses init_matrix() to re-generate original A as needed.
// Generates random RHS b and solves Ax=b.
// Returns residual, |Ax - b| / (n |A| |x|).
double get_residual(
    int nopiv, magma_uplo_t uplo, magma_int_t n,
    double *A, magma_int_t lda,
    magma_int_t *ipiv )
{
    const double c_one     = MAGMA_D_ONE;
    const double c_neg_one = MAGMA_D_NEG_ONE;
    const magma_int_t ione = 1;
    magma_int_t upper = (uplo == MagmaUpper);
    
    // this seed should be DIFFERENT than used in init_matrix
    // (else x is column of A, so residual can be exactly zero)
    magma_int_t ISEED[4] = {0,0,0,2};
    magma_int_t info = 0;
    double *x, *b;
    
    // initialize RHS
    TESTING_MALLOC_CPU( x, double, n );
    TESTING_MALLOC_CPU( b, double, n );
    lapackf77_dlarnv( &ione, ISEED, &n, b );
    blasf77_dcopy( &n, b, &ione, x, &ione );
    
    // solve Ax = b
    if (nopiv) {
        if (upper) {
            blasf77_dtrsm( MagmaLeftStr, MagmaUpperStr,
                           MagmaConjTransStr, MagmaUnitStr,
                           &n, &ione, &c_one,
                           A, &lda, x, &n );
            for (int i=0; i < n; i++) {
                x[i] = MAGMA_D_DIV( x[i], A[i+i*lda] );
            }
            blasf77_dtrsm( MagmaLeftStr, MagmaUpperStr,
                           MagmaNoTransStr, MagmaUnitStr,
                           &n, &ione, &c_one,
                           A, &lda, x, &n );
        } else {
            blasf77_dtrsm( MagmaLeftStr, MagmaLowerStr,
                           MagmaNoTransStr, MagmaUnitStr,
                           &n, &ione, &c_one,
                           A, &lda, x, &n );
            for (int i=0; i < n; i++) {
                x[i] = MAGMA_D_DIV( x[i], A[i+i*lda] );
            }
            blasf77_dtrsm( MagmaLeftStr, MagmaLowerStr,
                           MagmaConjTransStr, MagmaUnitStr,
                           &n, &ione, &c_one,
                           A, &lda, x, &n );
        }
    }
    else {
        lapackf77_dsytrs( lapack_uplo_const(uplo), &n, &ione, A, &lda, ipiv, x, &n, &info );
    }
    if (info != 0)
        printf("lapackf77_dsytrs returned error %d: %s.\n",
               (int) info, magma_strerror( info ));
    // reset to original A
    init_matrix( nopiv, n, n, A, lda );
    
    // compute r = Ax - b, saved in b
    blasf77_dgemv( "Notrans", &n, &n, &c_one, A, &lda, x, &ione, &c_neg_one, b, &ione );
    
    // compute residual |Ax - b| / (n*|A|*|x|)
    double norm_x, norm_A, norm_r, work[1];
    norm_A = lapackf77_dlange( MagmaFullStr, &n, &n, A, &lda, work );
    norm_r = lapackf77_dlange( MagmaFullStr, &n, &ione, b, &n, work );
    norm_x = lapackf77_dlange( MagmaFullStr, &n, &ione, x, &n, work );
    
    //printf( "r=\n" ); magma_dprint( 1, n, b, 1 );
    
    TESTING_FREE_CPU( x );
    TESTING_FREE_CPU( b );
    
    //printf( "r=%.2e, A=%.2e, x=%.2e, n=%d\n", norm_r, norm_A, norm_x, n );
    return norm_r / (n * norm_A * norm_x);
}

double get_residual_aasen(
    int nopiv, magma_uplo_t uplo, magma_int_t n,
    double *A, magma_int_t lda,
    magma_int_t *ipiv )
{
    magma_int_t ione = 1;
    double c_one  = MAGMA_D_ONE;
    double c_mone = MAGMA_D_NEG_ONE;
    double *L, *T;
    #define  A(i,j) ( A[(i) + (j)*lda])
    #define  L(i,j) ( L[(i) + (j)*n])
    #define  T(i,j) ( T[(i) + (j)*n])
    TESTING_MALLOC_CPU( L, double, n*n );
    TESTING_MALLOC_CPU( T, double, n*n );
    memset( L, 0, n*n*sizeof(double) );
    memset( T, 0, n*n*sizeof(double) );

    int nb = magma_get_dsytrf_aasen_nb(n);
    // extract T
    for (int i=0; i<n; i++)
    {
        int istart = max(0, i-nb);
        for (int j=istart; j<=i; j++) {
            T(i,j) = A(i,j);
        }
        for (int j=istart; j<i;  j++) {
            T(j,i) = MAGMA_D_CNJG(A(i,j));
        }
    }
    // extract L
    for (int i=0; i<min(n,nb); i++) 
    {
        L(i,i) = c_one;
    }
    for (int i=nb; i<n; i++)
    {
        for (int j=0; j<i-nb; j++) {
            L(i,nb+j) = A(i,j);
        }
        L(i,i) = c_one;
    }

    // solve
    magma_int_t ISEED[4] = {0,0,0,2};
    magma_int_t info = 0;
    double *x, *b;
    
    // initialize RHS
    TESTING_MALLOC_CPU( x, double, n );
    TESTING_MALLOC_CPU( b, double, n );
    lapackf77_dlarnv( &ione, ISEED, &n, b );
    blasf77_dcopy( &n, b, &ione, x, &ione );
    // pivot..
    for (int i=0; i<n; i++) {
        int piv = ipiv[i]-1;
        double val = x[i];
        x[i] = x[piv];
        x[piv] = val;
    }
    // forward solve
    blasf77_dtrsv( MagmaLowerStr, MagmaNoTransStr, MagmaUnitStr, &n, &L(0,0),&n, x,&ione);
    // banded solver
    magma_int_t nrhs = 1, *p = NULL;
    if (MAGMA_SUCCESS != magma_imalloc_cpu(&p, n)) {
        printf( " failed to allocate perm\n" );
        return 0;
    }
    lapackf77_dgesv(&n,&nrhs, &T(0,0),&n, p,x,&n, &info);
    magma_free_cpu(p);
    // backward solve
    blasf77_dtrsv( MagmaLowerStr, MagmaConjTransStr, MagmaUnitStr, &n, &L(0,0),&n, x,&ione);
    // pivot..
    for (int i=n-1; i>=0; i--) {
        int piv = ipiv[i]-1;
        double val = x[i];
        x[i] = x[piv];
        x[piv] = val;
    }

    // reset to original A
    init_matrix( nopiv, n, n, A, lda );

    // compute r = Ax - b, saved in b
    blasf77_dgemv( "Notrans", &n, &n, &c_one, A, &lda, x, &ione, &c_mone, b, &ione );
    
    // compute residual |Ax - b| / (n*|A|*|x|)
    double norm_x, norm_A, norm_r, work[1];
    norm_A = lapackf77_dlange( MagmaFullStr, &n, &n, A, &lda, work );
    norm_r = lapackf77_dlange( MagmaFullStr, &n, &ione, b, &n, work );
    norm_x = lapackf77_dlange( MagmaFullStr, &n, &ione, x, &n, work );
    
    //printf( "r=\n" ); magma_dprint( 1, n, b, 1 );
    TESTING_FREE_CPU( L );
    TESTING_FREE_CPU( T );
    
    TESTING_FREE_CPU( x );
    TESTING_FREE_CPU( b );
    
    #undef T
    #undef L
    #undef A
    //printf( "r=%.2e, A=%.2e, x=%.2e, n=%d\n", norm_r, norm_A, norm_x, n );
    return norm_r / (n * norm_A * norm_x);
}

// On input, LU and ipiv is LU factorization of A. On output, LU is overwritten.
// Works for any m, n.
// Uses init_matrix() to re-generate original A as needed.
// Returns error in factorization, |PA - LU| / (n |A|)
// This allocates 3 more matrices to store A, L, and U.
double get_LDLt_error(int nopiv, magma_uplo_t uplo, magma_int_t N,
                      double *LD, magma_int_t lda,
                      magma_int_t *ipiv)
{
    magma_int_t i, j;
    double c_one  = MAGMA_D_ONE;
    double c_zero = MAGMA_D_ZERO;
    double *A, *L, *D;
    double work[1], matnorm, residual;
    #define LD(i,j) (LD[(i) + (j)*lda])
    #define  A(i,j) ( A[(i) + (j)*N])
    #define  L(i,j) ( L[(i) + (j)*N])
    #define  D(i,j) ( D[(i) + (j)*N])

    TESTING_MALLOC_CPU( A, double, N*N );
    TESTING_MALLOC_CPU( L, double, N*N );
    TESTING_MALLOC_CPU( D, double, N*N );
    memset( L, 0, N*N*sizeof(double) );
    memset( D, 0, N*N*sizeof(double) );

    // set to original A, and apply pivoting
    init_matrix( nopiv, N, N, A, N );
    if (uplo == MagmaUpper) {
        for (j=N-1; j >= 0; j--) {
            int piv = (nopiv ? j+1 : ipiv[j]);
            if (piv < 0) {
                piv = -(piv+1);
                // extract 2-by-2 pivot
                D(j,j)     = LD(j,j);
                D(j,j-1)   = MAGMA_D_CNJG(LD(j-1,j));
                D(j-1,j)   = LD(j-1,j);
                D(j-1,j-1) = LD(j-1,j-1);
                // exract L
                L(j,j) = c_one;
                for (i=0; i < j-1; i++) {
                    L(i,j) = LD(i,j);
                }
                j--;
                L(j,j) = c_one;
                for (i=0; i < j; i++) {
                    L(i,j) = LD(i,j);
                }
                if (piv != j) {
                    // apply row-pivoting to previous L
                    for (i=j+2; i < N; i++) {
                        double val = L(j,i);
                        L(j,i) = L(piv,i);
                        L(piv,i) = val;
                    }
                    // apply row-pivoting to A
                    for (i=0; i < N; i++) {
                        double val = A(j,i);
                        A(j,i) = A(piv,i);
                        A(piv,i) = val;
                    }
                    // apply col-pivoting to A
                    for (i=0; i < N; i++) {
                        double val = A(i,j);
                        A(i,j) = A(i,piv);
                        A(i,piv) = val;
                    }
                }
            } else {
                piv = piv-1;
                // extract 1-by-1 pivot
                D(j,j) = LD(j,j);
                // exract L
                L(j,j) = c_one;
                for (i=0; i < j; i++) {
                    L(i,j) = LD(i,j);
                }
                if (piv != j) {
                    // apply row-pivoting to previous L
                    for (i=j+1; i < N; i++) {
                        double val = L(j,i);
                        L(j,i) = L(piv,i);
                        L(piv,i) = val;
                    }
                    // apply row-pivoting to A
                    for (i=0; i < N; i++) {
                        double val = A(j,i);
                        A(j,i) = A(piv,i);
                        A(piv,i) = val;
                    }
                    // apply col-pivoting to A
                    for (i=0; i < N; i++) {
                        double val = A(i,j);
                        A(i,j) = A(i,piv);
                        A(i,piv) = val;
                    }
                }
            }
        }
        if (nopiv) {
            // compute W = D*U
            blasf77_dgemm(MagmaNoTransStr, MagmaNoTransStr, &N, &N, &N,
                          &c_one, D, &N, L, &N, &c_zero, LD, &lda);
            // compute D = U'*W
            blasf77_dgemm(MagmaConjTransStr, MagmaNoTransStr, &N, &N, &N,
                          &c_one, L, &N, LD, &lda, &c_zero, D, &N);
        } else {
            // compute W = U*D
            blasf77_dgemm(MagmaNoTransStr, MagmaNoTransStr, &N, &N, &N,
                          &c_one, L, &N, D, &N, &c_zero, LD, &lda);
            // compute D = W*U'
            blasf77_dgemm(MagmaNoTransStr, MagmaConjTransStr, &N, &N, &N,
                          &c_one, LD, &lda, L, &N, &c_zero, D, &N);
        }
    } else {
        for (j=0; j < N; j++) {
            int piv = (nopiv ? j+1 : ipiv[j]);
            if (piv < 0) {
                piv = -(piv+1);
                // extract 2-by-2 pivot
                D(j,j)     = LD(j,j);
                D(j,j+1)   = MAGMA_D_CNJG(LD(j+1,j));
                D(j+1,j)   = LD(j+1,j);
                D(j+1,j+1) = LD(j+1,j+1);
                // exract L
                L(j,j) = c_one;
                for (i=j+2; i < N; i++) {
                    L(i,j) = LD(i,j);
                }
                j++;
                L(j,j) = c_one;
                for (i=j+1; i < N; i++) {
                    L(i,j) = LD(i,j);
                }
                if (piv != j) {
                    // apply row-pivoting to previous L
                    for (i=0; i < j-1; i++) {
                        double val = L(j,i);
                        L(j,i) = L(piv,i);
                        L(piv,i) = val;
                    }
                    // apply row-pivoting to A
                    for (i=0; i < N; i++) {
                        double val = A(j,i);
                        A(j,i) = A(piv,i);
                        A(piv,i) = val;
                    }
                    // apply col-pivoting to A
                    for (i=0; i < N; i++) {
                        double val = A(i,j);
                        A(i,j) = A(i,piv);
                        A(i,piv) = val;
                    }
                }
            } else {
                piv = piv-1;
                // extract 1-by-1 pivot
                D(j,j) = LD(j,j);
                // exract L
                L(j,j) = c_one;
                for (i=j+1; i < N; i++) {
                    L(i,j) = LD(i,j);
                }
                if (piv != j) {
                    // apply row-pivoting to previous L
                    for (i=0; i < j; i++) {
                        double val = L(j,i);
                        L(j,i) = L(piv,i);
                        L(piv,i) = val;
                    }
                    // apply row-pivoting to A
                    for (i=0; i < N; i++) {
                        double val = A(j,i);
                        A(j,i) = A(piv,i);
                        A(piv,i) = val;
                    }
                    // apply col-pivoting to A
                    for (i=0; i < N; i++) {
                        double val = A(i,j);
                        A(i,j) = A(i,piv);
                        A(i,piv) = val;
                    }
                }
            }
        }
        // compute W = L*D
        blasf77_dgemm(MagmaNoTransStr, MagmaNoTransStr, &N, &N, &N,
                      &c_one, L, &N, D, &N, &c_zero, LD, &lda);
        // compute D = W*L'
        blasf77_dgemm(MagmaNoTransStr, MagmaConjTransStr, &N, &N, &N,
                      &c_one, LD, &lda, L, &N, &c_zero, D, &N);
    }
    // compute norm of A
    matnorm = lapackf77_dlange(MagmaFullStr, &N, &N, A, &lda, work);

    for( j = 0; j < N; j++ ) {
        for( i = 0; i < N; i++ ) {
            D(i,j) = MAGMA_D_SUB( D(i,j), A(i,j) );
        }
    }
    residual = lapackf77_dlange(MagmaFullStr, &N, &N, D, &N, work);

    TESTING_FREE_CPU( L );
    TESTING_FREE_CPU( D );

    return residual / (matnorm * N);
}


double get_LTLt_error(int nopiv, magma_uplo_t uplo, magma_int_t N,
                      double *LT, magma_int_t lda,
                      magma_int_t *ipiv)
{
    double work[1], matnorm, residual;
    double c_one  = MAGMA_D_ONE;
    double c_zero = MAGMA_D_ZERO;
    double *A, *L, *T;
    #define LT(i,j) (LT[(i) + (j)*lda])
    #define  T(i,j) ( T[(i) + (j)*N])
    TESTING_MALLOC_CPU( A, double, N*N );
    TESTING_MALLOC_CPU( L, double, N*N );
    TESTING_MALLOC_CPU( T, double, N*N );
    memset( L, 0, N*N*sizeof(double) );
    memset( T, 0, N*N*sizeof(double) );

    int nb = magma_get_dsytrf_aasen_nb(N);
    // for debuging
    /*
    magma_int_t *p;
    magma_imalloc_cpu(&p, N);
    for (int i=0; i<N; i++) {
        p[i] = i;
    }
    for (int i=0; i<N; i++) {
        int piv = ipiv[i]-1;
        int i2 = p[piv];
        p[piv] = p[i];
        p[i] = i2;
    }
    printf( " p=[" );
    for (int i=0; i<N; i++) {
        printf("%d ", p[i] );
    }
    printf( "];\n" );
    magma_free_cpu(p);
    */
    // extract T
    for (int i=0; i < N; i++) {
        int istart = max(0, i-nb);
        for (int j=istart; j <= i; j++) {
            T(i,j) = LT(i,j);
        }
        for (int j=istart; j < i;  j++) {
            T(j,i) = MAGMA_D_CNJG( LT(i,j) );
        }
    }
    //printf( "T=" );
    //magma_dprint(N,N, &T(0,0),N);
    // extract L
    for (int i=0; i<min(N,nb); i++) 
    {
        L(i,i) = c_one;
    }
    for (int i=nb; i<N; i++)
    {
        for (int j=0; j<i-nb; j++) {
            L(i,nb+j) = LT(i,j);
        }
        L(i,i) = c_one;
    }
    //printf( "L=" );
    //magma_dprint(N,N, &L(0,0),N);

    // compute LD = L*T
    blasf77_dgemm(MagmaNoTransStr, MagmaNoTransStr, &N, &N, &N,
                  &c_one, L, &N, T, &N, &c_zero, LT, &lda);
    // compute T = LD*L'
    blasf77_dgemm(MagmaNoTransStr, MagmaConjTransStr, &N, &N, &N,
                  &c_one, LT, &lda, L, &N, &c_zero, T, &N);

    // compute norm of A
    init_matrix( nopiv, N, N, A, N );
    matnorm = lapackf77_dlange(MagmaFullStr, &N, &N, A, &lda, work);
    //printf( "A0=" );
    //magma_dprint(N,N, &A(0,0),N);

    // apply symmetric pivoting
    for (int j=0; j<N; j++) {
        int piv = ipiv[j]-1;
        if (piv != j) {
            // apply row-pivoting to A
            for (int i=0; i<N; i++) {
                double val = A(j,i);
                A(j,i) = A(piv,i);
                A(piv,i) = val;
            }
            // apply col-pivoting to A
            for (int i=0; i<N; i++) {
                double val = A(i,j);
                A(i,j) = A(i,piv);
                A(i,piv) = val;
            }
        }
    }

    // compute factorization error
    for(int j = 0; j < N; j++ ) {
        for(int i = 0; i < N; i++ ) {
            T(i,j) = MAGMA_D_SUB( T(i,j), A(i,j) );
        }
    }
    residual = lapackf77_dlange(MagmaFullStr, &N, &N, T, &N, work);

    TESTING_FREE_CPU( A );
    TESTING_FREE_CPU( L );
    TESTING_FREE_CPU( T );

    return residual / (matnorm * N);
}

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing dsytrf
*/
int main( int argc, char** argv)
{
    TESTING_INIT();

    double *h_A, *work, temp;
    real_Double_t   gflops, gpu_perf, gpu_time = 0.0, cpu_perf=0, cpu_time=0;
    double          error, error_lapack = 0.0;
    magma_int_t     *ipiv;
    magma_int_t     cpu_panel = 1, N, n2, lda, lwork, info;
    magma_int_t     status = 0;
    magma_int_t     cpu = 0, nopiv = 0, nopiv_gpu = 0, row = 0, aasen = 0;
    
    magma_opts opts;
    opts.parse_opts( argc, argv );
    
    // TODO: this doesn't work. Options need to be added to parse_opts()
    for( int i = 1; i < argc; ++i ) {
        if ( strcmp("--cpu-panel", argv[i]) == 0) cpu_panel = 1;
        if ( strcmp("--gpu-panel", argv[i]) == 0) cpu_panel = 0;
    }
    
    switch (opts.version) {
        case 1:
            cpu = 1;
            printf( "\n%% CPU-Interface to Bunch-Kauffman on GPU" );
            break;
        case 2:
            //gpu = 1;
            printf( "\n%% GPU-Interface to Bunch-Kauffman on GPU" );
            printf( "\n not yet..\n\n" );
            return 0;
            break;
        case 3:
            nopiv = 1;
            printf( "\n%% CPU-Interface to hybrid Non-pivoted LDLt (A is SPD)" );
            break;
        case 4:
            nopiv_gpu = 1;
            printf( "\n%% GPU-Interface to hybrid Non-pivoted LDLt (A is SPD)" );
            break;
        //case 5:
        //    row = 1;
        //    printf( "\n Bunch-Kauffman: GPU-only version (row-major)" );
        //    break;
        case 6:
            aasen = 1;
            printf( "\n%% CPU-Interface to Aasen's (%s)",(cpu_panel ? "CPU panel" : "GPU panel") );
            break;
        default:
            printf( "\nversion = %d not supported\n\n", (int) opts.version );
            return 0;
    }
    printf( " (%s)\n", lapack_uplo_const(opts.uplo) );
    printf( " (--version: 1 = Bunch-Kauffman (CPU), 2 = Bunch-Kauffman (GPU), 3 = No-piv (CPU), 4 = No-piv (GPU))\n\n" );
    
    double tol = opts.tolerance * lapackf77_dlamch("E");

    if ( opts.check == 2 ) {
        printf("%%   M     N   CPU GFlop/s (sec)   GPU GFlop/s (sec)   |Ax-b|/(N*|A|*|x|)\n");
    }
    else {
        printf("%%   M     N   CPU GFlop/s (sec)   GPU GFlop/s (sec)   |PAP'-LDL'|/(N*|A|)\n");
    }
    printf("%%========================================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            N = opts.nsize[itest];
            lda    = N;
            n2     = lda*N;
            gflops = FLOPS_DPOTRF( N ) / 1e9;
            
            TESTING_MALLOC_PIN( ipiv, magma_int_t, N );
            TESTING_MALLOC_PIN( h_A,  double, n2 );
            
            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            if ( opts.lapack ) {
                lwork = -1;
                lapackf77_dsytrf( lapack_uplo_const(opts.uplo), &N, h_A, &lda, ipiv, &temp, &lwork, &info);
                lwork = (int)MAGMA_D_REAL(temp);
                TESTING_MALLOC_CPU( work, double, lwork );

                init_matrix( nopiv, N, N, h_A, lda );
                cpu_time = magma_wtime();
                lapackf77_dsytrf( lapack_uplo_const(opts.uplo), &N, h_A, &lda, ipiv, work, &lwork, &info);
                cpu_time = magma_wtime() - cpu_time;
                cpu_perf = gflops / cpu_time;
                if (info != 0)
                    printf("lapackf77_dsytrf returned error %d: %s.\n",
                           (int) info, magma_strerror( info ));
                error_lapack = get_residual( nopiv, opts.uplo, N, h_A, lda, ipiv );

                TESTING_FREE_CPU( work );
            }
           
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            init_matrix( (nopiv | nopiv_gpu), N, N, h_A, lda );

            //printf( "A0=" );
            //magma_dprint(N,N,h_A,lda);
            if (nopiv) {
                // CPU-interface to non-piv LDLt
                gpu_time = magma_wtime();
                magma_dsytrf_nopiv( opts.uplo, N, h_A, lda, &info);
                gpu_time = magma_wtime() - gpu_time;
            } else if (cpu) {
                // CPU-interface to Bunch-Kauffman LDLt
                gpu_time = magma_wtime();
                magma_dsytrf( opts.uplo, N, h_A, lda, ipiv, &info);
                gpu_time = magma_wtime() - gpu_time;
            } else if (nopiv_gpu) {
                // GPU-interface to non-piv LDLt
                magma_int_t ldda = magma_roundup( N, opts.align );
                magmaDouble_ptr d_A;
                if (MAGMA_SUCCESS != magma_dmalloc( &d_A, N*ldda  )) {
                    printf( " failed to allocate d_A(%dx%d)\n", (int) N, (int) ldda );
                    return 0;
                }
                magma_dsetmatrix(N, N, h_A, lda, d_A, ldda);
                gpu_time = magma_wtime();
                magma_dsytrf_nopiv_gpu( opts.uplo, N, d_A, ldda, &info);
                gpu_time = magma_wtime() - gpu_time;
                magma_dgetmatrix(N, N, d_A, ldda, h_A, lda);
                magma_free( d_A );
            } else if (aasen) {
                // CPU-interface to Aasen's LTLt
                gpu_time = magma_wtime();
                magma_dsytrf_aasen( opts.uplo, cpu_panel, N, h_A, lda, ipiv, &info);
                gpu_time = magma_wtime() - gpu_time;
            } else if (row) {
                //magma_dsytrf_gpu_row( opts.uplo, N, h_A, lda, ipiv, work, lwork, &info);
            } else {
                //magma_dsytrf_hybrid( opts.uplo, N, h_A, lda, ipiv, work, lwork, &info);
            }
            gpu_perf = gflops / gpu_time;
            if (info != 0)
                printf("magma_dsytrf returned error %d: %s.\n",
                       (int) info, magma_strerror( info ));
            
            /* =====================================================================
               Check the factorization
               =================================================================== */
            if ( opts.lapack ) {
                printf("%5d %5d   %7.2f (%7.2f)   %7.2f (%7.2f)",
                       (int) N, (int) N, cpu_perf, cpu_time, gpu_perf, gpu_time );
            }
            else {
                printf("%5d %5d     ---   (  ---  )   %7.2f (%7.2f)",
                       (int) N, (int) N, gpu_perf, gpu_time );
            }
            if ( opts.check == 2 ) {
                if (aasen) {
                    error = get_residual_aasen( (nopiv | nopiv_gpu), opts.uplo, N, h_A, lda, ipiv );
                } else {
                    error = get_residual( (nopiv | nopiv_gpu), opts.uplo, N, h_A, lda, ipiv );
                }
                printf("   %8.2e   %s", error, (error < tol ? "ok" : "failed"));
                if (opts.lapack)
                    printf(" (lapack rel.res. = %8.2e)", error_lapack);
                printf("\n");
                status += ! (error < tol);
            }
            else if ( opts.check ) {
                if (aasen) {
                    error = get_LTLt_error( (nopiv | nopiv_gpu), opts.uplo, N, h_A, lda, ipiv );
                } else {
                    error = get_LDLt_error( (nopiv | nopiv_gpu), opts.uplo, N, h_A, lda, ipiv );
                }
                printf("   %8.2e   %s\n", error, (error < tol ? "ok" : "failed"));
                status += ! (error < tol);
            }
            else {
                printf("     ---   \n");
            }
 
            TESTING_FREE_PIN( ipiv );
            TESTING_FREE_PIN( h_A  );
            fflush( stdout );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }

    TESTING_FINALIZE();
    return status;
}
