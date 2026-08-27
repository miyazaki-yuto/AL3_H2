#pragma once

#include "KamataEngine.h"

class LerpManager final {
public:
	enum class EaseType {
		Linear,
		EaseInSine,
		EaseOutSine,
		EaseInOutSine,
		EaseInQuad,
		EaseOutQuad,
		EaseInOutQuad,
		EaseInCubic,
		EaseOutCubic,
		EaseInOutCubic,
		SmoothStep,
		SmootherStep,
		EaseOutBack,
		EaseOutBounce,
	};

	// tは0.0f～1.0fへ制限してから、指定したイージングを適用する。
	static float ApplyEasing(float t, EaseType easeType);

	static float Lerp(float start, float end, float t, EaseType easeType = EaseType::Linear);
	static KamataEngine::Vector2 Lerp(
	    const KamataEngine::Vector2& start,
	    const KamataEngine::Vector2& end,
	    float t,
	    EaseType easeType = EaseType::Linear);
	static KamataEngine::Vector3 Lerp(
	    const KamataEngine::Vector3& start,
	    const KamataEngine::Vector3& end,
	    float t,
	    EaseType easeType = EaseType::Linear);

	// ラジアン角を最短方向へ補間する。
	static float LerpAngle(float start, float end, float t, EaseType easeType = EaseType::Linear);

	static float Linear(float t);
	static float EaseInSine(float t);
	static float EaseOutSine(float t);
	static float EaseInOutSine(float t);
	static float EaseInQuad(float t);
	static float EaseOutQuad(float t);
	static float EaseInOutQuad(float t);
	static float EaseInCubic(float t);
	static float EaseOutCubic(float t);
	static float EaseInOutCubic(float t);
	static float SmoothStep(float t);
	static float SmootherStep(float t);
	static float EaseOutBack(float t);
	static float EaseOutBounce(float t);

private:
	static float Clamp01(float value);
};
