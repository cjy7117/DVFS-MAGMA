/*
    -- MAGMA (version 1.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2012

       @author Stan Tomov
       @author Raffaele Solca

       @precisions normal z -> c

*/
#include "common_magma.h"
#include "magma_bulge.h"
#include "magma_zbulge.h"
#include <cblas.h>

#define PRECISION_z

extern"C" {
    void magma_zmove_eig(char range, magma_int_t n, double *w, magma_int_t *il,
                         magma_int_t *iu, double vl, double vu, magma_int_t *m);

    magma_int_t magma_zbulge_get_Vblksiz(magma_int_t n, magma_int_t nb)
    {
#if (defined(PRECISION_s) || defined(PRECISION_d))
        return min(nb,48);
#else
        return min(nb,32);
#endif
    }

    magma_int_t magma_zbulge_get_lq2(magma_int_t n)
    {
        magma_int_t nb = magma_bulge_get_nb(n);
        magma_int_t Vblksiz = magma_zbulge_get_Vblksiz(n, nb);
        magma_int_t ldv = nb + Vblksiz + 1;
        magma_int_t ldt = Vblksiz;
        return magma_bulge_get_blkcnt(n, nb, Vblksiz) * Vblksiz * (ldt + ldv + 1);
    }
}

extern "C" magma_int_t
magma_zheevdx_2stage(char jobz, char range, char uplo,
                     magma_int_t n,
                     cuDoubleComplex *a, magma_int_t lda,
                     double vl, double vu, magma_int_t il, magma_int_t iu,
                     magma_int_t *m, double *w,
                     cuDoubleComplex *work, magma_int_t lwork,
                     double *rwork, magma_int_t lrwork,
                     magma_int_t *iwork, magma_int_t liwork,
                     magma_int_t *info)
{
/*  -- MAGMA (version 1.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2012

    Purpose
    =======
    ZHEEVD_2STAGE computes all eigenvalues and, optionally, eigenvectors of a
    complex Hermitian matrix A. It uses a two-stage algorithm for the tridiagonalization.
    If eigenvectors are desired, it uses a divide and conquer algorithm.

    The divide and conquer algorithm makes very mild assumptions about
    floating point arithmetic. It will work on machines with a guard
    digit in add/subtract, or on those binary machines without guard
    digits which subtract like the Cray X-MP, Cray Y-MP, Cray C-90, or
    Cray-2. It could conceivably fail on hexadecimal or decimal machines
    without guard digits, but we know of none.

    Arguments
    =========
    JOBZ    (input) CHARACTER*1
            = 'N':  Compute eigenvalues only;
            = 'V':  Compute eigenvalues and eigenvectors.

    RANGE   (input) CHARACTER*1
            = 'A': all eigenvalues will be found.
            = 'V': all eigenvalues in the half-open interval (VL,VU]
                   will be found.
            = 'I': the IL-th through IU-th eigenvalues will be found.

    UPLO    (input) CHARACTER*1
            = 'U':  Upper triangle of A is stored;
            = 'L':  Lower triangle of A is stored.

    N       (input) INTEGER
            The order of the matrix A.  N >= 0.

    A       (input/output) COMPLEX_16 array, dimension (LDA, N)
            On entry, the Hermitian matrix A.  If UPLO = 'U', the
            leading N-by-N upper triangular part of A contains the
            upper triangular part of the matrix A.  If UPLO = 'L',
            the leading N-by-N lower triangular part of A contains
            the lower triangular part of the matrix A.
            On exit, if JOBZ = 'V', then if INFO = 0, the first m columns
            of A contains the required
            orthonormal eigenvectors of the matrix A.
            If JOBZ = 'N', then on exit the lower triangle (if UPLO='L')
            or the upper triangle (if UPLO='U') of A, including the
            diagonal, is destroyed.

    LDA     (input) INTEGER
            The leading dimension of the array A.  LDA >= max(1,N).

    VL      (input) DOUBLE PRECISION
    VU      (input) DOUBLE PRECISION
            If RANGE='V', the lower and upper bounds of the interval to
            be searched for eigenvalues. VL < VU.
            Not referenced if RANGE = 'A' or 'I'.

    IL      (input) INTEGER
    IU      (input) INTEGER
            If RANGE='I', the indices (in ascending order) of the
            smallest and largest eigenvalues to be returned.
            1 <= IL <= IU <= N, if N > 0; IL = 1 and IU = 0 if N = 0.
            Not referenced if RANGE = 'A' or 'V'.

    M       (output) INTEGER
            The total number of eigenvalues found.  0 <= M <= N.
            If RANGE = 'A', M = N, and if RANGE = 'I', M = IU-IL+1.

    W       (output) DOUBLE PRECISION array, dimension (N)
            If INFO = 0, the required m eigenvalues in ascending order.

    WORK    (workspace/output) COMPLEX_16 array, dimension (MAX(1,LWORK))
            On exit, if INFO = 0, WORK(1) returns the optimal LWORK.

    LWORK   (input) INTEGER
            The length of the array WORK.
            If N <= 1,                LWORK >= 1.
            If JOBZ  = 'N' and N > 1, LWORK >= LQ2 + N * (NB + 1).
            If JOBZ  = 'V' and N > 1, LWORK >= LQ2 + 2*N + N**2.
                                      where LQ2 is the size needed to store
                                      the Q2 matrix and is returned by
                                      MAGMA_BULGE_GET_LQ2.

            If LWORK = -1, then a workspace query is assumed; the routine
            only calculates the optimal sizes of the WORK, RWORK and
            IWORK arrays, returns these values as the first entries of
            the WORK, RWORK and IWORK arrays, and no error message
            related to LWORK or LRWORK or LIWORK is issued by XERBLA.

    RWORK   (workspace/output) DOUBLE PRECISION array,
                                           dimension (LRWORK)
            On exit, if INFO = 0, RWORK(1) returns the optimal LRWORK.

    LRWORK  (input) INTEGER
            The dimension of the array RWORK.
            If N <= 1,                LRWORK must be at least 1.
            If JOBZ  = 'N' and N > 1, LRWORK must be at least N.
            If JOBZ  = 'V' and N > 1, LRWORK must be at least
                           1 + 5*N + 2*N**2.

            If LRWORK = -1, then a workspace query is assumed; the
            routine only calculates the optimal sizes of the WORK, RWORK
            and IWORK arrays, returns these values as the first entries
            of the WORK, RWORK and IWORK arrays, and no error message
            related to LWORK or LRWORK or LIWORK is issued by XERBLA.

    IWORK   (workspace/output) INTEGER array, dimension (MAX(1,LIWORK))
            On exit, if INFO = 0, IWORK(1) returns the optimal LIWORK.

    LIWORK  (input) INTEGER
            The dimension of the array IWORK.
            If N <= 1,                LIWORK must be at least 1.
            If JOBZ  = 'N' and N > 1, LIWORK must be at least 1.
            If JOBZ  = 'V' and N > 1, LIWORK must be at least 3 + 5*N.

            If LIWORK = -1, then a workspace query is assumed; the
            routine only calculates the optimal sizes of the WORK, RWORK
            and IWORK arrays, returns these values as the first entries
            of the WORK, RWORK and IWORK arrays, and no error message
            related to LWORK or LRWORK or LIWORK is issued by XERBLA.

    INFO    (output) INTEGER
            = 0:  successful exit
            < 0:  if INFO = -i, the i-th argument had an illegal value
            > 0:  if INFO = i and JOBZ = 'N', then the algorithm failed
                  to converge; i off-diagonal elements of an intermediate
                  tridiagonal form did not converge to zero;
                  if INFO = i and JOBZ = 'V', then the algorithm failed
                  to compute an eigenvalue while working on the submatrix
                  lying in rows and columns INFO/(N+1) through
                  mod(INFO,N+1).

    Further Details
    ===============
    Based on contributions by
       Jeff Rutter, Computer Science Division, University of California
       at Berkeley, USA

    Modified description of INFO. Sven, 16 Feb 05.
    =====================================================================   */

    char uplo_[2] = {uplo, 0};
    char jobz_[2] = {jobz, 0};
    char range_[2] = {range, 0};
    cuDoubleComplex c_one  = MAGMA_Z_ONE;
    magma_int_t ione = 1;
    magma_int_t izero = 0;
    double c_b18 = 1.;

    double d__1;

    double eps;
    double anrm;
    magma_int_t imax;
    double rmin, rmax;
    double sigma;
    magma_int_t iinfo;
    magma_int_t lwmin, lrwmin, liwmin;
    magma_int_t lower;
    magma_int_t wantz;
    magma_int_t iscale;
    double safmin;
    double bignum;
    double smlnum;
    magma_int_t lquery;
    magma_int_t alleig, valeig, indeig;

    double* dwork;

    wantz = lapackf77_lsame(jobz_, MagmaVectorsStr);
    lower = lapackf77_lsame(uplo_, MagmaLowerStr);

    alleig = lapackf77_lsame( range_, "A" );
    valeig = lapackf77_lsame( range_, "V" );
    indeig = lapackf77_lsame( range_, "I" );

    lquery = lwork == -1 || lrwork == -1 || liwork == -1;

    *info = 0;
    if (! (wantz || lapackf77_lsame(jobz_, MagmaNoVectorsStr))) {
        *info = -1;
    } else if (! (alleig || valeig || indeig)) {
        *info = -2;
    } else if (! (lower || lapackf77_lsame(uplo_, MagmaUpperStr))) {
        *info = -3;
    } else if (n < 0) {
        *info = -4;
    } else if (lda < max(1,n)) {
        *info = -6;
    } else {
        if (valeig) {
            if (n > 0 && vu <= vl) {
                *info = -8;
            }
        } else if (indeig) {
            if (il < 1 || il > max(1,n)) {
                *info = -9;
            } else if (iu < min(n,il) || iu > n) {
                *info = -10;
            }
        }
    }

    magma_int_t nb = magma_bulge_get_nb(n);
    magma_int_t Vblksiz = magma_zbulge_get_Vblksiz(n, nb);

    magma_int_t ldt = Vblksiz;
    magma_int_t ldv = nb + Vblksiz + 1;
    magma_int_t blkcnt = magma_bulge_get_blkcnt(n, nb, Vblksiz);

    magma_int_t lq2 = blkcnt * Vblksiz * (ldt + ldv + 1);

    if (wantz) {
        lwmin = lq2 + 2 * n + n * n;
        lrwmin = 1 + 5 * n + 2 * n * n;
        liwmin = 5 * n + 3;
    } else {
        lwmin = lq2 + n * (nb + 1);
        lrwmin = n;
        liwmin = 1;
    }

    MAGMA_Z_SET2REAL(work[0],(double)lwmin);
    rwork[0] = lrwmin;
    iwork[0] = liwmin;

    if ((lwork < lwmin) && !lquery) {
        *info = -14;
    } else if ((lrwork < lrwmin) && ! lquery) {
        *info = -16;
    } else if ((liwork < liwmin) && ! lquery) {
        *info = -18;
    }

    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }
    else if (lquery) {
        return *info;
    }

    /* Quick return if possible */
    if (n == 0) {
        return *info;
    }

    if (n == 1) {
        w[0] = MAGMA_Z_REAL(a[0]);
        if (wantz) {
            a[0] = MAGMA_Z_ONE;
        }
        return *info;
    }

    magma_int_t threads = 0;
    char *env;

    /* determine the number of threads */

    // First check MKL_NUM_THREADS if MKL is used
