cmake_minimum_required(VERSION 3.20)
project(niro_official_replay LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(S ${CMAKE_CURRENT_SOURCE_DIR}/source)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations -DCL_SILENCE_DEPRECATION -DGL_SILENCE_DEPRECATION")
include_directories(${S} ${S}/msgq ${S}/third_party ${S}/third_party/libyuv/include /opt/homebrew/include)
link_directories(/opt/homebrew/lib /opt/homebrew/opt/ffmpeg/lib /opt/homebrew/opt/openssl@3/lib /opt/homebrew/opt/curl/lib)
include_directories(/opt/homebrew/opt/ffmpeg/include /opt/homebrew/opt/openssl@3/include /opt/homebrew/opt/curl/include)
file(GLOB REPLAY ${S}/tools/replay/*.cc)
list(FILTER REPLAY EXCLUDE REGEX "tests/")
file(GLOB SCHEMA ${S}/cereal/gen/cpp/*.c++)
set(MSG ${S}/msgq_repo/msgq)
add_executable(replay ${REPLAY} ${SCHEMA}
 ${S}/common/params.cc ${S}/common/swaglog.cc ${S}/common/util.cc ${S}/common/watchdog.cc ${S}/common/ratekeeper.cc
 ${S}/third_party/json11/json11.cpp ${S}/cereal/messaging/socketmaster.cc
 ${MSG}/ipc.cc ${MSG}/event.cc ${MSG}/impl_zmq.cc ${MSG}/impl_msgq.cc ${MSG}/impl_fake.cc ${MSG}/msgq.cc
 ${MSG}/visionipc/visionipc.cc ${MSG}/visionipc/visionipc_server.cc ${MSG}/visionipc/visionipc_client.cc ${MSG}/visionipc/visionbuf.cc ${MSG}/visionipc/visionbuf_cl.cc)
target_link_libraries(replay capnp kj zmq avutil avcodec avformat bz2 zstd curl ssl crypto ncurses ${CMAKE_CURRENT_SOURCE_DIR}/yuv-build/libyuv.a "-framework OpenCL")

target_compile_options(replay PRIVATE -UNDEBUG)
