###
#
# @file      : CMakeLists.txt
#
# @description   : Project MAGMA
#
# @version       :
# @created by    : 
# @creation date : 23-01-2012
# @last modIFied : lun. 23 janv. 2012 19:44:56 CET
#
###

CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

# Add include and link directories
# --------------------------------
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})

# List of sources files
# ---------------------
SET(SRCS
    zbdt01.f
    zget22.f
    zhet21.f
    zhst01.f
    zunt01.f
	zqpt01.f
    zqrt02.f
    zlarfy.f
    zstt21.f
    ###
    cbdt01.f
    cget22.f
    chet21.f
    chst01.f
    cunt01.f
	cqpt01.f
    cqrt02.f
    clarfy.f
    cstt21.f
    ###
    dbdt01.f
    dget22.f
    dsyt21.f
    dhst01.f
    dort01.f
	dqpt01.f
    dqrt02.f
    dlarfy.f
    dstt21.f
    ###
    sbdt01.f
    sget22.f
    ssyt21.f
    shst01.f
    sort01.f
	sqpt01.f
    sqrt02.f
    slarfy.f
    sstt21.f
   )

# Create libmagma_lapacktest.a
# -----------------------------
ADD_LIBRARY(magma_lapacktest ${SRCS})
SET_PROPERTY(TARGET magma_lapacktest PROPERTY LINKER_LANGUAGE Fortran)
TARGET_LINK_LIBRARIES(magma_lapacktest ${TMG_LIBRARIES})

# Install step of liblapacktest.a
# -------------------------------
INSTALL(TARGETS magma_lapacktest
        DESTINATION lib)

# List of sources files
# ---------------------
SET(ALINTST
    aladhd.f alaerh.f alaesm.f alahd.f alareq.f
    alasum.f alasvm.f chkxer.f icopy.f ilaenv.f xlaenv.f xerbla.f
   )

SET(SCLNTST
    slaord.f
   )

SET(DZLNTST
    dlaord.f
   )

SET(SLINTST
    schkaa.f
    schkeq.f schkgb.f schkge.f schkgt.f
    schklq.f schkpb.f schkpo.f schkps.f schkpp.f
    schkpt.f schkq3.f schkql.f schkqp.f schkqr.f schkrq.f
    schktb.f schktp.f schktr.f
    schktz.f
    sdrvgt.f sdrvls.f sdrvpb.f
    sdrvpp.f sdrvpt.f sdrvsp.f
    serrgt.f serrlq.f serrls.f
    serrps.f serrql.f serrqp.f serrqr.f
    serrrq.f serrtr.f serrtz.f
    sgbt01.f sgbt02.f sgbt05.f sgelqs.f sgeqls.f sgeqrs.f
    sgerqs.f sget01.f sget02.f
    sget03.f sget04.f sget06.f sget07.f sgtt01.f sgtt02.f
    sgtt05.f slaptm.f slarhs.f slatb4.f slatb5.f slattb.f slattp.f
    slattr.f slavsp.f slavsy.f slqt01.f slqt02.f
    slqt03.f spbt01.f spbt02.f spbt05.f spot01.f
    spot02.f spot03.f spot05.f spst01.f sppt01.f
    sppt02.f sppt03.f sppt05.f sptt01.f sptt02.f
    sptt05.f sqlt01.f sqlt02.f sqlt03.f sqpt01.f
    sqrt01.f sqrt02.f sqrt03.f sqrt11.f sqrt12.f
    sqrt13.f sqrt14.f sqrt15.f sqrt16.f sqrt17.f
    srqt01.f srqt02.f srqt03.f srzt01.f srzt02.f
    sspt01.f ssyt01.f
    stbt02.f stbt03.f stbt05.f stbt06.f stpt01.f
    stpt02.f stpt03.f stpt05.f stpt06.f strt01.f
    strt02.f strt03.f strt05.f strt06.f
    stzt01.f stzt02.f sgennd.f
    ####
    serrvx.f sdrvge.f serrge.f sdrvgb.f sdrvpo.f
	serrpo.f
   )

