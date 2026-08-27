#include "PostProcessCRT.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>

using namespace KamataEngine;

namespace {

constexpr DXGI_FORMAT kSceneTextureFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
constexpr DXGI_FORMAT kSpriteTextureFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

void EnsureSucceeded(HRESULT result) {
	if (FAILED(result)) {
		assert(false);
		std::abort();
	}
}

} // namespace

PostProcessCRT::~PostProcessCRT() {
	if (instance_ == this) {
		instance_ = nullptr;
	}
	if (constantBuffer_ && mappedParameters_) {
		constantBuffer_->Unmap(0, nullptr);
		mappedParameters_ = nullptr;
	}
}

void PostProcessCRT::Initialize() {
	instance_ = this;
	directXCommon_ = DirectXCommon::GetInstance();
	assert(directXCommon_ != nullptr);
	parameters_.resolution[0] = static_cast<float>(directXCommon_->GetBackBufferWidth());
	parameters_.resolution[1] = static_cast<float>(directXCommon_->GetBackBufferHeight());

	viewport_.TopLeftX = 0.0f;
	viewport_.TopLeftY = 0.0f;
	viewport_.Width = parameters_.resolution[0];
	viewport_.Height = parameters_.resolution[1];
	viewport_.MinDepth = 0.0f;
	viewport_.MaxDepth = 1.0f;
	scissorRect_ = {
		0, 0, directXCommon_->GetBackBufferWidth(), directXCommon_->GetBackBufferHeight()};

	CreateRenderResources();
	CreateConstantBuffer();
	CreateGraphicsPipeline();
	*mappedParameters_ = parameters_;
	initialized_ = true;
}

void PostProcessCRT::Update() {
	if (!initialized_) {
		return;
	}
	parameters_.time += 1.0f / 60.0f;
	// 強い砂嵐を最初に見せつつ、約0.5秒で滑らかに減衰する。
	parameters_.damageEffect *= 0.89f;
	if (parameters_.damageEffect < 0.005f) {
		parameters_.damageEffect = 0.0f;
	}
	// 命中の余韻は少し長めに残し、連続ヒットで再加算できるようにする。
	parameters_.successfulHitEffect *= 0.94f;
	if (parameters_.successfulHitEffect < 0.005f) {
		parameters_.successfulHitEffect = 0.0f;
	}

#ifdef USE_IMGUI
	bool enabled = parameters_.enabled > 0.5f;
	ImGui::Begin("CRT Filter");
	if (ImGui::Checkbox("Enabled", &enabled)) {
		parameters_.enabled = enabled ? 1.0f : 0.0f;
	}
	ImGui::SliderFloat("Curvature", &parameters_.curvature, 0.0f, 0.12f, "%.3f");
	ImGui::SliderFloat(
	    "Scanline", &parameters_.scanlineStrength, 0.0f, 0.30f, "%.3f");
	ImGui::SliderFloat(
	    "RGB Offset (px)", &parameters_.rgbOffsetPixels, 0.0f, 4.0f, "%.2f");
	ImGui::SliderFloat(
	    "Vignette", &parameters_.vignetteStrength, 0.0f, 0.60f, "%.3f");
	ImGui::SliderFloat("Noise", &parameters_.noiseStrength, 0.0f, 0.08f, "%.3f");
	ImGui::SliderFloat(
	    "Horizontal Jitter (px)", &parameters_.horizontalJitterPixels,
	    0.0f, 3.0f, "%.2f");
	ImGui::SliderFloat(
	    "Rolling Noise", &parameters_.rollingNoiseStrength,
	    0.0f, 2.0f, "%.2f");
	ImGui::SliderFloat(
	    "Flicker (weak)", &parameters_.flickerStrength, 0.0f, 0.03f, "%.3f");
	ImGui::SliderFloat(
	    "Phosphor Mask (weak)", &parameters_.phosphorMaskStrength,
	    0.0f, 0.15f, "%.3f");
	ImGui::ProgressBar(
	    parameters_.damageEffect, ImVec2(-1.0f, 0.0f), "Damage Effect");
	ImGui::ProgressBar(
	    (std::min)(parameters_.successfulHitEffect, 1.0f),
	    ImVec2(-1.0f, 0.0f), "Hit Praise");
	ImGui::ProgressBar(
	    parameters_.transitionProgress, ImVec2(-1.0f, 0.0f),
	    "CRT Shutdown");
	ImGui::End();
#endif

	*mappedParameters_ = parameters_;
}

