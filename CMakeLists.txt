cmake_minimum_required(VERSION 3.10)
project(openups CXX C)

set(SOURCES 
src/main.cpp
src/lib/usbhid.cpp
src/lib/HIDOpenUPSBase.cpp
src/lib/HIDOpenUPS.cpp
src/lib/HIDOpenUPS2.cpp
src/lib/HIDNUCUPS.cpp
src/lib/HIDDCDCUSB.cpp
src/lib/HIDInterface.cpp
src/utils/util.cpp
)

option(_DEBUG "Debug messages" OFF)
if (_DEBUG)
  add_definitions(-D_DEBUG)
endif()

add_executable(openups ${SOURCES})


target_include_directories(openups PUBLIC src/utils/ src/lib/)
target_link_libraries(openups
usb
)

install(
  TARGETS openups
  DESTINATION bin
  )

install(FILES udev/70-openups.rules DESTINATION lib/udev/rules.d)

# Snap Build
# snapcraft clean
# snapcraft
# snapcraft upload --release=beta openups_0.4_amd64.snap