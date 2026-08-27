// BossBeam.obj専用の発光表現。
// effectColor.a > 1.5 のモデルだけがこの処理を通る。
float BossBeamHash(float2 p) {
	p = frac(p * float2(123.34f, 456.21f));
	p += dot(p, p + 45.32f);
	return frac(p.x * p.y);
}

float4 ApplyBossBeamEffect(
	Texture2D beamTexture, SamplerState beamSampler, float2 baseUv,
	float time, float4 effectColor) {
	// UVを発射方向へ流し、複数の速度を重ねてエネルギーの筋を作る。
	float2 uv = baseUv;
	float fastFlow = frac(uv.y * 5.0f - time * 3.8f);
	float slowFlow = frac(uv.y * 2.0f - time * 1.35f);
	float noise = BossBeamHash(floor(float2(uv.x * 18.0f, slowFlow * 30.0f)));
	float flowPulse = 0.58f + 0.42f * sin(fastFlow * 6.2831853f + noise * 3.0f);

	// ビーム中央は白～黄、外側は赤～橙。輪郭は時間で呼吸する。
	float centerDistance = abs(uv.x - 0.5f) * 2.0f;
	float pulseWidth = 0.78f + sin(time * 8.0f + uv.y * 10.0f) * 0.08f;
	float body = 1.0f - smoothstep(pulseWidth, 1.0f, centerDistance);
	float core = 1.0f - smoothstep(0.0f, 0.24f, centerDistance);
	float edge = smoothstep(0.42f, 0.82f, centerDistance) * body;

	float4 textureColor = beamTexture.Sample(beamSampler, uv);
	float3 outerColor = float3(1.0f, 0.035f, 0.005f);
	float3 middleColor = float3(1.0f, 0.28f, 0.015f);
	float3 beamColor = lerp(outerColor, middleColor, 1.0f - centerDistance);
	beamColor = lerp(beamColor, float3(1.0f, 0.92f, 0.58f), core);
	beamColor *= 1.15f + flowPulse * 0.75f + edge * 0.55f;
	beamColor += textureColor.rgb * 0.18f;

	// 予兆中は薄く、本発射中は高密度にする。マーカー用alphaは出力しない。
	float warningFactor = effectColor.g > 0.5f ? 0.52f : 1.0f;
	float alpha = saturate((0.30f + body * 0.68f + core * 0.22f) * warningFactor);
	return float4(beamColor * warningFactor, alpha);
}