SET(CLINTST
    cchkaa.f
    cchkeq.f cchkgb.f cchkge.f cchkgt.f
    cchklq.f cchkpb.f
    cchkpo.f cchkps.f cchkpp.f cchkpt.f cchkq3.f cchkql.f cchkqp.f
    cchkqr.f cchkrq.f cchktb.f
    cchktp.f cchktr.f cchktz.f
    cdrvgt.f cdrvhp.f
    cdrvls.f cdrvpb.f cdrvpp.f cdrvpt.f
    cdrvsp.f
    cerrgt.f cerrlq.f
    cerrls.f cerrps.f cerrql.f cerrqp.f
    cerrqr.f cerrrq.f cerrtr.f cerrtz.f
    cgbt01.f cgbt02.f cgbt05.f cgelqs.f cgeqls.f cgeqrs.f
    cgerqs.f cget01.f cget02.f
    cget03.f cget04.f cget07.f cgtt01.f cgtt02.f
    cgtt05.f chet01.f chpt01.f claipd.f claptm.f clarhs.f clatb4.f clatb5.f
    clatsp.f clatsy.f clattb.f clattp.f clattr.f
    clavhe.f clavhp.f clavsp.f clavsy.f clqt01.f
    clqt02.f clqt03.f cpbt01.f cpbt02.f cpbt05.f
    cpot01.f cpot02.f cpot03.f cpot05.f cpst01.f
    cppt01.f cppt02.f cppt03.f cppt05.f cptt01.f
    cptt02.f cptt05.f cqlt01.f cqlt02.f cqlt03.f
    cqpt01.f cqrt01.f cqrt02.f cqrt03.f cqrt11.f
    cqrt12.f cqrt13.f cqrt14.f cqrt15.f cqrt16.f
    cqrt17.f crqt01.f crqt02.f crqt03.f crzt01.f crzt02.f
    csbmv.f  cspt01.f
    cspt02.f cspt03.f csyt01.f csyt02.f csyt03.f
    ctbt02.f ctbt03.f ctbt05.f ctbt06.f ctpt01.f
    ctpt02.f ctpt03.f ctpt05.f ctpt06.f ctrt01.f
    ctrt02.f ctrt03.f ctrt05.f ctrt06.f
    ctzt01.f ctzt02.f sget06.f cgennd.f
    ####
    cerrvx.f cdrvge.f cdrvgb.f cerrge.f cdrvpo.f
	cerrpo.f
   )

SET(DLINTST
    dchkaa.f
    dchkeq.f dchkgb.f dchkge.f dchkgt.f
    dchklq.f dchkpb.f dchkpo.f dchkps.f dchkpp.f
    dchkpt.f dchkq3.f dchkql.f dchkqp.f dchkqr.f dchkrq.f
    dchktb.f dchktp.f dchktr.f
    dchktz.f
    ddrvgt.f ddrvls.f ddrvpb.f
    ddrvpp.f ddrvpt.f ddrvsp.f
    derrgt.f derrlq.f derrls.f
    derrps.f derrql.f derrqp.f derrqr.f
    derrrq.f derrtr.f derrtz.f
    dgbt01.f dgbt02.f dgbt05.f dgelqs.f dgeqls.f dgeqrs.f
    dgerqs.f dget01.f dget02.f
    dget03.f dget04.f dget06.f dget07.f dgtt01.f dgtt02.f
    dgtt05.f dlaptm.f dlarhs.f dlatb4.f dlatb5.f dlattb.f dlattp.f
    dlattr.f dlavsp.f dlavsy.f dlqt01.f dlqt02.f
    dlqt03.f dpbt01.f dpbt02.f dpbt05.f dpot01.f
    dpot02.f dpot03.f dpot05.f dpst01.f dppt01.f
    dppt02.f dppt03.f dppt05.f dptt01.f dptt02.f
    dptt05.f dqlt01.f dqlt02.f dqlt03.f dqpt01.f
    dqrt01.f dqrt02.f dqrt03.f dqrt11.f dqrt12.f
    dqrt13.f dqrt14.f dqrt15.f dqrt16.f dqrt17.f
    drqt01.f drqt02.f drqt03.f drzt01.f drzt02.f
    dspt01.f dsyt01.f
    dtbt02.f dtbt03.f dtbt05.f dtbt06.f dtpt01.f
    dtpt02.f dtpt03.f dtpt05.f dtpt06.f dtrt01.f
    dtrt02.f dtrt03.f dtrt05.f dtrt06.f
    dtzt01.f dtzt02.f dgennd.f
    ####
    derrvx.f ddrvge.f ddrvgb.f derrge.f ddrvpo.f derrpo.f
   )

