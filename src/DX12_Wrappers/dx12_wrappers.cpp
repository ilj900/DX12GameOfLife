#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <wrl.h>

#include "dx12_wrappers.h"

#include <future>

using Microsoft::WRL::ComPtr;

bool FDX12Context::Initialize(void* Win32Handle, uint32_t Width, uint32_t Height)
{
    HRESULT HR = S_OK;

    /// Create Device
    HR = CreateDXGIFactory(IID_PPV_ARGS(&DxgiFactory));

    HR = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&Device));

    /// Create Command Queue
    D3D12_COMMAND_QUEUE_DESC Desc = {};
    Desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    HR = Device->CreateCommandQueue(&Desc, IID_PPV_ARGS(&CommandQueue));

    /// Create Swapchain
    DXGI_SWAP_CHAIN_DESC1 SwapChainDesc = {};
    SwapChainDesc.BufferCount = 2;
    SwapChainDesc.Width = Width;
    SwapChainDesc.Height = Height;
    SwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    SwapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> SwapChain1;
    HR = DxgiFactory->CreateSwapChainForHwnd(CommandQueue.Get(), static_cast<HWND>(Win32Handle), &SwapChainDesc, nullptr, nullptr, &SwapChain1);

    HR = SwapChain1.As(&SwapChain3);
    CurrentFrameIndex = SwapChain3->GetCurrentBackBufferIndex();

    for (int i = 0; i < FrameCount; i++)
    {
        HR = SwapChain3->GetBuffer(i, IID_PPV_ARGS(&BackBuffers[i]));
    }

    /// Create Command Allocator and Command List
    HR = Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CommandAllocator));

    HR = Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&CommandList));
    HR = CommandList->Close();

    HR = Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence));
    FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    return SUCCEEDED(HR);
}

void FDX12Context::Shutdown()
{
    WaitIdle();

    CloseHandle(FenceEvent);
    FenceEvent = nullptr;
}

void FDX12Context::Dispatch(uint32_t X, uint32_t Y, uint32_t Z)
{
    HRESULT HR = S_OK;

    HR = CommandAllocator->Reset();
    HR = CommandList->Reset(CommandAllocator.Get(), PSO.Get());

    CommandList->SetComputeRootSignature(RootSignature.Get());

    CommandList->SetComputeRootUnorderedAccessView(0, OutputTexture->GetGPUVirtualAddress());

    CommandList->Dispatch(X, Y, Z);

    D3D12_RESOURCE_BARRIER Barrier = {};
    Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    Barrier.Transition.pResource = BackBuffers[CurrentFrameIndex].Get();
    Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    CommandList->ResourceBarrier(1, &Barrier);

    CommandList->CopyResource(BackBuffers[CurrentFrameIndex].Get(), OutputTexture.Get());
    std::swap(Barrier.Transition.StateBefore, Barrier.Transition.StateAfter);
    CommandList->ResourceBarrier(1, &Barrier);

    HR = CommandList->Close();
    ID3D12CommandList* Lists[] = {CommandList.Get()};
    CommandQueue->ExecuteCommandLists(1, Lists);
}

void FDX12Context::Present()
{
    HRESULT HR = S_OK;

    HR = SwapChain3->Present(1, 0);

    FenceValue++;
    HR = CommandQueue->Signal(Fence.Get(), FenceValue);

    if (Fence->GetCompletedValue() < FenceValue)
    {
        HR = Fence->SetEventOnCompletion(FenceValue, FenceEvent);
        WaitForSingleObject(FenceEvent, INFINITE);
    }

    CurrentFrameIndex = SwapChain3->GetCurrentBackBufferIndex();
}

void FDX12Context::WaitIdle()
{
    HRESULT HR = S_OK;
    FenceValue++;
    HR = CommandQueue->Signal(Fence.Get(), FenceValue);

    if (Fence->GetCompletedValue() < FenceValue)
    {
        HR = Fence->SetEventOnCompletion(FenceValue, FenceEvent);
        WaitForSingleObject(FenceEvent, INFINITE);
    }
}
