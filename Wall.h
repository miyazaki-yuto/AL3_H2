#pragma once
#include "KamataEngine.h"

class Wall {
public:
	Wall() = default;
	~Wall() = default;

	void Initialize(KamataEngine::Model* model, KamataEngine::Camera* camera);
	void Update();
	void Draw();
	const KamataEngine::Vector3& GetCenter() const { return worldTransform_.translation_; }
	// Wall.obj の内側面。ここより外へはキャラクター・スキルを出さない。
	float GetInnerRadius() const { return kArenaInnerRadius; }

private:
	// 新しいWall.objの実寸。外径2000、床に接する内側半径400。
	inline static constexpr float kSourceWallDiameter = 2000.0f;
	inline static constexpr float kArenaWallDiameter = 800.0f;
	inline static constexpr float kSourceWallInnerRadius = 400.0f;
	inline static constexpr float kSourceWallBottomY = 1.0f;
	inline static constexpr float kGroundHeight = -0.1f;
	inline static constexpr float kArenaWallScale = kArenaWallDiameter / kSourceWallDiameter;
	inline static constexpr float kArenaInnerRadius = kSourceWallInnerRadius * kArenaWallScale;
	KamataEngine::WorldTransform worldTransform_;
	KamataEngine::Model* model_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;
};
