#include "LerpManager.h"

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace KamataEngine;

float LerpManager::Clamp01(float value) {
	return std::clamp(value, 0.0f, 1.0f);
}

float LerpManager::ApplyEasing(float t, EaseType easeType) {
	t = Clamp01(t);

	switch (easeType) {
	case EaseType::EaseInSine:
		return EaseInSine(t);
	case EaseType::EaseOutSine:
		return EaseOutSine(t);
	case EaseType::EaseInOutSine:
		return EaseInOutSine(t);
	case EaseType::EaseInQuad:
		return EaseInQuad(t);
	case EaseType::EaseOutQuad:
		return EaseOutQuad(t);
	case EaseType::EaseInOutQuad:
		return EaseInOutQuad(t);
	case EaseType::EaseInCubic:
		return EaseInCubic(t);
	case EaseType::EaseOutCubic:
		return EaseOutCubic(t);
	case EaseType::EaseInOutCubic:
		return EaseInOutCubic(t);
	case EaseType::SmoothStep:
		return SmoothStep(t);
	case EaseType::SmootherStep:
		return SmootherStep(t);
	case EaseType::EaseOutBack:
		return EaseOutBack(t);
	case EaseType::EaseOutBounce:
		return EaseOutBounce(t);
	case EaseType::Linear:
	default:
		return Linear(t);
	}
}

float LerpManager::Lerp(float start, float end, float t, EaseType easeType) {
	const float easedT = ApplyEasing(t, easeType);
	return start + (end - start) * easedT;
}

Vector2 LerpManager::Lerp(const Vector2& start, const Vector2& end, float t, EaseType easeType) {
	const float easedT = ApplyEasing(t, easeType);
	return {
	    start.x + (end.x - start.x) * easedT,
	    start.y + (end.y - start.y) * easedT,
	};
}

Vector3 LerpManager::Lerp(const Vector3& start, const Vector3& end, float t, EaseType easeType) {
	const float easedT = ApplyEasing(t, easeType);
	return {
	    start.x + (end.x - start.x) * easedT,
	    start.y + (end.y - start.y) * easedT,
	    start.z + (end.z - start.z) * easedT,
	};
}

float LerpManager::LerpAngle(float start, float end, float t, EaseType easeType) {
	const float difference = std::atan2(std::sin(end - start), std::cos(end - start));
	return start + difference * ApplyEasing(t, easeType);
}

float LerpManager::Linear(float t) {
	return Clamp01(t);
}

float LerpManager::EaseInSine(float t) {
	t = Clamp01(t);
	return 1.0f - std::cos((t * std::numbers::pi_v<float>) / 2.0f);
}

float LerpManager::EaseOutSine(float t) {
	t = Clamp01(t);
	return std::sin((t * std::numbers::pi_v<float>) / 2.0f);
}

float LerpManager::EaseInOutSine(float t) {
	t = Clamp01(t);
	return -(std::cos(std::numbers::pi_v<float> * t) - 1.0f) / 2.0f;
}

float LerpManager::EaseInQuad(float t) {
	t = Clamp01(t);
	return t * t;
}

float LerpManager::EaseOutQuad(float t) {
	t = Clamp01(t);
	return 1.0f - (1.0f - t) * (1.0f - t);
}

float LerpManager::EaseInOutQuad(float t) {
	t = Clamp01(t);
	return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
}

float LerpManager::EaseInCubic(float t) {
	t = Clamp01(t);
	return t * t * t;
}

float LerpManager::EaseOutCubic(float t) {
	t = Clamp01(t);
	return 1.0f - std::pow(1.0f - t, 3.0f);
}

float LerpManager::EaseInOutCubic(float t) {
	t = Clamp01(t);
	return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

float LerpManager::SmoothStep(float t) {
	t = Clamp01(t);
	return t * t * (3.0f - 2.0f * t);
}

float LerpManager::SmootherStep(float t) {
	t = Clamp01(t);
	return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float LerpManager::EaseOutBack(float t) {
	t = Clamp01(t);
	constexpr float kBackStrength = 1.70158f;
	constexpr float kBackScale = kBackStrength + 1.0f;
	const float shiftedT = t - 1.0f;
	return 1.0f + kBackScale * shiftedT * shiftedT * shiftedT + kBackStrength * shiftedT * shiftedT;
}

float LerpManager::EaseOutBounce(float t) {
	t = Clamp01(t);
	constexpr float kBounceScale = 7.5625f;
	constexpr float kBounceDivision = 2.75f;

	if (t < 1.0f / kBounceDivision) {
		return kBounceScale * t * t;
	}
	if (t < 2.0f / kBounceDivision) {
		t -= 1.5f / kBounceDivision;
		return kBounceScale * t * t + 0.75f;
	}
	if (t < 2.5f / kBounceDivision) {
		t -= 2.25f / kBounceDivision;
		return kBounceScale * t * t + 0.9375f;
	}

	t -= 2.625f / kBounceDivision;
	return kBounceScale * t * t + 0.984375f;
}
