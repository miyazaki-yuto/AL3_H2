#include "TransformUtility.h"

#include <cmath>

using namespace KamataEngine;

namespace {

Matrix4x4 Multiply(const Matrix4x4& lhs, const Matrix4x4& rhs) {
	Matrix4x4 result{};
	for (int row = 0; row < 4; ++row) {
		for (int column = 0; column < 4; ++column) {
			for (int i = 0; i < 4; ++i) {
				result.m[row][column] += lhs.m[row][i] * rhs.m[i][column];
			}
		}
	}
	return result;
}

} // namespace

// GameScene.cppにあったアフィン変換関数をこちらに移動させます
Matrix4x4 MakeAffineTransform(const Vector3& scale, const Vector3& rotation, const Vector3& translation) {
	const float sinX = std::sin(rotation.x);
	const float cosX = std::cos(rotation.x);
	const float sinY = std::sin(rotation.y);
	const float cosY = std::cos(rotation.y);
	const float sinZ = std::sin(rotation.z);
	const float cosZ = std::cos(rotation.z);

	const Matrix4x4 scaleMatrix = {
	    scale.x, 0.0f, 0.0f, 0.0f,
	    0.0f, scale.y, 0.0f, 0.0f,
	    0.0f, 0.0f, scale.z, 0.0f,
	    0.0f, 0.0f, 0.0f, 1.0f};
	const Matrix4x4 rotateXMatrix = {
	    1.0f, 0.0f, 0.0f, 0.0f,
	    0.0f, cosX, sinX, 0.0f,
	    0.0f, -sinX, cosX, 0.0f,
	    0.0f, 0.0f, 0.0f, 1.0f};
	const Matrix4x4 rotateYMatrix = {
	    cosY, 0.0f, -sinY, 0.0f,
	    0.0f, 1.0f, 0.0f, 0.0f,
	    sinY, 0.0f, cosY, 0.0f,
	    0.0f, 0.0f, 0.0f, 1.0f};
	const Matrix4x4 rotateZMatrix = {
	    cosZ, sinZ, 0.0f, 0.0f,
	    -sinZ, cosZ, 0.0f, 0.0f,
	    0.0f, 0.0f, 1.0f, 0.0f,
	    0.0f, 0.0f, 0.0f, 1.0f};
	const Matrix4x4 translateMatrix = {
	    1.0f, 0.0f, 0.0f, 0.0f,
	    0.0f, 1.0f, 0.0f, 0.0f,
	    0.0f, 0.0f, 1.0f, 0.0f,
	    translation.x, translation.y, translation.z, 1.0f};

	const Matrix4x4 rotateMatrix = Multiply(Multiply(rotateZMatrix, rotateYMatrix), rotateXMatrix);
	return Multiply(Multiply(scaleMatrix, rotateMatrix), translateMatrix);
}

void UpdateWorldTransform(KamataEngine::WorldTransform& worldTransform) {
	// スケール、回転、平行移動を合成して行列を計算する
	worldTransform.matWorld_ = MakeAffineTransform(worldTransform.scale_, worldTransform.rotation_, worldTransform.translation_);

	// 親の行列が存在する場合、自身の行列に親の行列を掛け合わせる
	if (worldTransform.parent_) {
		worldTransform.matWorld_ = Multiply(worldTransform.matWorld_, worldTransform.parent_->matWorld_);
	}

	// 定数バッファへの書き込み
	worldTransform.TransferMatrix();
}
KamataEngine::Vector3 TransformNormal(const KamataEngine::Vector3& v, const KamataEngine::Matrix4x4& m) {
	KamataEngine::Vector3 result;
	// 平行移動成分（m[3][0~2]）を無視し、3x3の回転・スケール部分のみをベクトルに掛ける
	result.x = v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0];
	result.y = v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1];
	result.z = v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2];
	return result;
}

KamataEngine::Vector2
    WorldToScreen(const KamataEngine::Vector3& worldPos, const KamataEngine::Matrix4x4& matView, const KamataEngine::Matrix4x4& matProjection, float windowWidth, float windowHeight) {
	// View Projection変換行列の合成
	KamataEngine::Matrix4x4 matVP = Multiply(matView, matProjection);

	// ベクトルと行列の掛け算
	float x = worldPos.x * matVP.m[0][0] + worldPos.y * matVP.m[1][0] + worldPos.z * matVP.m[2][0] + 1.0f * matVP.m[3][0];
	float y = worldPos.x * matVP.m[0][1] + worldPos.y * matVP.m[1][1] + worldPos.z * matVP.m[2][1] + 1.0f * matVP.m[3][1];
	float z = worldPos.x * matVP.m[0][2] + worldPos.y * matVP.m[1][2] + worldPos.z * matVP.m[2][2] + 1.0f * matVP.m[3][2];
	float w = worldPos.x * matVP.m[0][3] + worldPos.y * matVP.m[1][3] + worldPos.z * matVP.m[2][3] + 1.0f * matVP.m[3][3];

	// w除算 (NDC座標系へ変換: -1.0 ~ 1.0)
	if (w == 0.0f)
		w = 0.0001f; // 0除算防止
	KamataEngine::Vector3 ndc = {x / w, y / w, z / w};

	// スクリーン座標系へ変換 (Y軸の向きが反転することに注意)
	KamataEngine::Vector2 screenPos;
	screenPos.x = (ndc.x + 1.0f) * (windowWidth / 2.0f);
	screenPos.y = (1.0f - ndc.y) * (windowHeight / 2.0f);

	return screenPos;
}
