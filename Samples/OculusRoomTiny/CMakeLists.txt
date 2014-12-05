project(OculusRoomTiny)

file(GLOB SOURCE_FILES *.cpp *.h)
list(APPEND EXTRA_LIBS OculusVR)

if (WIN32)
    find_package( DirectX REQUIRED)
    list(APPEND EXTRA_LIBS ${DirectX_D3D10_LIBRARY})
    list(APPEND EXTRA_LIBS ${DirectX_D3DX10_LIBRARY})
    list(APPEND EXTRA_LIBS ${DirectX_D3D10_1_LIBRARY})
    list(APPEND EXTRA_LIBS ${DirectX_D3D11_LIBRARY})
    list(APPEND EXTRA_LIBS ${DirectX_D3DX11_LIBRARY})
    list(APPEND EXTRA_LIBS d3dcompiler dxgi dxguid)
endif()

add_executable(${PROJECT_NAME} WIN32 ${SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} ${EXTRA_LIBS})