#ifdef USEMKL
    env = getenv("MKL_NUM_THREADS");
    if (env != NULL)
        threads = atoi(env);
#endif
    // Second check OMP_NUM_THREADS
    if (threads < 1){
        env = getenv("OMP_NUM_THREADS");
        if (env != NULL)
            threads = atoi(env);
    }
    // Third use the number of CPUs
    if (threads < 1)
        threads = sysconf(_SC_NPROCESSORS_ONLN);
    // Fourth use one thread
    if (threads < 1)
        threads = 1;


#define ENABLE_TIMER
#ifdef ENABLE_TIMER
    printf("using %d threads\n", threads);
#endif

    /* Get machine constants. */
    safmin = lapackf77_dlamch("Safe minimum");
    eps = lapackf77_dlamch("Precision");
    smlnum = safmin / eps;
    bignum = 1. / smlnum;
    rmin = magma_dsqrt(smlnum);
    rmax = magma_dsqrt(bignum);

    /* Scale matrix to allowable range, if necessary. */
    anrm = lapackf77_zlanhe("M", uplo_, &n, a, &lda, rwork);
    iscale = 0;
    if (anrm > 0. && anrm < rmin) {
        iscale = 1;
        sigma = rmin / anrm;
    } else if (anrm > rmax) {
        iscale = 1;
        sigma = rmax / anrm;
    }
    if (iscale == 1) {
        lapackf77_zlascl(uplo_, &izero, &izero, &c_b18, &sigma, &n, &n, a,
                         &lda, info);
    }

    magma_int_t indT2   = 0;
    magma_int_t indTAU2 = indT2  + blkcnt*ldt*Vblksiz;
    magma_int_t indV2   = indTAU2+ blkcnt*Vblksiz;
    magma_int_t indtau1 = indV2  + blkcnt*ldv*Vblksiz;
    magma_int_t indwrk  = indtau1+ n;
    magma_int_t indwk2  = indwrk + n * n;

    magma_int_t llwork = lwork - indwrk;
    magma_int_t llwrk2 = lwork - indwk2;

    magma_int_t inde = 0;
    magma_int_t indrwk = inde + n;

    magma_int_t llrwk = lrwork - indrwk;

