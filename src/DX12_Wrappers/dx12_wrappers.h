#pragma once

#include <wrl/client.h>

#include <cstdint>

struct FDX12Context
{
    bool Initialize(void* Win32Handle, uint32_t Width, uint32_t Height);
    void Shutdown();

    void Dispatch(uint32_t X, uint32_t Y, uint32_t Z);
    void Present();

    void WaitIdle();

private:
    constexpr static uint32_t FrameCount = 2;

    uint32_t Width = 0;
    uint32_t Height = 0;
    uint32_t CurrentFrameIndex = 0;

    Microsoft::WRL::ComPtr<IDXGIFactory6> DxgiFactory;
    Microsoft::WRL::ComPtr<ID3D12Device> Device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> CommandQueue;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> SwapChain3;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> CommandList;
    Microsoft::WRL::ComPtr<ID3D12Fence> Fence;
    Microsoft::WRL::ComPtr<ID3D12Resource> BackBuffers[FrameCount];
    Microsoft::WRL::ComPtr<ID3D12RootSignature> RootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> PSO;
    Microsoft::WRL::ComPtr<ID3D12Resource> OutputTexture;

    UINT64 FenceValue = 0;
    HANDLE FenceEvent = nullptr;
};