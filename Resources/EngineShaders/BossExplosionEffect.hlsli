// Boss近距離爆破専用。progressは0（発生）～1（消散）。
float BossExplosionHash(float2 p) {
	p = frac(p * float2(127.1f, 311.7f));
	p += dot(p, p + 74.7f);
	return frac(p.x * p.y);
}

float4 ApplyBossExplosionEffect(
	Texture2D effectTexture, SamplerState effectSampler, float2 baseUv,
	float progress, float4 effectColor) {
	float2 centeredUv = (baseUv - 0.5f) * 2.0f;
	float radius = length(centeredUv);
	float angle = atan2(centeredUv.y, centeredUv.x);
	float angularNoise = BossExplosionHash(float2(floor((angle + 3.14159265f) * 14.0f), floor(progress * 24.0f)));
	float distortedRadius = radius + (angularNoise - 0.5f) * 0.15f * (1.0f - progress);

	// 高速で外へ走る衝撃波と、少し遅れて消える熱球を合成する。
	float ringRadius = lerp(0.10f, 1.06f, progress);
	float ringWidth = lerp(0.20f, 0.055f, progress);
	float shockRing = 1.0f - smoothstep(ringWidth, ringWidth * 2.1f, abs(distortedRadius - ringRadius));
	float fireBallRadius = lerp(0.68f, 0.28f, progress);
	float fireBall = 1.0f - smoothstep(fireBallRadius * 0.55f, fireBallRadius, distortedRadius);
	float whiteCore = 1.0f - smoothstep(0.0f, lerp(0.30f, 0.06f, progress), distortedRadius);
	float flicker = 0.72f + 0.28f * sin(angle * 11.0f + progress * 42.0f + angularNoise * 7.0f);

	float3 red = float3(1.0f, 0.025f, 0.0f);
	float3 orange = float3(1.0f, 0.28f, 0.005f);
	float3 hot = float3(1.0f, 0.92f, 0.56f);
	float3 color = red * shockRing * 1.45f;
	color += orange * fireBall * (1.25f + flicker * 0.55f);
	color = lerp(color, hot * 2.1f, whiteCore);
	float4 textureColor = effectTexture.Sample(effectSampler, baseUv);
	color += textureColor.rgb * (shockRing + fireBall) * 0.12f;

	float fade = 1.0f - smoothstep(0.72f, 1.0f, progress);
	float alpha = saturate((shockRing * 0.95f + fireBall * 0.82f + whiteCore * 0.35f) * fade);
	clip(alpha - 0.015f);
	return float4(color, alpha);
}
