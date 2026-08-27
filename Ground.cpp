#include "Ground.h"
#include "TransformUtility.h"
using namespace KamataEngine;

void Ground::Initialize(KamataEngine::Model* model, KamataEngine::Camera* camera) {
	model_ = model;
	camera_ = camera;
	worldTransform_.Initialize();
	worldTransform_.translation_ = {0.0f, -0.1f, 0.0f};
	UpdateWorldTransform(worldTransform_);
}

void Ground::Update() { UpdateWorldTransform(worldTransform_); }

void Ground::Draw() {
	if (model_ == nullptr || camera_ == nullptr) {
		return;
	}

	model_->Draw(worldTransform_, *camera_);
}
