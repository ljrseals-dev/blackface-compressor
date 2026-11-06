cmake_minimum_required(VERSION 3.20)
project(Blackface VERSION 1.0.0 LANGUAGES CXX)

# Steinberg VST3 SDK 3.7.8 or newer (needs VSTGUI 4.11+). Download: https://www.steinberg.net/developers/
set(VST3_SDK_ROOT "" CACHE PATH "Path to the VST3 SDK (folder with pluginterfaces, public.sdk, vstgui4)")
if(NOT VST3_SDK_ROOT OR NOT EXISTS "${VST3_SDK_ROOT}/CMakeLists.txt")
  message(FATAL_ERROR "Set -DVST3_SDK_ROOT=/path/to/VST3_SDK")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(SMTG_ENABLE_VSTGUI_SUPPORT ON CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
add_subdirectory(${VST3_SDK_ROOT} ${PROJECT_BINARY_DIR}/vst3sdk)
smtg_enable_vst3_sdk()

smtg_add_vst3plugin(Blackface
  source/BlackfaceEntry.cpp
  source/BlackfaceProcessor.cpp
  source/BlackfaceController.cpp
  source/BlackfaceViews.cpp)
target_include_directories(Blackface PRIVATE source source/dsp)
target_link_libraries(Blackface PRIVATE sdk vstgui_support)

set(BF_RESOURCES
  resource/blackface.uidesc
  resource/panel.png resource/panel@2x.png
  resource/knob_big.png resource/knob_big@2x.png
  resource/knob_small.png resource/knob_small@2x.png
  resource/knob_mini.png resource/knob_mini@2x.png
  resource/vu_face.png resource/vu_face@2x.png
  resource/vu_glass.png resource/vu_glass@2x.png)
smtg_target_add_plugin_resources(Blackface RESOURCES ${BF_RESOURCES})

if(SMTG_MAC)
  smtg_target_set_bundle(Blackface BUNDLE_IDENTIFIER com.therealla.blackface COMPANY_NAME "theRealla")
endif()

# Host-less DSP test (no SDK needed):  cmake --build . --target blackface_tests && ctest
add_executable(blackface_tests tests/engine_test.cpp)
target_include_directories(blackface_tests PRIVATE source source/dsp)
