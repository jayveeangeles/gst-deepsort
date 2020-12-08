cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(deepsort-plugin LANGUAGES CXX)

EXECUTE_PROCESS( COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE ARCHITECTURE )
MESSAGE("Detected ${ARCHITECTURE} architecture")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -std=c++17 -Wall -Wunused-function -Wunused-variable -Wfatal-errors")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -ggdb -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined")

set(OPENCV_ROOT "" CACHE PATH "OpenCV SDK root path")

find_package(PkgConfig)
pkg_check_modules(GST REQUIRED gstreamer-1.0>=1.8 gstreamer-video-1.0>=1.8 gstreamer-base-1.0>=1.8)

# find_package(CUDA REQUIRED cudart cublas curand)
list(APPEND GPU_ARCHS 70 75 86)

# Generate SASS for each architecture
foreach(arch ${GPU_ARCHS})
  set(GENCODES "${GENCODES} -gencode arch=compute_${arch},code=sm_${arch}")
endforeach()

# Generate PTX for the last architecture
list(GET GPU_ARCHS -1 LATEST_GPU_ARCH)
set(GENCODES "${GENCODES} -gencode arch=compute_${LATEST_GPU_ARCH},code=compute_${LATEST_GPU_ARCH}")

find_package(Eigen3 REQUIRED)

# Find OpenCV 
find_package(OpenCV REQUIRED core imgproc imgcodecs highgui PATHS ${OPENCV_ROOT} ${CMAKE_SYSTEM_PREFIX_PATH} PATH_SUFFIXES build share NO_DEFAULT_PATH)
find_package(OpenCV REQUIRED core imgproc imgcodecs highgui)

if(NOT DEFINED TENSORFLOW_ROOT_DIR)
  set(TENSORFLOW_ROOT_DIR "/usr/local/lib/./python3.6/dist-packages/tensorflow_core")
endif()

if(NOT DEFINED TENSORFLOWCC_ROOT_DIR)
  set(TENSORFLOWCC_ROOT_DIR "/usr/local/lib/tensorflow")
endif()

if(NOT DEFINED DEEPSORT_ROOT_DIR)
  set(DEEPSORT_ROOT_DIR "/workspace/deep_sort")
endif()

include_directories(
  # ${CUDA_TOOLKIT_INCLUDE}
  ${GST_INCLUDE_DIRS}
  ${DEEPSORT_ROOT_DIR}/src/app/include
  ${DEEPSORT_ROOT_DIR}/src/features/include
  ${DEEPSORT_ROOT_DIR}/src/matching/include
  ${DEEPSORT_ROOT_DIR}/src/thirdPart/include
  ${EIGEN3_INCLUDE_DIR}
  /usr/include/gstreamer-1.0/gst
  ${TENSORFLOW_ROOT_DIR}/./include)

link_directories(${TENSORFLOW_ROOT_DIR})
link_directories(${TENSORFLOWCC_ROOT_DIR})
link_directories(${DEEPSORT_ROOT_DIR}/./lib/)

file(GLOB_RECURSE SRC_FILES "./*.cpp")

add_library(gstdeepsortplugin SHARED ${SRC_FILES})
set_target_properties(gstdeepsortplugin PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
target_link_libraries(gstdeepsortplugin deepsort ${GST_LIBRARIES} gst_detectionmetas gst_trackedmetas ${OpenCV_LIBS} tensorflow_cc tensorflow_framework) 

#Install library
install(TARGETS gstdeepsortplugin LIBRARY DESTINATION "/usr/lib/${ARCHITECTURE}-linux-gnu/gstreamer-1.0/" CONFIGURATIONS Release Debug)