#define ENABLE_TIMER
#ifdef ENABLE_TIMER
    magma_timestr_t start, st1, st2, end;

    start = get_current_time();
#endif

    cuDoubleComplex *dT1;

    if (MAGMA_SUCCESS != magma_zmalloc( &dT1, n*nb)) {
        *info = MAGMA_ERR_DEVICE_ALLOC;
        return *info;
    }

    magma_zhetrd_he2hb(uplo, n, nb, a, lda, &work[indtau1], &work[indwrk], llwork, dT1, threads, info);

#ifdef ENABLE_TIMER
    st1 = get_current_time();

    printf("  time zhetrd_he2hb = %6.2f\n" , GetTimerValue(start,st1)/1000.);
#endif

    magma_int_t lda2 = nb+1+(nb-1);
    if(lda2>n){
        printf("error matrix too small N=%d NB=%d\n",n,nb);
        return -14;
    }

    /* copy the input matrix into WORK(INDWRK) with band storage */
    cuDoubleComplex* A2 = &work[indwrk];

    memset(A2 , 0, n*lda2*sizeof(cuDoubleComplex));

    for (magma_int_t j = 0; j < n-nb; j++)
    {
        cblas_zcopy(nb+1, &a[j*(lda+1)], 1, &A2[j*lda2], 1);
        memset(&a[j*(lda+1)], 0, (nb+1)*sizeof(cuDoubleComplex));
        a[nb + j*(lda+1)] = c_one;
    }
    for (magma_int_t j = 0; j < nb; j++)
    {
        cblas_zcopy(nb-j, &a[(j+n-nb)*(lda+1)], 1, &A2[(j+n-nb)*lda2], 1);
        memset(&a[(j+n-nb)*(lda+1)], 0, (nb-j)*sizeof(cuDoubleComplex));
    }

