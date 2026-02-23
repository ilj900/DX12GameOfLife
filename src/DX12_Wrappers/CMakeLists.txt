find_library(D3D12_LIB d3d12)
find_library(DXGI_LIB dxgi)
find_library(DXCOMPILER_LIB dxcompiler)

set(SRC dx12_wrappers.cpp)

add_library(DX12_Wrappers STATIC ${SRC})

target_include_directories(DX12_Wrappers PUBLIC .)

target_link_libraries(DX12_Wrappers PUBLIC ${D3D12_LIB} ${DXGI_LIB} ${DXCOMPILER_LIB})

if (MSVC)
    target_compile_definitions(DX12_Wrappers PRIVATE _WIN32_WINNT=0x0A00)
endif()
