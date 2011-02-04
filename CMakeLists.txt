cmake_minimum_required (VERSION 2.8)
Project ( libcuzmem )

# Generate deb with
# cpack -G DEB <source tree>

## DEB PACKAGE GENERATION STUFF ########################
set (CPACK_DEBIAN_PACKAGE_NAME "libcuzmem")
set (CPACK_DEBIAN_PACKAGE_ARCHITECTURE "i386")
#set (CPACK_DEBIAN_PACKAGE_DEPENDS "libsdl1.2debian (>=1.2.14-4ubuntu1.1)")
set (CPACK_PACKAGE_CONTACT "tshack@retro-box.com")
set (CPACK_DEBIAN_PACKAGE_DESCRIPTION "libcuzmem\n Allows for runtime tuning of a CUDA program's zero-copy utilization.")
set (CPACK_DEBIAN_PACKAGE_SECTION "devel")
########################################################

## GENERAL PACKAGE STUFF ###############################
SET (CPACK_PACKAGE_VERSION_MAJOR "0")
SET (CPACK_PACKAGE_VERSION_MINOR "1")
SET (CPACK_PACKAGE_VERSION_PATCH "0-dev")
INCLUDE (CPack)
########################################################

FIND_PACKAGE (CUDA REQUIRED)
IF (CUDA_FOUND)
    CUDA_INCLUDE_DIRECTORIES (
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # Don't link against CUDA runtime
    SET (CUDA_LIBRARIES "")
ENDIF (CUDA_FOUND)


## BUILD TARGET SOURCE FILES ###########################
SET ( SRC_LIBCUZMEM
    libcuzmem.cu
    plans.c
    tuner_exhaust.c
)

SET ( SRC_TEST
    test.c
    plans.c
    context.c
)

########################################################


## BUILD TARGETS #######################################
CUDA_ADD_LIBRARY ( cuzmem SHARED
    ${SRC_LIBCUZMEM}
)

ADD_EXECUTABLE ( test
    ${SRC_TEST}
)
########################################################


## INSTALL TARGETS (used by CPack) #####################
#INSTALL (
#    TARGETS libcuzmem
#    LIBRARY DESTINATION lib
#)
########################################################

