/*
 *  -- MAGMA (version 1.3.0) --
 *     Univ. of Tennessee, Knoxville
 *     Univ. of California, Berkeley
 *     Univ. of Colorado, Denver
 *     November 2012
 *
 * @author Mark Gates
 * @generated s Wed Nov 14 22:54:10 2012
 *
 **/
#include <stdlib.h>
#include <stdio.h>

// make sure that asserts are enabled
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

// includes, project
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"

#define A(i,j)  &A[  (i) + (j)*ld ]
#define dA(i,j) &dA[ (i) + (j)*ld ]
#define C2(i,j) &C2[ (i) + (j)*ld ]

int main( int argc, char** argv )
{
    TESTING_CUDA_INIT();
    
    float c_neg_one = MAGMA_S_NEG_ONE;
    magma_int_t ione = 1;
    const char trans[] = { 'N', 'C', 'T' };
    const char uplo[]  = { 'L', 'U' };
    const char diag[]  = { 'U', 'N' };
    const char side[]  = { 'L', 'R' };
    
    float  *A,  *B,  *C,   *C2;
    float *dA, *dB, *dC1, *dC2;
    float alpha = MAGMA_S_MAKE( 0.5, 0.1 );
    float beta  = MAGMA_S_MAKE( 0.7, 0.2 );
    float dalpha = 0.6;
    float dbeta  = 0.8;
    float work[1], error;
    magma_int_t m = 50;
    magma_int_t n = 35;
    magma_int_t k = 40;
    magma_int_t ISEED[4] = { 0, 1, 2, 3 };
    magma_int_t size, maxn, ld, info;
    magma_int_t *piv;
    magma_err_t err;
    
    printf( "\n" );
    
    // allocate matrices
    // over-allocate so they can be any combination of {m,n,k} x {m,n,k}.
    maxn = max( max( m, n ), k );
    ld = maxn;
    size = maxn*maxn;
    err = magma_malloc_cpu( (void**) &piv, maxn );    assert( err == 0 );
    err = magma_smalloc_pinned( &A , size );  assert( err == 0 );
    err = magma_smalloc_pinned( &B , size );  assert( err == 0 );
    err = magma_smalloc_pinned( &C , size );  assert( err == 0 );
    err = magma_smalloc_pinned( &C2, size );  assert( err == 0 );
    err = magma_smalloc( &dA,  size );      assert( err == 0 );
    err = magma_smalloc( &dB,  size );      assert( err == 0 );
    err = magma_smalloc( &dC1, size );      assert( err == 0 );
    err = magma_smalloc( &dC2, size );      assert( err == 0 );
    
    // initialize matrices
    size = maxn*maxn;
    lapackf77_slarnv( &ione, ISEED, &size, A  );
    lapackf77_slarnv( &ione, ISEED, &size, B  );
    lapackf77_slarnv( &ione, ISEED, &size, C  );
    
    printf( "========== Level 1 BLAS ==========\n" );
    
    // ----- test SSWAP
    // swap 2nd and 3rd columns of dA, then copy to C2 and compare with A
    printf( "\ntesting sswap\n" );
    assert( k >= 4 );
    magma_ssetmatrix( m, k, A, ld, dA, ld );
    magma_sswap( m, dA(0,1), 1, dA(0,2), 1 );
    magma_sgetmatrix( m, k, dA, ld, C2, ld );
    blasf77_saxpy( &m, &c_neg_one, A(0,0), &ione, C2(0,0), &ione );
    blasf77_saxpy( &m, &c_neg_one, A(0,1), &ione, C2(0,2), &ione );  // swapped
    blasf77_saxpy( &m, &c_neg_one, A(0,2), &ione, C2(0,1), &ione );  // swapped
    blasf77_saxpy( &m, &c_neg_one, A(0,3), &ione, C2(0,3), &ione );
    size = 4;
    error = lapackf77_slange( "F", &m, &size, C2, &ld, work );
    printf( "sswap diff %.2g\n", error );
    
    // ----- test ISAMAX
    // get argmax of column of A
    printf( "\ntesting isamax\n" );
    magma_ssetmatrix( m, k, A, ld, dA, ld );
    for( int j = 0; j < k; ++j ) {
        int i1 = magma_isamax( m, dA(0,j), 1 );
        int i2 = cublasIsamax( m, dA(0,j), 1 );
        assert( i1 == i2 );
        printf( "i1 %4d, i2 %4d, diff %d\n", i1, i2, i1-i2 );
    }
    
    printf( "\n========== Level 2 BLAS ==========\n" );
    
    // ----- test SGEMV
    // c = alpha*A*b + beta*c,  with A m*n; b,c m or n-vectors
    // try no-trans/trans
    printf( "\ntesting sgemv\n" );
    for( int ia = 0; ia < 3; ++ia ) {
        magma_ssetmatrix( m, n, A,  ld, dA,  ld );
        magma_ssetvector( maxn, B, 1, dB,  1 );
        magma_ssetvector( maxn, C, 1, dC1, 1 );
        magma_ssetvector( maxn, C, 1, dC2, 1 );
        magma_sgemv( trans[ia], m, n, alpha, dA, ld, dB, 1, beta, dC1, 1 );
        cublasSgemv( trans[ia], m, n, alpha, dA, ld, dB, 1, beta, dC2, 1 );
        
        // check results, storing diff between magma and cuda call in C2
        size = (trans[ia] == 'N' ? m : n);
        cublasSaxpy( size, c_neg_one, dC1, 1, dC2, 1 );
        magma_sgetvector( size, dC2, 1, C2, 1 );
        error = lapackf77_slange( "F", &size, &ione, C2, &ld, work );
        printf( "sgemv( %c ) diff %.2g\n", trans[ia], error );
    }
    
    // ----- test SSYMV
    // c = alpha*A*b + beta*c,  with A m*m symmetric; b,c m-vectors
    // try upper/lower
    printf( "\ntesting ssymv\n" );
    for( int iu = 0; iu < 2; ++iu ) {
        magma_ssetmatrix( m, m, A, ld, dA, ld );
        magma_ssetvector( m, B, 1, dB,  1 );
        magma_ssetvector( m, C, 1, dC1, 1 );
        magma_ssetvector( m, C, 1, dC2, 1 );
        magma_ssymv( uplo[iu], m, alpha, dA, ld, dB, 1, beta, dC1, 1 );
        cublasSsymv( uplo[iu], m, alpha, dA, ld, dB, 1, beta, dC2, 1 );
                                      
        // check results, storing diff between magma and cuda call in C2
        cublasSaxpy( m, c_neg_one, dC1, 1, dC2, 1 );
        magma_sgetvector( m, dC2, 1, C2, 1 );
        error = lapackf77_slange( "F", &m, &ione, C2, &ld, work );
        printf( "ssymv( %c ) diff %.2g\n", uplo[iu], error );
    }
    
    // ----- test STRSV
    // solve A*c = c,  with A m*m triangular; c m-vector
    // try upper/lower, no-trans/trans, unit/non-unit diag
    printf( "\ntesting strsv\n" );
    // Factor A into LU to get well-conditioned triangles, else solve yields garbage.
    // Still can give garbage if solves aren't consistent with LU factors,
    // e.g., using unit diag for U.
    lapackf77_sgetrf( &m, &m, A, &ld, piv, &info );
    for( int iu = 0; iu < 2; ++iu ) {
    for( int it = 0; it < 3; ++it ) {
    for( int id = 0; id < 2; ++id ) {
        magma_ssetmatrix( m, m, A, ld, dA, ld );
        magma_ssetvector( m, C, 1, dC1, 1 );
        magma_ssetvector( m, C, 1, dC2, 1 );
        magma_strsv( uplo[iu], trans[it], diag[id], m, dA, ld, dC1, 1 );
        cublasStrsv( uplo[iu], trans[it], diag[id], m, dA, ld, dC2, 1 );
                                      
        // check results, storing diff between magma and cuda call in C2
        cublasSaxpy( m, c_neg_one, dC1, 1, dC2, 1 );
        magma_sgetvector( m, dC2, 1, C2, 1 );
        error = lapackf77_slange( "F", &m, &ione, C2, &ld, work );
        printf( "strsv( %c, %c, %c ) diff %.2g\n", uplo[iu], trans[it], diag[id], error );
    }}}
    
    printf( "\n========== Level 3 BLAS ==========\n" );
    
    // ----- test SGEMM
    // C = alpha*A*B + beta*C,  with A m*k or k*m; B k*n or n*k; C m*n
    // try combinations of no-trans/trans
    printf( "\ntesting sgemm\n" );
    for( int ia = 0; ia < 3; ++ia ) {
    for( int ib = 0; ib < 3; ++ib ) {
        bool nta = (trans[ia] == 'N');
        bool ntb = (trans[ib] == 'N');
        magma_ssetmatrix( (nta ? m : k), (nta ? m : k), A, ld, dA,  ld );
        magma_ssetmatrix( (ntb ? k : n), (ntb ? n : k), B, ld, dB,  ld );
        magma_ssetmatrix( m, n, C, ld, dC1, ld );
        magma_ssetmatrix( m, n, C, ld, dC2, ld );
        magma_sgemm( trans[ia], trans[ib], m, n, k, alpha, dA, ld, dB, ld, beta, dC1, ld );
        cublasSgemm( trans[ia], trans[ib], m, n, k, alpha, dA, ld, dB, ld, beta, dC2, ld );
        
        // check results, storing diff between magma and cuda call in C2
        cublasSaxpy( ld*n, c_neg_one, dC1, 1, dC2, 1 );
        magma_sgetmatrix( m, n, dC2, ld, C2, ld );
        error = lapackf77_slange( "F", &m, &n, C2, &ld, work );
        printf( "sgemm( %c, %c ) diff %.2g\n", trans[ia], trans[ib], error );
    }}
    
    // ----- test SSYMM
    // C = alpha*A*B + beta*C  (left)  with A m*m symmetric; B,C m*n; or
    // C = alpha*B*A + beta*C  (right) with A n*n symmetric; B,C m*n
    // try left/right, upper/lower
    printf( "\ntesting ssymm\n" );
    for( int is = 0; is < 2; ++is ) {
    for( int iu = 0; iu < 2; ++iu ) {
        magma_ssetmatrix( m, m, A, ld, dA,  ld );
        magma_ssetmatrix( m, n, B, ld, dB,  ld );
        magma_ssetmatrix( m, n, C, ld, dC1, ld );
        magma_ssetmatrix( m, n, C, ld, dC2, ld );
        magma_ssymm( side[is], uplo[iu], m, n, alpha, dA, ld, dB, ld, beta, dC1, ld );
        cublasSsymm( side[is], uplo[iu], m, n, alpha, dA, ld, dB, ld, beta, dC2, ld );
        
        // check results, storing diff between magma and cuda call in C2
        cublasSaxpy( ld*n, c_neg_one, dC1, 1, dC2, 1 );
        magma_sgetmatrix( m, n, dC2, ld, C2, ld );
        error = lapackf77_slange( "F", &m, &n, C2, &ld, work );
        printf( "ssymm( %c, %c ) diff %.2g\n", side[is], uplo[iu], error );
    }}
    
    // ----- test SSYRK
    // C = alpha*A*A^H + beta*C  (no-trans) with A m*k and C m*m symmetric; or
    // C = alpha*A^H*A + beta*C  (trans)    with A k*m and C m*m symmetric
    // try upper/lower, no-trans/trans
    printf( "\ntesting ssyrk\n" );
    for( int iu = 0; iu < 2; ++iu ) {
    for( int it = 0; it < 3; ++it ) {
        magma_ssetmatrix( n, k, A, ld, dA,  ld );
        magma_ssetmatrix( n, n, C, ld, dC1, ld );
        magma_ssetmatrix( n, n, C, ld, dC2, ld );
        magma_ssyrk( uplo[iu], trans[it], n, k, dalpha, dA, ld, dbeta, dC1, ld );
        cublasSsyrk( uplo[iu], trans[it], n, k, dalpha, dA, ld, dbeta, dC2, ld );
        
        // check results, storing diff between magma and cuda call in C2
        cublasSaxpy( ld*n, c_neg_one, dC1, 1, dC2, 1 );
        magma_sgetmatrix( n, n, dC2, ld, C2, ld );
        error = lapackf77_slange( "F", &n, &n, C2, &ld, work );
        printf( "ssyrk( %c, %c ) diff %.2g\n", uplo[iu], trans[it], error );
    }}
    
    // ----- test SSYR2K
    // C = alpha*A*B^H + ^alpha*B*A^H + beta*C  (no-trans) with A,B n*k; C n*n symmetric; or
    // C = alpha*A^H*B + ^alpha*B^H*A + beta*C  (trans)    with A,B k*n; C n*n symmetric
    // try upper/lower, no-trans/trans
    printf( "\ntesting ssyr2k\n" );
    for( int iu = 0; iu < 2; ++iu ) {
    for( int it = 0; it < 3; ++it ) {
        bool nt = (trans[it] == 'N');
        magma_ssetmatrix( (nt ? n : k), (nt ? n : k), A, ld, dA,  ld );
        magma_ssetmatrix( n, n, C, ld, dC1, ld );
        magma_ssetmatrix( n, n, C, ld, dC2, ld );
        magma_ssyr2k( uplo[iu], trans[it], n, k, alpha, dA, ld, dB, ld, dbeta, dC1, ld );
        cublasSsyr2k( uplo[iu], trans[it], n, k, alpha, dA, ld, dB, ld, dbeta, dC2, ld );
        
        // check results, storing diff between magma and cuda call in C2
        cublasSaxpy( ld*n, c_neg_one, dC1, 1, dC2, 1 );
        magma_sgetmatrix( n, n, dC2, ld, C2, ld );
        error = lapackf77_slange( "F", &n, &n, C2, &ld, work );
        printf( "ssyr2k( %c, %c ) diff %.2g\n", uplo[iu], trans[it], error );
    }}
    
    // ----- test STRMM
    // C = alpha*A*C  (left)  with A m*m triangular; C m*n; or
    // C = alpha*C*A  (right) with A n*n triangular; C m*n
    // try left/right, upper/lower, no-trans/trans, unit/non-unit
    printf( "\ntesting strmm\n" );
    for( int is = 0; is < 2; ++is ) {
    for( int iu = 0; iu < 2; ++iu ) {
    for( int it = 0; it < 3; ++it ) {
    for( int id = 0; id < 2; ++id ) {
        bool left = (side[is] == 'L');
        magma_ssetmatrix( (left ? m : n), (left ? m : n), A, ld, dA,  ld );
        magma_ssetmatrix( m, n, C, ld, dC1, ld );
        magma_ssetmatrix( m, n, C, ld, dC2, ld );
        magma_strmm( side[is], uplo[iu], trans[it], diag[id], m, n, alpha, dA, ld, dC1, ld );
        cublasStrmm( side[is], uplo[iu], trans[it], diag[id], m, n, alpha, dA, ld, dC2, ld );
        
        // check results, storing diff between magma and cuda call in C2
        cublasSaxpy( ld*n, c_neg_one, dC1, 1, dC2, 1 );
        magma_sgetmatrix( m, n, dC2, ld, C2, ld );
        error = lapackf77_slange( "F", &n, &n, C2, &ld, work );
        printf( "strmm( %c, %c ) diff %.2g\n", uplo[iu], trans[it], error );
    }}}}
    
    // ----- test STRSM
    // solve A*X = alpha*B  (left)  with A m*m triangular; B m*n; or
    // solve X*A = alpha*B  (right) with A n*n triangular; B m*n
    // try left/right, upper/lower, no-trans/trans, unit/non-unit
    printf( "\ntesting strsm\n" );
    for( int is = 0; is < 2; ++is ) {
    for( int iu = 0; iu < 2; ++iu ) {
    for( int it = 0; it < 3; ++it ) {
    for( int id = 0; id < 2; ++id ) {
        bool left = (side[is] == 'L');
        magma_ssetmatrix( (left ? m : n), (left ? m : n), A, ld, dA,  ld );
        magma_ssetmatrix( m, n, C, ld, dC1, ld );
        magma_ssetmatrix( m, n, C, ld, dC2, ld );
        magma_strsm( side[is], uplo[iu], trans[it], diag[id], m, n, alpha, dA, ld, dC1, ld );
        cublasStrsm( side[is], uplo[iu], trans[it], diag[id], m, n, alpha, dA, ld, dC2, ld );
        
        // check results, storing diff between magma and cuda call in C2
        cublasSaxpy( ld*n, c_neg_one, dC1, 1, dC2, 1 );
        magma_sgetmatrix( m, n, dC2, ld, C2, ld );
        error = lapackf77_slange( "F", &n, &n, C2, &ld, work );
        printf( "strsm( %c, %c ) diff %.2g\n", uplo[iu], trans[it], error );
    }}}}
    
    // cleanup
    magma_free_cpu( piv );
    magma_free_pinned( A  );
    magma_free_pinned( B  );
    magma_free_pinned( C  );
    magma_free_pinned( C2 );
    magma_free( dA  );
    magma_free( dB  );
    magma_free( dC1 );
    magma_free( dC2 );
    
    TESTING_CUDA_FINALIZE();
    return 0;
}
