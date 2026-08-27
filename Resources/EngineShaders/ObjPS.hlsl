#include "Obj.hlsli"
#include "BossBeamEffect.hlsli"
#include "PlayerBeamEffect.hlsli"
#include "BossExplosionEffect.hlsli"
#include "PlayerExplosionEffect.hlsli"
#include "PraiseHitEffect.hlsli"
#include "DashEnergyEffect.hlsli"

SamplerState smp : register(s0);      // 0番スロットに設定されたサンプラー

float4 main(VSOutput input) : SV_TARGET {
	// UV変換
	float2 uv = float2(
	    input.uv.x * m_uv_scale.x + m_uv_offset.x, input.uv.y * m_uv_scale.y + m_uv_offset.y);
	// テクスチャマッピング
	Texture2D tex = ResourceDescriptorHeap[m_textureDescriptorIndex];
	if (color.a > 10.5f) {
		return ApplyDashEnergyEffect(tex, smp, uv, color.a - 11.0f, true);
	}
	if (color.a > 8.5f) {
		return ApplyDashEnergyEffect(tex, smp, uv, color.a - 9.0f, false);
	}
	if (color.a > 6.5f) {
		return ApplyPlayerExplosionEffect(
			tex, smp, uv, saturate(color.a - 7.0f), color);
	}
	if (color.a > 4.5f) {
		return ApplyPraiseHitEffect(tex, smp, uv, saturate(color.a - 5.0f), color);
	}
	if (color.a > 3.5f) {
		return ApplyBossExplosionEffect(tex, smp, uv, m_uv_offset.z, color);
	}
	// Player/BossビームはObjectColorのalphaを描画種別マーカーとして使用する。
	// 通常OBJは従来のライティング処理をそのまま通る。
	if (color.a > 2.5f) {
		return ApplyPlayerBeamEffect(tex, smp, uv, m_uv_offset.z, color);
	}
	if (color.a > 1.5f) {
		return ApplyBossBeamEffect(tex, smp, uv, m_uv_offset.z, color);
	}
	float4 texcolor = tex.Sample(smp, uv);
	// 完全透明に近い画素は、色だけでなく深度にも書き込ませない。
	// 透明OBJ同士が重なった際に、後ろのモデルやライトが抜けて見える現象を防ぐ。
	clip(texcolor.a - 0.02f);

	// 光沢度
	const float shininess = 24.0f;
	// 影側へ光が回り込みすぎないようにして、面の陰影をはっきりさせる。
	const float lightingWrap = min(m_wrap, 0.15f);
	// 頂点から視点への方向ベクトル
	float3 viewDir = normalize(cameraPos - input.worldpos.xyz);

	// 環境反射光
	float3 ambient = m_ambient;

	// シェーディングによる色
    float4 shadecolor = float4(ambientColor * ambient, m_alpha);
	
    int i;

	// 平行光源
	for (i = 0; i < DIRLIGHT_NUM; i++) {
		if (dirLights[i].active) {
			// ライトに向かうベクトルと法線の内積
			float NdotL = dot(dirLights[i].direction, input.normal);
			// ハーフベクトル
			float3 halfVector = normalize(dirLights[i].direction + viewDir);
			// 拡散反射光 (Wrap Lighting)
            float3 diffuse = pow(saturate(NdotL + lightingWrap) / (1.0f + lightingWrap), 2) * m_diffuse;
			// 鏡面反射光 (Blinn-Phong)
			float3 specular = pow(saturate(dot(input.normal, halfVector)), shininess) * m_specular;

			// 全て加算する
			shadecolor.rgb += (diffuse + specular) * dirLights[i].color;
		}
	}

	// 点光源
	for (i = 0; i < POINTLIGHT_NUM; i++) {
		if (pointLights[i].active) {
			// ライトへの方向と距離
			float3 direction = pointLights[i].position - input.worldpos.xyz;
			float distance = length(direction);
			direction = normalize(direction);

			// ライトに向かうベクトルと法線の内積
			float NdotL = dot(direction, input.normal);
			// ハーフベクトル
			float3 halfVector = normalize(direction + viewDir);
			// 距離減衰
			float factor = pow(saturate(1.0f - distance / pointLights[i].radius), pointLights[i].decay);
			// 拡散反射光 (Wrap Lighting)
            float3 diffuse = pow(saturate(NdotL + lightingWrap) / (1.0f + lightingWrap), 2) * m_diffuse;
			// 鏡面反射光 (Blinn-Phong)
			float3 specular = pow(saturate(dot(input.normal, halfVector)), shininess) * m_specular;

			// 全て加算する
			shadecolor.rgb += pointLights[i].color * pointLights[i].intensity * (diffuse + specular) * factor;
		}
	}

	// スポットライト
	for (i = 0; i < SPOTLIGHT_NUM; i++) {
		if (spotLights[i].active) {
			// ライトへの方向と距離
			float3 direction = spotLights[i].position - input.worldpos.xyz;
			float distance = length(direction);
			direction = normalize(direction);

			// 距離減衰
			float distanceFactor = pow(saturate(1.0f - distance / spotLights[i].radius), spotLights[i].decay);

			// 角度減衰
			float cosTheta = dot(direction, spotLights[i].direction);
			float angleFactor = smoothstep(spotLights[i].cosAngle.y, spotLights[i].cosAngle.x, cosTheta);

			// 減衰を合成
			float factor = distanceFactor * angleFactor;

			// ライトに向かうベクトルと法線の内積
			float NdotL = dot(direction, input.normal);
			// ハーフベクトル
			float3 halfVector = normalize(direction + viewDir);
			// 拡散反射光 (Wrap Lighting)
            float3 diffuse = pow(saturate(NdotL + lightingWrap) / (1.0f + lightingWrap), 2) * m_diffuse;
			// 鏡面反射光 (Blinn-Phong)
			float3 specular = pow(saturate(dot(input.normal, halfVector)), shininess) * m_specular;

			// 全て加算する
			shadecolor.rgb += spotLights[i].color * spotLights[i].intensity * (diffuse + specular) * factor;
		}
	}

	// 丸影
	for (i = 0; i < CIRCLESHADOW_NUM; i++) {
		if (circleShadows[i].active) {
			// オブジェクト表面からキャスターへのベクトル
			float3 direction = circleShadows[i].position - input.worldpos.xyz;
			// 光線方向での距離
			float distance = dot(direction, circleShadows[i].direction);

			// 距離減衰係数
			float distanceFactor = saturate(
			    1.0f / (circleShadows[i].atten.x + circleShadows[i].atten.y * distance +
			            circleShadows[i].atten.z * distance * distance));
			// 距離がマイナスなら0にする
			distanceFactor *= step(0, distance);

			// ライトの座標
			float3 lightPos = circleShadows[i].position +
			                  circleShadows[i].direction * circleShadows[i].distanceCasterLight;
			//  オブジェクト表面からライトへのベクトル（単位ベクトル）
			float3 L = normalize(lightPos - input.worldpos.xyz);
			// 角度減衰
			float cosTheta = dot(L, circleShadows[i].direction);
			float angleFactor = smoothstep(circleShadows[i].cosAngle.y, circleShadows[i].cosAngle.x, cosTheta);
			
			// 減衰を合成
			float factor = distanceFactor * angleFactor;

			// 全て減算する
			shadecolor.rgb -= factor;
		}
	}

	// シェーディングによる色で描画
	return shadecolor * texcolor * color;
}
