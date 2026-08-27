#pragma once

#include "KamataEngine.h"

#include <cstdint>
#include <wrl.h>

// ゲーム画面とUIをまとめてオフスクリーンへ描画し、CRTフィルターを適用する。
class PostProcessCRT final {
public:
	PostProcessCRT() = default;
	~PostProcessCRT();

	void Initialize();
	void Update();
	void BeginScene();
	void CaptureSpriteLayer();
	void RebindSceneTarget();
	void EndSceneAndDraw();
	static void RebindActiveSceneTarget();
	static void ClearActiveSceneColor(const KamataEngine::Vector4& color);
	static void TriggerDamageEffect();
	static void TriggerSuccessfulHitEffect(int hitCount = 1);
	static void SetTransitionProgress(float progress);

private:
	struct Parameters {
		float resolution[2] = {1280.0f, 720.0f};
		float time = 0.0f;
		float curvature = 0.08f;
		float scanlineStrength = 0.10f;
		float rgbOffsetPixels = 1.15f;
		float vignetteStrength = 0.24f;
		float noiseStrength = 0.010f;
		float horizontalJitterPixels = 0.65f;
		float rollingNoiseStrength = 0.65f;
		// UIの視認性を守るため、フリッカーと蛍光体マスクは弱くする。
		float flickerStrength = 0.004f;
		float phosphorMaskStrength = 0.025f;
		float phosphorMaskScale = 1.0f;
		float enabled = 1.0f;
		float damageEffect = 0.0f;
		float successfulHitEffect = 0.0f;
		float transitionProgress = 0.0f;
	};

	void CreateRenderResources();
	void CreateConstantBuffer();
	void CreateGraphicsPipeline();
	void BindSceneTarget(bool clearTargets, bool sRGB = true);
	void ClearSpriteLayer();

	KamataEngine::DirectXCommon* directXCommon_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> sceneTexture_;
	Microsoft::WRL::ComPtr<ID3D12Resource> spriteLayerTexture_;
	Microsoft::WRL::ComPtr<ID3D12Resource> depthTexture_;
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> spriteLayerRtvHeap_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
	Parameters parameters_{};
	Parameters* mappedParameters_ = nullptr;
	D3D12_VIEWPORT viewport_{};
	D3D12_RECT scissorRect_{};
	D3D12_RESOURCE_STATES sceneTextureState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	D3D12_RESOURCE_STATES spriteLayerTextureState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	bool initialized_ = false;
	inline static PostProcessCRT* instance_ = nullptr;
	inline static PostProcessCRT* activeInstance_ = nullptr;
};
