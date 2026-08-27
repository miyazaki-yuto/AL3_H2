// Player設置攻撃の爆破専用。progressは0（発生）～1（消散）。
float PlayerExplosionHash(float2 p) {
	p = frac(p * float2(127.1f, 311.7f));
	p += dot(p, p + 74.7f);
	return frac(p.x * p.y);
}

float4 ApplyPlayerExplosionEffect(
	Texture2D effectTexture, SamplerState effectSampler, float2 baseUv,
	float progress, float4 effectColor) {
	float2 centeredUv = (baseUv - 0.5f) * 2.0f;
	float radius = length(centeredUv);
	float angle = atan2(centeredUv.y, centeredUv.x);
	float angularNoise = PlayerExplosionHash(
		float2(floor((angle + 3.14159265f) * 14.0f), floor(progress * 24.0f)));
	float distortedRadius =
		radius + (angularNoise - 0.5f) * 0.15f * (1.0f - progress);

	// Boss爆発と同じ勢いを保ちつつ、Playerカラーの青～水色へ置き換える。
	float ringRadius = lerp(0.10f, 1.06f, progress);
	float ringWidth = lerp(0.20f, 0.055f, progress);
	float shockRing = 1.0f - smoothstep(
		ringWidth, ringWidth * 2.1f, abs(distortedRadius - ringRadius));
	float energyBallRadius = lerp(0.68f, 0.28f, progress);
	float energyBall = 1.0f - smoothstep(
		energyBallRadius * 0.55f, energyBallRadius, distortedRadius);
	float whiteCore = 1.0f - smoothstep(
		0.0f, lerp(0.30f, 0.06f, progress), distortedRadius);
	float flicker = 0.72f + 0.28f *
		sin(angle * 11.0f + progress * 42.0f + angularNoise * 7.0f);

	float3 deepBlue = float3(0.005f, 0.12f, 1.0f);
	float3 cyan = float3(0.0f, 0.82f, 1.0f);
	float3 hot = float3(0.70f, 0.96f, 1.0f);
	float3 outputColor = deepBlue * shockRing * 1.55f;
	outputColor += cyan * energyBall * (1.30f + flicker * 0.60f);
	outputColor = lerp(outputColor, hot * 2.2f, whiteCore);
	float4 textureColor = effectTexture.Sample(effectSampler, baseUv);
	outputColor += textureColor.rgb * (shockRing + energyBall) * 0.10f;

	float fade = 1.0f - smoothstep(0.72f, 1.0f, progress);
	float alpha = saturate(
		(shockRing * 0.95f + energyBall * 0.82f + whiteCore * 0.35f) * fade);
	clip(alpha - 0.015f);
	return float4(outputColor, alpha);
}