void PostProcessCRT::BeginScene() {
	if (!initialized_) {
		return;
	}
	activeInstance_ = this;
	BindSceneTarget(true);
	ClearSpriteLayer();
}

void PostProcessCRT::ClearSpriteLayer() {
	ID3D12Resource* backBuffer = directXCommon_->GetCurrentBackBufferResource();
	if (backBuffer == nullptr) {
		return;
	}
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = kSceneTextureFormat;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	directXCommon_->GetDevice()->CreateRenderTargetView(
	    backBuffer, &rtvDesc,
	    spriteLayerRtvHeap_->GetCPUDescriptorHandleForHeapStart());
	constexpr float transparent[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	directXCommon_->GetCommandList()->ClearRenderTargetView(
	    spriteLayerRtvHeap_->GetCPUDescriptorHandleForHeapStart(), transparent,
	    0, nullptr);
}

void PostProcessCRT::CaptureSpriteLayer() {
	if (!initialized_) {
		return;
	}
	ID3D12Resource* backBuffer = directXCommon_->GetCurrentBackBufferResource();
	if (backBuffer == nullptr) {
		return;
	}
	ID3D12GraphicsCommandList* commandList = directXCommon_->GetCommandList();
	D3D12_RESOURCE_BARRIER barriers[2] = {
	    CD3DX12_RESOURCE_BARRIER::Transition(
	        backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET,
	        D3D12_RESOURCE_STATE_COPY_SOURCE),
	    CD3DX12_RESOURCE_BARRIER::Transition(
	        spriteLayerTexture_.Get(), spriteLayerTextureState_,
	        D3D12_RESOURCE_STATE_COPY_DEST),
	};
	commandList->ResourceBarrier(_countof(barriers), barriers);
	commandList->CopyResource(spriteLayerTexture_.Get(), backBuffer);
	barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
	    backBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE,
	    D3D12_RESOURCE_STATE_RENDER_TARGET);
	barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
	    spriteLayerTexture_.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
	    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(_countof(barriers), barriers);
	spriteLayerTextureState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

void PostProcessCRT::RebindSceneTarget() {
	if (!initialized_) {
		return;
	}
	BindSceneTarget(false);
}

void PostProcessCRT::RebindActiveSceneTarget() {
	if (activeInstance_ != nullptr) {
		// KamataEngineのSprite PSOはUNORMのRTVを使用する。
		// リソース自体は同じなので、3D描画済みの内容を保ったまま
		// Sprite互換のRTVへ表示形式だけを切り替える。
		activeInstance_->BindSceneTarget(false, false);
	}
}

void PostProcessCRT::ClearActiveSceneColor(const Vector4& color) {
	if (activeInstance_ == nullptr || !activeInstance_->initialized_) {
		return;
	}
	const float clearColor[4] = {color.x, color.y, color.z, color.w};
	activeInstance_->directXCommon_->GetCommandList()->ClearRenderTargetView(
	    activeInstance_->rtvHeap_->GetCPUDescriptorHandleForHeapStart(),
	    clearColor, 0, nullptr);
}

void PostProcessCRT::TriggerDamageEffect() {
	if (instance_ != nullptr) {
		instance_->parameters_.damageEffect = 1.0f;
	}
}

void PostProcessCRT::TriggerSuccessfulHitEffect(int hitCount) {
	if (instance_ == nullptr || hitCount <= 0) {
		return;
	}
	// 単発でも見え、連続ヒットでは最大1.5まで発光を蓄積する。
	const float addedStrength = 0.22f * static_cast<float>((std::min)(hitCount, 5));
	instance_->parameters_.successfulHitEffect = (std::min)(
	    1.5f,
	    (std::max)(instance_->parameters_.successfulHitEffect, 0.24f) +
	        addedStrength);
}

void PostProcessCRT::SetTransitionProgress(float progress) {
	if (instance_ == nullptr) {
		return;
	}
	instance_->parameters_.transitionProgress =
	    std::clamp(progress, 0.0f, 1.0f);
}

void PostProcessCRT::BindSceneTarget(bool clearTargets, bool sRGB) {
	ID3D12GraphicsCommandList* commandList = directXCommon_->GetCommandList();
	if (sceneTextureState_ != D3D12_RESOURCE_STATE_RENDER_TARGET) {
		const D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		    sceneTexture_.Get(), sceneTextureState_, D3D12_RESOURCE_STATE_RENDER_TARGET);
		commandList->ResourceBarrier(1, &barrier);
		sceneTextureState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
	    rtvHeap_->GetCPUDescriptorHandleForHeapStart();
	if (!sRGB) {
		rtvHandle.ptr += directXCommon_->GetDevice()->GetDescriptorHandleIncrementSize(
		    D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}
	const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
	    dsvHeap_->GetCPUDescriptorHandleForHeapStart();
	commandList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);
	commandList->RSSetViewports(1, &viewport_);
	commandList->RSSetScissorRects(1, &scissorRect_);
	if (clearTargets) {
		constexpr float clearColor[4] = {0.008f, 0.008f, 0.015f, 1.0f};
		commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		commandList->ClearDepthStencilView(
		    dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	}
}

void PostProcessCRT::EndSceneAndDraw() {
	if (!initialized_) {
		return;
	}
	activeInstance_ = nullptr;
	ID3D12GraphicsCommandList* commandList = directXCommon_->GetCommandList();
	if (sceneTextureState_ != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
		const D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		    sceneTexture_.Get(), sceneTextureState_,
		    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		commandList->ResourceBarrier(1, &barrier);
		sceneTextureState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}

	// バックバッファへ戻し、オフスクリーンの全内容（UIを含む）を一度に加工する。
	directXCommon_->SetRenderTargets(true);
	commandList->RSSetViewports(1, &viewport_);
	commandList->RSSetScissorRects(1, &scissorRect_);
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(pipelineState_.Get());
	ID3D12DescriptorHeap* descriptorHeaps[] = {srvHeap_.Get()};
	commandList->SetDescriptorHeaps(1, descriptorHeaps);
	commandList->SetGraphicsRootConstantBufferView(
	    0, constantBuffer_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(
	    1, srvHeap_->GetGPUDescriptorHandleForHeapStart());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(3, 1, 0, 0);
}

void PostProcessCRT::CreateRenderResources() {
	ID3D12Device* device = directXCommon_->GetDevice();

	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	// 3D/ImGui用のsRGB RTVとSprite用のUNORM RTVを同じリソースに作る。
	rtvHeapDesc.NumDescriptors = 2;
	EnsureSucceeded(device->CreateDescriptorHeap(
	    &rtvHeapDesc, IID_PPV_ARGS(&rtvHeap_)));

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.NumDescriptors = 1;
	EnsureSucceeded(device->CreateDescriptorHeap(
	    &dsvHeapDesc, IID_PPV_ARGS(&dsvHeap_)));

	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{};
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.NumDescriptors = 2;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	EnsureSucceeded(device->CreateDescriptorHeap(
	    &srvHeapDesc, IID_PPV_ARGS(&srvHeap_)));

	D3D12_DESCRIPTOR_HEAP_DESC spriteLayerRtvHeapDesc{};
	spriteLayerRtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	spriteLayerRtvHeapDesc.NumDescriptors = 1;
	EnsureSucceeded(device->CreateDescriptorHeap(
	    &spriteLayerRtvHeapDesc, IID_PPV_ARGS(&spriteLayerRtvHeap_)));

	D3D12_HEAP_PROPERTIES defaultHeap{};
	defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
	defaultHeap.CreationNodeMask = 1;
	defaultHeap.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC sceneDesc{};
	sceneDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	sceneDesc.Width = static_cast<UINT64>(directXCommon_->GetBackBufferWidth());
	sceneDesc.Height = static_cast<UINT>(directXCommon_->GetBackBufferHeight());
	sceneDesc.DepthOrArraySize = 1;
	sceneDesc.MipLevels = 1;
	sceneDesc.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
	sceneDesc.SampleDesc.Count = 1;
	sceneDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	sceneDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	D3D12_CLEAR_VALUE sceneClearValue{};
	sceneClearValue.Format = kSceneTextureFormat;
	sceneClearValue.Color[0] = 0.008f;
	sceneClearValue.Color[1] = 0.008f;
	sceneClearValue.Color[2] = 0.015f;
	sceneClearValue.Color[3] = 1.0f;
	EnsureSucceeded(device->CreateCommittedResource(
	    &defaultHeap, D3D12_HEAP_FLAG_NONE, &sceneDesc,
	    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &sceneClearValue,
	    IID_PPV_ARGS(&sceneTexture_)));

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = kSceneTextureFormat;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	device->CreateRenderTargetView(
	    sceneTexture_.Get(), &rtvDesc, rtvHeap_->GetCPUDescriptorHandleForHeapStart());
	D3D12_RENDER_TARGET_VIEW_DESC spriteRtvDesc{};
	spriteRtvDesc.Format = kSpriteTextureFormat;
	spriteRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	D3D12_CPU_DESCRIPTOR_HANDLE spriteRtvHandle =
	    rtvHeap_->GetCPUDescriptorHandleForHeapStart();
	spriteRtvHandle.ptr += device->GetDescriptorHandleIncrementSize(
	    D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	device->CreateRenderTargetView(
	    sceneTexture_.Get(), &spriteRtvDesc, spriteRtvHandle);
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = kSceneTextureFormat;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	device->CreateShaderResourceView(
	    sceneTexture_.Get(), &srvDesc, srvHeap_->GetCPUDescriptorHandleForHeapStart());

	// Sprite::PostDrawは必ずバックバッファへ描画するため、その結果を
	// コピーして保持する透明UIレイヤーを用意する。
	D3D12_RESOURCE_DESC spriteLayerDesc = sceneDesc;
	spriteLayerDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	EnsureSucceeded(device->CreateCommittedResource(
	    &defaultHeap, D3D12_HEAP_FLAG_NONE, &spriteLayerDesc,
	    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr,
	    IID_PPV_ARGS(&spriteLayerTexture_)));
	D3D12_CPU_DESCRIPTOR_HANDLE spriteLayerSrvHandle =
	    srvHeap_->GetCPUDescriptorHandleForHeapStart();
	spriteLayerSrvHandle.ptr += device->GetDescriptorHandleIncrementSize(
	    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	device->CreateShaderResourceView(
	    spriteLayerTexture_.Get(), &srvDesc, spriteLayerSrvHandle);

	D3D12_RESOURCE_DESC depthDesc{};
	depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthDesc.Width = static_cast<UINT64>(directXCommon_->GetBackBufferWidth());
	depthDesc.Height = static_cast<UINT>(directXCommon_->GetBackBufferHeight());
	depthDesc.DepthOrArraySize = 1;
	depthDesc.MipLevels = 1;
	depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	depthClearValue.DepthStencil.Depth = 1.0f;
	EnsureSucceeded(device->CreateCommittedResource(
	    &defaultHeap, D3D12_HEAP_FLAG_NONE, &depthDesc,
	    D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClearValue,
	    IID_PPV_ARGS(&depthTexture_)));
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	device->CreateDepthStencilView(
	    depthTexture_.Get(), &dsvDesc, dsvHeap_->GetCPUDescriptorHandleForHeapStart());
}

void PostProcessCRT::CreateConstantBuffer() {
	D3D12_HEAP_PROPERTIES uploadHeap{};
	uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
	uploadHeap.CreationNodeMask = 1;
	uploadHeap.VisibleNodeMask = 1;
	D3D12_RESOURCE_DESC bufferDesc{};
	bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufferDesc.Width = (sizeof(Parameters) + 0xffu) & ~0xffu;
	bufferDesc.Height = 1;
	bufferDesc.DepthOrArraySize = 1;
	bufferDesc.MipLevels = 1;
	bufferDesc.SampleDesc.Count = 1;
	bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	EnsureSucceeded(directXCommon_->GetDevice()->CreateCommittedResource(
	    &uploadHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
	    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
	    IID_PPV_ARGS(&constantBuffer_)));
	EnsureSucceeded(constantBuffer_->Map(
	    0, nullptr, reinterpret_cast<void**>(&mappedParameters_)));
}

void PostProcessCRT::CreateGraphicsPipeline() {
	ShaderManager* shaderManager = directXCommon_->GetShaderManager();
	shaderManager->SetBaseDirectory(L"./Resources/EngineShaders/");
	shaderManager->Compile(
	    "CRTFullscreenVS", L"CRTFullscreen.VS.hlsl", L"vs_6_0");
	shaderManager->Compile("CRTPS", L"CRT.PS.hlsl", L"ps_6_0");
	IDxcBlob* vertexShader = shaderManager->GetBlob("CRTFullscreenVS");
	IDxcBlob* pixelShader = shaderManager->GetBlob("CRTPS");
	assert(vertexShader != nullptr && pixelShader != nullptr);

	CD3DX12_DESCRIPTOR_RANGE srvRange;
	srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);
	CD3DX12_ROOT_PARAMETER rootParameters[2]{};
	rootParameters[0].InitAsConstantBufferView(
	    0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[1].InitAsDescriptorTable(
	    1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);
	D3D12_STATIC_SAMPLER_DESC sampler{};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.MaxAnisotropy = 1;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumStaticSamplers = 1;
	rootSignatureDesc.pStaticSamplers = &sampler;
	rootSignatureDesc.Flags =
	    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSignature;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	EnsureSucceeded(D3D12SerializeRootSignature(
	    &rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1,
	    &serializedRootSignature, &errorBlob));
	EnsureSucceeded(directXCommon_->GetDevice()->CreateRootSignature(
	    0, serializedRootSignature->GetBufferPointer(),
	    serializedRootSignature->GetBufferSize(), IID_PPV_ARGS(&rootSignature_)));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
	pipelineDesc.pRootSignature = rootSignature_.Get();
	pipelineDesc.VS = CD3DX12_SHADER_BYTECODE(
	    vertexShader->GetBufferPointer(), vertexShader->GetBufferSize());
	pipelineDesc.PS = CD3DX12_SHADER_BYTECODE(
	    pixelShader->GetBufferPointer(), pixelShader->GetBufferSize());
	pipelineDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	pipelineDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	pipelineDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	pipelineDesc.DepthStencilState.DepthEnable = false;
	pipelineDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineDesc.NumRenderTargets = 1;
	pipelineDesc.RTVFormats[0] = kSceneTextureFormat;
	pipelineDesc.SampleDesc.Count = 1;
	EnsureSucceeded(directXCommon_->GetDevice()->CreateGraphicsPipelineState(
	    &pipelineDesc, IID_PPV_ARGS(&pipelineState_)));
}
