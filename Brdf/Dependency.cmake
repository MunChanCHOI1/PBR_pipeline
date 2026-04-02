include(ExternalProject)

set(DEP_INSTALL_DIR ${PROJECT_BINARY_DIR}/install)
set(DEP_INCLUDE_DIR ${DEP_INSTALL_DIR}/include)
set(DEP_LIB_DIR ${DEP_INSTALL_DIR}/lib)

# 1. spdlog
ExternalProject_Add(
    dep_spdlog
    GIT_REPOSITORY "https://github.com/gabime/spdlog.git"
    GIT_TAG "v1.x"
    GIT_SHALLOW 1
    CMAKE_ARGS 
        -DCMAKE_INSTALL_PREFIX=${DEP_INSTALL_DIR}
        -DSPDLOG_BUILD_EXAMPLE=OFF
        -DSPDLOG_BUILD_TESTS=OFF
)
set(DEP_LIST ${DEP_LIST} dep_spdlog)


# 2. stb (디렉토리 생성 명령 분리)
ExternalProject_Add(
    dep_stb
    GIT_REPOSITORY "https://github.com/nothings/stb"
    GIT_TAG "master"
    GIT_SHALLOW 1
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    TEST_COMMAND ""
    INSTALL_COMMAND ${CMAKE_COMMAND} -E make_directory ${DEP_INSTALL_DIR}/include/stb
            COMMAND ${CMAKE_COMMAND} -E copy
            ${PROJECT_BINARY_DIR}/dep_stb-prefix/src/dep_stb/stb_image.h
            ${DEP_INSTALL_DIR}/include/stb/stb_image.h
)
set(DEP_LIST ${DEP_LIST} dep_stb)

# 3. GLM (디렉토리 생성 명령 분리)
ExternalProject_Add(
    dep_glm
    GIT_REPOSITORY "https://github.com/g-truc/glm"
    GIT_TAG "1.0.1"
    GIT_SHALLOW 1
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    TEST_COMMAND ""
    INSTALL_COMMAND ${CMAKE_COMMAND} -E make_directory ${DEP_INSTALL_DIR}/include/glm
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${PROJECT_BINARY_DIR}/dep_glm-prefix/src/dep_glm/glm
            ${DEP_INSTALL_DIR}/include/glm
)
set(DEP_LIST ${DEP_LIST} dep_glm)

# 4. Assimp
ExternalProject_Add(
    dep_assimp
    GIT_REPOSITORY "https://github.com/assimp/assimp.git"
    GIT_TAG "v5.3.1"
    GIT_SHALLOW 1
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${DEP_INSTALL_DIR}
        -DCMAKE_BUILD_TYPE=Debug
        -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDebugDLL
        -DASSIMP_BUILD_TESTS=OFF
        -DASSIMP_BUILD_ASSIMP_TOOLS=OFF
        -DASSIMP_BUILD_SAMPLES=OFF
        -DASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT=OFF
        -DASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT=OFF
        -DASSIMP_BUILD_ZLIB=ON
        -DASSIMP_BUILD_OBJ_IMPORTER=ON
        -DASSIMP_BUILD_FBX_IMPORTER=ON
        -DASSIMP_INJECT_DEBUG_POSTFIX=OFF
        -DBUILD_SHARED_LIBS=OFF
        -DASSIMP_WARNINGS_AS_ERRORS=OFF
        -DCMAKE_PDB_OUTPUT_DIRECTORY_DEBUG=${PROJECT_BINARY_DIR}/dep_assimp-prefix/src/dep_assimp-build/code
)
set(DEP_LIBS ${DEP_LIBS} assimp-vc143-mt zlibstaticd)
set(DEP_LIST ${DEP_LIST} dep_assimp)


# Dependency.cmake 마지막 부분
add_dependencies(${PROJECT_NAME} ${DEP_LIST})