SET(ZLINTST
    zchkaa.f
    zchkeq.f zchkgb.f zchkge.f zchkgt.f
    zchklq.f zchkpb.f
    zchkpo.f zchkps.f zchkpp.f zchkpt.f zchkq3.f zchkql.f zchkqp.f
    zchkqr.f zchkrq.f zchktb.f
    zchktp.f zchktr.f zchktz.f
    zdrvgt.f zdrvhp.f
    zdrvls.f zdrvpb.f zdrvpp.f zdrvpt.f
    zdrvsp.f
    zerrgt.f zerrlq.f
    zerrls.f zerrps.f zerrql.f zerrqp.f
    zerrqr.f zerrrq.f zerrtr.f zerrtz.f
    zgbt01.f zgbt02.f zgbt05.f zgelqs.f zgeqls.f zgeqrs.f
    zgerqs.f zget01.f zget02.f
    zget03.f zget04.f zget07.f zgtt01.f zgtt02.f
    zgtt05.f zhet01.f zhpt01.f zlaipd.f zlaptm.f zlarhs.f zlatb4.f zlatb5.f
    zlatsp.f zlatsy.f zlattb.f zlattp.f zlattr.f
    zlavhe.f zlavhp.f zlavsp.f zlavsy.f zlqt01.f
    zlqt02.f zlqt03.f zpbt01.f zpbt02.f zpbt05.f
    zpot01.f zpot02.f zpot03.f zpot05.f zpst01.f
    zppt01.f zppt02.f zppt03.f zppt05.f zptt01.f
    zptt02.f zptt05.f zqlt01.f zqlt02.f zqlt03.f
    zqpt01.f zqrt01.f zqrt02.f zqrt03.f zqrt11.f
    zqrt12.f zqrt13.f zqrt14.f zqrt15.f zqrt16.f
    zqrt17.f zrqt01.f zrqt02.f zrqt03.f zrzt01.f zrzt02.f
    zsbmv.f  zspt01.f
    zspt02.f zspt03.f zsyt01.f zsyt02.f zsyt03.f
    ztbt02.f ztbt03.f ztbt05.f ztbt06.f ztpt01.f
    ztpt02.f ztpt03.f ztpt05.f ztpt06.f ztrt01.f
    ztrt02.f ztrt03.f ztrt05.f ztrt06.f
    ztzt01.f ztzt02.f dget06.f zgennd.f
    ####
    zerrvx.f zdrvge.f zdrvgb.f zerrge.f zdrvpo.f
	zerrpo.f 
   )

SET(DSLINTST
    dchkab.f
	ddrvab.f ddrvac.f derrab.f derrac.f dget08.f         
	alaerh.f alahd.f  aladhd.f alareq.f
	chkxer.f dlarhs.f dlatb4.f xerbla.f
	dget02.f dpot06.f
   )

SET(ZCLINTST
    zchkab.f
	zdrvab.f zdrvac.f zerrab.f zerrac.f zget08.f
	alaerh.f alahd.f  aladhd.f alareq.f
	chkxer.f zget02.f zlarhs.f zlatb4.f
	zsbmv.f  xerbla.f zpot06.f zlaipd.f
   )

SET(SLINTSTRFP
    schkrfp.f sdrvrfp.f sdrvrf1.f sdrvrf2.f sdrvrf3.f sdrvrf4.f serrrfp.f
	slatb4.f slarhs.f sget04.f spot01.f spot03.f spot02.f
	chkxer.f xerbla.f alaerh.f aladhd.f alahd.f alasvm.f 
   )

SET(DLINTSTRFP
    dchkrfp.f ddrvrfp.f ddrvrf1.f ddrvrf2.f ddrvrf3.f ddrvrf4.f derrrfp.f
	dlatb4.f dlarhs.f dget04.f dpot01.f dpot03.f dpot02.f
	chkxer.f xerbla.f alaerh.f aladhd.f alahd.f alasvm.f 
   )

SET(CLINTSTRFP
    cchkrfp.f cdrvrfp.f cdrvrf1.f cdrvrf2.f cdrvrf3.f cdrvrf4.f cerrrfp.f
	claipd.f clatb4.f clarhs.f csbmv.f cget04.f cpot01.f cpot03.f cpot02.f
	chkxer.f xerbla.f alaerh.f aladhd.f alahd.f alasvm.f 
   )

SET(ZLINTSTRFP
    zchkrfp.f zdrvrfp.f zdrvrf1.f zdrvrf2.f zdrvrf3.f zdrvrf4.f zerrrfp.f
	zlatb4.f zlaipd.f zlarhs.f zsbmv.f zget04.f zpot01.f zpot03.f zpot02.f
	chkxer.f xerbla.f alaerh.f aladhd.f alahd.f alasvm.f 
   )

IF(HAVE_PGI)
    SET(CUDA_INTERFACE
        ${CUDA_TOOLKIT_ROOT_DIR}/src/fortran_thunking.c
       )
ELSE()
    SET(CUDA_INTERFACE
        ${CUDA_TOOLKIT_ROOT_DIR}/src/fortran.c
       )
ENDIF()

# Define what libraries we have to link with
# ------------------------------------------
UNSET(libs_for_tests)
    
LIST(APPEND libs_for_tests magma)
LIST(APPEND libs_for_tests magmablas)
LIST(APPEND libs_for_tests magma)
LIST(APPEND libs_for_tests tmglib)
LIST(APPEND libs_for_tests "${CUDA_LIST_LDFLAGS}")

