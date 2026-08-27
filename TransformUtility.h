#pragma once
#include "KamataEngine.h"

/// <summary>
/// 行列を計算・転送する
/// </summary>
void UpdateWorldTransform(KamataEngine::WorldTransform& worldTransform);

/// <summary>
/// ベクトルを行列で回転させる（平行移動成分を無視して回転のみ適用）
/// </summary>
KamataEngine::Vector3 TransformNormal(const KamataEngine::Vector3& v, const KamataEngine::Matrix4x4& m);

/// <summary>
/// 3D座標から2Dスクリーン座標へ変換する
/// </summary>
KamataEngine::Vector2 WorldToScreen(const KamataEngine::Vector3& worldPos, const KamataEngine::Matrix4x4& matView, const KamataEngine::Matrix4x4& matProjection, float windowWidth, float windowHeight);