#include "dx12_wrappers.h"

#include <future>
#include <random>

using Microsoft::WRL::ComPtr;

#define CHECK_RESULT() CHECH(HR)

void CHECH(HRESULT HR)
{
    if (HR == S_OK) return;

    char* Msg = nullptr;

    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        HR,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
        (LPSTR)&Msg,
        0,
        nullptr);

    if (Msg)
    {
        OutputDebugStringA(Msg);
        LocalFree(Msg);
    }
}

bool FDX12Context::Initialize(void* Win32Handle, uint32_t Width, uint32_t Height)
{
    HRESULT HR = S_OK;

    this->Width = Width;
    this->Height = Height;

    /// Enable debug
    if (bDebug && SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&DebugController))))
    {
        DebugController->EnableDebugLayer();
    }

    /// Create Device

    UINT Flags = 0;
    if (bDebug) Flags |= DXGI_CREATE_FACTORY_DEBUG;
    HR = CreateDXGIFactory2(Flags, IID_PPV_ARGS(&DxgiFactory)); CHECK_RESULT();

    HR = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&Device)); CHECK_RESULT();

    /// Create Command Queue
    D3D12_COMMAND_QUEUE_DESC Desc = {};
    Desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    HR = Device->CreateCommandQueue(&Desc, IID_PPV_ARGS(&CommandQueue)); CHECK_RESULT();

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
    HR = DxgiFactory->CreateSwapChainForHwnd(CommandQueue.Get(), static_cast<HWND>(Win32Handle), &SwapChainDesc, nullptr, nullptr, &SwapChain1); CHECK_RESULT();

    HR = SwapChain1.As(&SwapChain3); CHECK_RESULT();
    CurrentFrameIndex = SwapChain3->GetCurrentBackBufferIndex();

    for (int i = 0; i < FrameCount; i++)
    {
        HR = SwapChain3->GetBuffer(i, IID_PPV_ARGS(&BackBuffers[i])); CHECK_RESULT();
    }

    /// Create Command Allocator and Command List
    HR = Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CommandAllocator)); CHECK_RESULT();

    HR = Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&CommandList)); CHECK_RESULT();
    HR = CommandList->Close(); CHECK_RESULT();

    HR = Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence)); CHECK_RESULT();
    FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    D3D12_DESCRIPTOR_HEAP_DESC HeapDescriptor = {};
    HeapDescriptor.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    HeapDescriptor.NumDescriptors = 3;
    HeapDescriptor.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    HR = Device->CreateDescriptorHeap(&HeapDescriptor, IID_PPV_ARGS(&UAVHeap)); CHECK_RESULT();

    if ((Width * Height) % 32 != 0)
    {
        throw std::runtime_error("Width * height must be a multiple of 32");
    }

    /// Allocate buffers
    /// We pack 32 values into one uin32_t
    uint32_t CellCount = Width * Height;
    uint32_t SizeInBytes = CellCount / 8;
    uint32_t SizeInUin32 = CellCount / 32;
    D3D12_HEAP_PROPERTIES HeapProperties = {};
    HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC BufferDesc = {};
    BufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    BufferDesc.Width = SizeInBytes;
    BufferDesc.Height = 1;
    BufferDesc.DepthOrArraySize = 1;
    BufferDesc.MipLevels = 1;
    BufferDesc.SampleDesc.Count = 1;
    BufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    BufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    for (auto & CellBuffer : CellBuffers)
    {
        HR = Device->CreateCommittedResource(&HeapProperties, D3D12_HEAP_FLAG_NONE, &BufferDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&CellBuffer)); CHECK_RESULT();
    }

    /// Fill the initial buffer
    std::vector<uint32_t> InitialState(SizeInUin32);
    std::mt19937 Generator(0);
    std::bernoulli_distribution AliveDistribution(0.2);
    for (uint32_t& PackedCells : InitialState)
    {
        PackedCells = 0;
        for (uint32_t Bit = 0; Bit < 32; Bit++)
        {
            if (AliveDistribution(Generator))
            {
                PackedCells |= (1u << Bit);
            }
        }
    }

    /// Upload data
    D3D12_HEAP_PROPERTIES UploadProps = {};
    UploadProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    ComPtr<ID3D12Resource> UploadBuffer;
    HR = Device->CreateCommittedResource(&UploadProps, D3D12_HEAP_FLAG_NONE, &BufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&UploadBuffer)); CHECK_RESULT();

    void* MappedData = nullptr;
    D3D12_RANGE ReadRange = {0, 0};
    HR = UploadBuffer->Map(0, &ReadRange, &MappedData); CHECK_RESULT();
    std::memcpy(MappedData, InitialState.data(), SizeInBytes);
    UploadBuffer->Unmap(0, nullptr);

    HR = CommandAllocator->Reset(); CHECK_RESULT();
    HR = CommandList->Reset(CommandAllocator.Get(), nullptr); CHECK_RESULT();
    CommandList->CopyResource(CellBuffers[0].Get(), UploadBuffer.Get());
    HR = CommandList->Close(); CHECK_RESULT();

    ID3D12CommandList* CommandLists[] = { CommandList.Get() };
    CommandQueue->ExecuteCommandLists(1, CommandLists);
    WaitIdle();

    /// Create the output texture
    D3D12_RESOURCE_DESC TextureDesc = {};
    TextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    TextureDesc.Width = Width;
    TextureDesc.Height = Height;
    TextureDesc.DepthOrArraySize = 1;
    TextureDesc.MipLevels = 1;
    TextureDesc.SampleDesc.Count = 1;
    TextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    TextureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    HR = Device->CreateCommittedResource(&HeapProperties, D3D12_HEAP_FLAG_NONE, &TextureDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&OutputTexture)); CHECK_RESULT();

    /// Creeate UAV descriptors
    UINT DescSize = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_CPU_DESCRIPTOR_HANDLE Handle = UAVHeap->GetCPUDescriptorHandleForHeapStart();

    for (int i = 0; i < 2; i++)
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
        UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
        UAVDesc.Buffer.NumElements = SizeInUin32;
        UAVDesc.Buffer.StructureByteStride = sizeof(uint32_t);
        Device->CreateUnorderedAccessView(CellBuffers[i].Get(), nullptr, &UAVDesc, Handle);
        Handle.ptr += DescSize;
    }

    D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
    UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    UAVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    Device->CreateUnorderedAccessView(OutputTexture.Get(), nullptr, &UAVDesc, Handle);

    /// Create the root signature
    D3D12_DESCRIPTOR_RANGE Ranges[1] = {};
    Ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    Ranges[0].NumDescriptors = 3;
    Ranges[0].BaseShaderRegister = 0;

    D3D12_ROOT_PARAMETER RootParameters[2] = {};
    RootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    RootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
    RootParameters[0].DescriptorTable.pDescriptorRanges = Ranges;
    RootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    RootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    RootParameters[1].Constants.Num32BitValues = 1;
    RootParameters[1].Constants.ShaderRegister = 0;
    RootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC RootSignatureDesc = {};
    RootSignatureDesc.NumParameters = 2;
    RootSignatureDesc.pParameters = RootParameters;
    RootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    HR = D3D12SerializeRootSignature(&RootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &SigBlob, &ErrorBlob); CHECK_RESULT();
    HR = Device->CreateRootSignature(0, SigBlob->GetBufferPointer(), SigBlob->GetBufferSize(), IID_PPV_ARGS(&RootSignature)); CHECK_RESULT();

    /// Prepare compiler
    HR = DxcCreateInstance((CLSID_DxcUtils), IID_PPV_ARGS(&Utils)); CHECK_RESULT();
    HR = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&Compiler)); CHECK_RESULT();

    HR = Utils->CreateDefaultIncludeHandler(&IncludeHandler); CHECK_RESULT();

    /// Compile shader
    auto ShaderBytecode = CompileShader(L"tick.hlsl", L"main", L"cs_6_0");

    D3D12_COMPUTE_PIPELINE_STATE_DESC PSODesc = {};
    PSODesc.pRootSignature = RootSignature.Get();
    PSODesc.CS = {ShaderBytecode.data(), ShaderBytecode.size()};
    HR = Device->CreateComputePipelineState(&PSODesc, IID_PPV_ARGS(&PSO)); CHECK_RESULT();

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

    ID3D12DescriptorHeap* Heaps[] = {UAVHeap.Get()};
    CommandList->SetDescriptorHeaps(1, Heaps);
    CommandList->SetComputeRootDescriptorTable(0, UAVHeap->GetGPUDescriptorHandleForHeapStart());
    CommandList->SetComputeRoot32BitConstant(1, CurrentBufferIndex, 0);

    CommandList->Dispatch(X, Y, Z);

    D3D12_RESOURCE_BARRIER Barriers[2] = {};
    Barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    Barriers[0].Transition.pResource = OutputTexture.Get();
    Barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    Barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    Barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    Barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    Barriers[1].Transition.pResource = BackBuffers[CurrentFrameIndex].Get();
    Barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    Barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    Barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    CommandList->ResourceBarrier(2, Barriers);

    CommandList->CopyResource(BackBuffers[CurrentFrameIndex].Get(), OutputTexture.Get());

    std::swap(Barriers[0].Transition.StateBefore, Barriers[0].Transition.StateAfter);
    std::swap(Barriers[1].Transition.StateBefore, Barriers[1].Transition.StateAfter);
    CommandList->ResourceBarrier(2, Barriers);

    HR = CommandList->Close();
    ID3D12CommandList* Lists[] = {CommandList.Get()};
    CommandQueue->ExecuteCommandLists(1, Lists);

    CurrentBufferIndex = 1 - CurrentBufferIndex;
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

