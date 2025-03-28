cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(BC2.elf C CXX)
include("${VITASDK}/share/vita.cmake" REQUIRED)
set(VITA_APP_NAME "DuckTales")
set(VITA_TITLEID  "DUK000000")
set(VITA_VERSION  "01.00")
set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -d ATTRIBUTE2=12")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wl,-q -Wall -Wextra -O3 -mfloat-abi=softfp")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++11")
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,stack-size=65536")
add_definitions(-D__psp2__)
add_executable(BC2.elf
  loader/main.c
  loader/jni_patch.c
  loader/dialog.c
  loader/so_util.c
  loader/pthr.c
  loader/etc1_utils.c
)

target_link_libraries(BC2.elf
  
  taihen_stub
  # pib
  # libScePiglet_stub
  
  # vitaGL
  # vitashark
  
  # libGLESv2
  # apitrace
  libGLESv2_stub
    libIMGEGL_stub
    libgpu_es4_ext_stub
    
    m
    # SDL2
    libfmodex_stub
      SceNgsUser_stub
      SceAudiodec_stub
      SceAudioIn_stub
    # SceFpu_stub
    # c_stub
    # SceLibc_stub
    gcc
    stdc++
  SceShaccCgExt
  
  mathneon
  kubridge_stub
  SceAppMgr_stub
  SceAppUtil_stub
  pthread
  SceAudio_stub
  SceCtrl_stub
  
  SceCommonDialog_stub
  SceDisplay_stub
  SceKernelDmacMgr_stub
  SceGxm_stub
  SceShaccCg_stub
  SceSysmodule_stub
  ScePower_stub
  SceTouch_stub
  SceVshBridge_stub
)

vita_create_self(eboot.bin BC2.elf UNSAFE NOASLR)
vita_create_vpk(DuckTales.vpk ${VITA_TITLEID} eboot.bin
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}

)

add_custom_target(copy
  COMMAND cp eboot.bin E:/app/${VITA_TITLEID}/eboot.bin
  DEPENDS eboot.bin
)
