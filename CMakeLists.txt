#DeviceVirtual

cmake_minimum_required(VERSION 3.30)

set(Project DeviceVirtual)
project(${Project})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LIB_VERSION_MAJOR 1)
set(LIB_VERSION_MINOR 0)
set(LIB_VERSION_PATCH 0)

if(UNIX AND NOT APPLE)
    if(${CMAKE_SYSTEM_NAME} MATCHES "Android")
        message("-- Configuring for Android ...")
        set(ANDROID_LINUX TRUE)
    elseif(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
        message("-- Configuring for FreeBSD ...")
        set(FREEBSD TRUE)
    else()
        message("-- Configuring for GNU/Linux ...")
        set(GNU_LINUX TRUE)
    endif()
endif()
if(UNIX AND APPLE)
    message("-- Configuring for Darwin (MacOS, iOS) ...")
    set(DARWIN TRUE)
endif()

if(WIN32)
	message("-- Configuring for Windows ...")
endif()


set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(PROJECT_DEVICEVIRTUAL_CORE_SOURCE_DIR ./core/src)
set(PROJECT_DEVICEVIRTUAL_CORE_INCLUDE_DIR ./core/include)
set(PROJECT_DEVICEVIRTUAL_NET_SOURCE_DIR ./network/src)
set(PROJECT_DEVICEVIRTUAL_NET_INCLUDE_DIR ./network/include)

if(WIN32)
include_directories(${PROJECT_DEVICEVIRTUAL_CORE_INCLUDE_DIR} ${PROJECT_DEVICEVIRTUAL_NET_INCLUDE_DIR} C:/msys64/mingw64/include/)
link_libraries(ws2_32)
link_directories(C:/msys64/mingw64/lib/)
else()
include_directories(${PROJECT_DEVICEVIRTUAL_CORE_INCLUDE_DIR} ${PROJECT_DEVICEVIRTUAL_NET_INCLUDE_DIR} /usr/include/ /usr/local/include/)
link_libraries(rt pthread dl)
link_directories(/usr/local/lib/ /usr/lib/ /lib/)
endif()

set(CORE_SOURCES
${CORE_SOURCES}
${PROJECT_DEVICEVIRTUAL_CORE_SOURCE_DIR}/StringExtensions.cpp
${PROJECT_DEVICEVIRTUAL_CORE_SOURCE_DIR}/Variant.cpp
${PROJECT_DEVICEVIRTUAL_CORE_SOURCE_DIR}/Base64.cpp
${PROJECT_DEVICEVIRTUAL_CORE_SOURCE_DIR}/DateTime.cpp
${PROJECT_DEVICEVIRTUAL_CORE_SOURCE_DIR}/Configuration.cpp
${PROJECT_DEVICEVIRTUAL_CORE_SOURCE_DIR}/Logger.cpp
${PROJECT_DEVICEVIRTUAL_CORE_SOURCE_DIR}/SignalHandler.cpp
)

set(CORE_HEADERS
${CORE_HEADERS}
${PROJECT_DEVICEVIRTUAL_CORE_INCLUDE_DIR}/StringExtensions.hpp
${PROJECT_DEVICEVIRTUAL_CORE_INCLUDE_DIR}/Variant.hpp
${PROJECT_DEVICEVIRTUAL_CORE_INCLUDE_DIR}/Base64.hpp
${PROJECT_DEVICEVIRTUAL_CORE_INCLUDE_DIR}/DateTime.hpp
${PROJECT_DEVICEVIRTUAL_CORE_INCLUDE_DIR}/Configuration.hpp
${PROJECT_DEVICEVIRTUAL_CORE_INCLUDE_DIR}/Logger.hpp
${PROJECT_DEVICEVIRTUAL_CORE_INCLUDE_DIR}/SignalHandler.hpp)

set(NET_SOURCES
${NET_SOURCES}
)

set(NET_HEADERS
${NET_HEADERS}
${PROJECT_DEVICEVIRTUAL_NET_INCLUDE_DIR}/Mqtt.hpp
)

if(WIN32)
    set(NET_SOURCES
    ${NET_SOURCES}
    ${PROJECT_DEVICEVIRTUAL_NET_SOURCE_DIR}/MqttWin32.cpp
    )
else()
    set(NET_SOURCES
    ${NET_SOURCES}
    ${PROJECT_DEVICEVIRTUAL_NET_SOURCE_DIR}/MqttPosix.cpp
    )
endif()


add_executable("VirtualSensors" ./src/VirtualSensors.cpp ${CORE_SOURCES} ${CORE_HEADERS} ${NET_SOURCES} ${NET_HEADERS})
add_executable("VirtualActuators" ./src/VirtualActuators.cpp ${CORE_SOURCES} ${CORE_HEADERS} ${NET_SOURCES} ${NET_HEADERS})