FOREACH(_dep LAPACK CBLAS BLAS TMG)
    IF(HAVE_${_dep})
        UNSET(${_dep}_path_flags)
        FOREACH(_path ${${_dep}_LIBRARY_PATH})
            LIST(APPEND ${_dep}_path_flags "-L${_path}")
        ENDFOREACH()
        LIST(APPEND libs_for_tests ${${_dep}_path_flags})
        LIST(APPEND libs_for_tests ${${_dep}_LIBRARY})
    ENDIF()
ENDFOREACH()
FOREACH(_dep DL)
    IF(HAVE_${_dep})
        LIST(APPEND libs_for_tests ${${_dep}_LIST_LDFLAGS})
    ENDIF()
ENDFOREACH()

LIST(APPEND libs_for_tests ${CMAKE_Fortran_FLAGS})
LIST(APPEND libs_for_tests ${CMAKE_Fortran_LDFLAGS})
LIST(APPEND libs_for_tests ${MAGMA_EXTRA_LIBS})

# Copy launcher
# -------------
ADD_CUSTOM_TARGET(lin_launcher ALL
                  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/lapack_testing.py
                                                   ${CMAKE_CURRENT_BINARY_DIR}/lapack_testing.py
                  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/stest.in
                                                   ${CMAKE_CURRENT_BINARY_DIR}/stest.in
                  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/dtest.in
                                                   ${CMAKE_CURRENT_BINARY_DIR}/dtest.in
                  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/ctest.in
                                                   ${CMAKE_CURRENT_BINARY_DIR}/ctest.in
                  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/ztest.in
                                                   ${CMAKE_CURRENT_BINARY_DIR}/ztest.in
                 )

# Create tests
# ------------
IF(BUILD_SINGLE)
    ADD_EXECUTABLE(xlintsts ${ALINTST} ${SLINTST} ${SCLNTST} ${CUDA_INTERFACE})
    SET_PROPERTY(TARGET xlintsts PROPERTY LINKER_LANGUAGE Fortran)
    TARGET_LINK_LIBRARIES(xlintsts ${libs_for_tests})
    INSTALL(TARGETS xlintsts DESTINATION ${CMAKE_INSTALL_PREFIX}/testing/lin)
    INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/stest.in
            DESTINATION ${CMAKE_INSTALL_PREFIX}/testing/lin)
ENDIF()

IF(BUILD_DOUBLE)
    ADD_EXECUTABLE(xlintstd ${ALINTST} ${DLINTST} ${DZLNTST} ${CUDA_INTERFACE})
    SET_PROPERTY(TARGET xlintstd PROPERTY LINKER_LANGUAGE Fortran)
    TARGET_LINK_LIBRARIES(xlintstd ${libs_for_tests})
    INSTALL(TARGETS xlintstd DESTINATION ${CMAKE_INSTALL_PREFIX}/testing/lin)
    INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/dtest.in
            DESTINATION ${CMAKE_INSTALL_PREFIX}/testing/lin)
ENDIF()

IF(BUILD_COMPLEX)
    ADD_EXECUTABLE(xlintstc ${ALINTST} ${CLINTST} ${SCLNTST} ${CUDA_INTERFACE})
    SET_PROPERTY(TARGET xlintstc PROPERTY LINKER_LANGUAGE Fortran)
    TARGET_LINK_LIBRARIES(xlintstc ${libs_for_tests})
    INSTALL(TARGETS xlintstc DESTINATION ${CMAKE_INSTALL_PREFIX}/testing/lin)
    INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/ctest.in
            DESTINATION ${CMAKE_INSTALL_PREFIX}/testing/lin)
ENDIF()

IF(BUILD_COMPLEX16)
    ADD_EXECUTABLE(xlintstz ${ALINTST} ${ZLINTST} ${DZLNTST} ${CUDA_INTERFACE})
    SET_PROPERTY(TARGET xlintstz PROPERTY LINKER_LANGUAGE Fortran)
    TARGET_LINK_LIBRARIES(xlintstz ${libs_for_tests})
    INSTALL(TARGETS xlintstz DESTINATION ${CMAKE_INSTALL_PREFIX}/testing/lin)
    INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/ztest.in
            DESTINATION ${CMAKE_INSTALL_PREFIX}/testing/lin)
ENDIF()

# Add tests (C/CPP)
# -----------------
ADD_TEST(NAME    lapack_testing
         COMMAND ${PYTHON_EXECUTABLE}
                 ${CMAKE_CURRENT_BINARY_DIR}/lapack_testing.py)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/lapack_testing.py
        DESTINATION ${CMAKE_INSTALL_PREFIX}/testing/lin)


###
### END CMakeLists.txt
###