#ifdef ENABLE_TIMER
    st2 = get_current_time();

    printf("  time zhetrd_convert = %6.2f\n" , GetTimerValue(st1,st2)/1000.);
#endif

    magma_zhetrd_hb2st(threads, uplo, n, nb, Vblksiz, A2, lda2, w, &rwork[inde], &work[indV2], ldv, &work[indTAU2], wantz, &work[indT2], ldt);

#ifdef ENABLE_TIMER
    end = get_current_time();

    printf("  time zhetrd_hb2st = %6.2f\n" , GetTimerValue(st2,end)/1000.);
    printf("time zhetrd = %6.2f\n", GetTimerValue(start,end)/1000.);
#endif

    /* For eigenvalues only, call DSTERF.  For eigenvectors, first call
     ZSTEDC to generate the eigenvector matrix, WORK(INDWRK), of the
     tridiagonal matrix, then call ZUNMTR to multiply it to the Householder
     transformations represented as Householder vectors in A. */
    if (! wantz) {
        lapackf77_dsterf(&n, w, &rwork[inde], info);

        magma_zmove_eig(range, n, w, &il, &iu, vl, vu, m);
    } else {

#ifdef ENABLE_TIMER
        start = get_current_time();
#endif

        if (MAGMA_SUCCESS != magma_dmalloc( &dwork, 3*n*(n/2 + 1) )) {
            *info = MAGMA_ERR_DEVICE_ALLOC;
            return *info;
        }

        magma_zstedx(range, n, vl, vu, il, iu, w, &rwork[inde],
                     &work[indwrk], n, &rwork[indrwk],
                     llrwk, iwork, liwork, dwork, info);

        magma_free( dwork );

#ifdef ENABLE_TIMER
        end = get_current_time();

        printf("time zstedx = %6.2f\n", GetTimerValue(start,end)/1000.);

        start = get_current_time();
#endif
        cuDoubleComplex *dZ;
        magma_int_t lddz = n;

        cuDoubleComplex *da;
        magma_int_t ldda = n;

        magma_zmove_eig(range, n, w, &il, &iu, vl, vu, m);

        if (MAGMA_SUCCESS != magma_zmalloc( &dZ, *m*lddz)) {
            *info = MAGMA_ERR_DEVICE_ALLOC;
            return *info;
        }
        if (MAGMA_SUCCESS != magma_zmalloc( &da, n*ldda )) {
            *info = MAGMA_ERR_DEVICE_ALLOC;
            return *info;
        }

        magma_zbulge_back(threads, uplo, n, nb, *m, Vblksiz, &work[indwrk + n * (il-1)], n, dZ, lddz,
                          &work[indV2], ldv, &work[indTAU2], &work[indT2], ldt, info);

#ifdef ENABLE_TIMER
        st1 = get_current_time();

        printf("  time zbulge_back = %6.2f\n" , GetTimerValue(start,st1)/1000.);
#endif

        magma_zsetmatrix( n, n, a, lda, da, ldda );

        magma_zunmqr_gpu_2stages(MagmaLeft, MagmaNoTrans, n-nb, *m, n-nb, da+nb, ldda,
                                 dZ+nb, n, dT1, nb, info);

        magma_zgetmatrix( n, *m, dZ, lddz, a, lda );
        magma_free(dT1);
        magma_free(dZ);
        magma_free(da);

#ifdef ENABLE_TIMER
        end = get_current_time();

        printf("  time zunmqr + copy = %6.2f\n", GetTimerValue(st1,end)/1000.);

        printf("time eigenvectors backtransf. = %6.2f\n" , GetTimerValue(start,end)/1000.);
#endif

    }

    /* If matrix was scaled, then rescale eigenvalues appropriately. */
    if (iscale == 1) {
        if (*info == 0) {
            imax = n;
        } else {
            imax = *info - 1;
        }
        d__1 = 1. / sigma;
        blasf77_dscal(&imax, &d__1, w, &ione);
    }

    work[0]  = MAGMA_Z_MAKE((double) lwmin, 0.);
    rwork[0] = (double) lrwmin;
    iwork[0] = liwmin;

    return *info;
} /* magma_zheevdx_2stage */
