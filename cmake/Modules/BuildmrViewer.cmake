include(ExternalProject)


set(mrViewer_ARGS
  ${TLRENDER_EXTERNAL_ARGS}
  -DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}
  -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
  -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
  -DTLRENDER_ENABLE_MMAP=${TLRENDER_ENABLE_MMAP}
  -DTLRENDER_ENABLE_PYTHON=${TLRENDER_ENABLE_PYTHON}
  -DTLRENDER_BUILD_GL=ON
  -DTLRENDER_BUILD_BMD=${TLRENDER_BUILD_BMD}
  -DTLRENDER_BMD_SDK=${TLRENDER_BMD_SDK})

ExternalProject_Add(
    mrViewer2
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/mrViewer
    DEPENDS tlRender FLTK BOOST
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/mrViewer
    LIST_SEPARATOR |
    CMAKE_ARGS ${mrViewer_ARGS})
