#include "Wall.h"
#include "TransformUtility.h"

using namespace KamataEngine;

void Wall::Initialize(KamataEngine::Model* model, KamataEngine::Camera* camera) {
	model_ = model;
	camera_ = camera;
	worldTransform_.Initialize();
	worldTransform_.scale_ = {kArenaWallScale, kArenaWallScale, kArenaWallScale};
	// OBJの底面Y=1を地面Y=-0.1へ合わせる。
	worldTransform_.translation_.y =
	    kGroundHeight - kSourceWallBottomY * kArenaWallScale;
	UpdateWorldTransform(worldTransform_);
}

void Wall::Update() { UpdateWorldTransform(worldTransform_); }

void Wall::Draw() {
	if (model_ == nullptr || camera_ == nullptr) {
		return;
	}

	model_->Draw(worldTransform_, *camera_);
}