std::vector<uint8_t> FDX12Context::CompileShader(const wchar_t* FilePath, const wchar_t* EntryPoint, const wchar_t* Profile)
{
    HRESULT HR = S_OK;

    ComPtr<IDxcBlobEncoding> SourceBlob;
    HR = Utils->LoadFile(FilePath, nullptr, &SourceBlob);

    DxcBuffer Source = {};
    Source.Ptr = SourceBlob->GetBufferPointer();
    Source.Size = SourceBlob->GetBufferSize();
    Source.Encoding = DXC_CP_ACP;

    LPCWSTR Args[] ={
        FilePath,
        L"-E", EntryPoint,
        L"-T", Profile
    };

    ComPtr<IDxcResult> Result;
    HR = Compiler->Compile(&Source, Args, _countof(Args), IncludeHandler.Get(), IID_PPV_ARGS(&Result));

    ComPtr<IDxcBlobUtf8> Errors;
    HR = Result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&Errors), nullptr);
    if (Errors && Errors->GetStringLength() > 0)
    {
        OutputDebugStringA(Errors->GetStringPointer());
    }

    Result->GetStatus(&HR);
    if (FAILED(HR))
        return {};

    ComPtr<IDxcBlob> ShaderBlob;
    HR = Result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&ShaderBlob), nullptr);

    auto* Begin = reinterpret_cast<uint8_t*>(ShaderBlob->GetBufferPointer());
    return {Begin, Begin + ShaderBlob->GetBufferSize()};
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
