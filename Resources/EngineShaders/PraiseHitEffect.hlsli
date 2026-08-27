// PlayerがBossへ命中させた時の報酬パーティクル専用。
float PraiseHitHash(float2 p) {
	p = frac(p * float2(269.5f, 183.3f));
	p += dot(p, p + 47.2f);
	return frac(p.x * p.y);
}

float3 PraiseSpectrum(float phase) {
	return 0.62f + 0.38f * cos(6.2831853f * (phase + float3(0.00f, 0.66f, 0.33f)));
}

float4 ApplyPraiseHitEffect(
	Texture2D effectTexture, SamplerState effectSampler, float2 baseUv,
	float progress, float4 effectColor) {
	float2 p = (baseUv - 0.5f) * 2.0f;
	float radius = length(p);
	float angle = atan2(p.y, p.x);
	float noise = PraiseHitHash(float2(floor((angle + 3.14159265f) * 18.0f), floor(progress * 20.0f)));

	// 中心フラッシュ、外へ走るリング、放射状の火花を重ねる。
	float flash = (1.0f - smoothstep(0.0f, 0.48f + progress * 0.28f, radius)) * (1.0f - progress);
	float ringRadius = lerp(0.12f, 0.96f, progress);
	float ring = 1.0f - smoothstep(0.045f, 0.14f, abs(radius - ringRadius));
	float rayPattern = pow(saturate(sin(angle * 9.0f + noise * 4.0f) * 0.5f + 0.5f), 7.0f);
	float rays = rayPattern * (1.0f - smoothstep(0.18f, 1.08f, radius)) * (1.0f - progress);
	float spark = step(0.78f, noise) * (1.0f - smoothstep(0.25f, 1.0f, radius));

	float3 rainbow = PraiseSpectrum(angle / 6.2831853f + progress * 0.75f + noise * 0.25f);
	float3 rewardColor = lerp(effectColor.rgb, rainbow, 0.42f + ring * 0.25f);
	float whiteCore = saturate(flash * 1.7f + ring * 0.55f);
	float3 color = rewardColor * (rays * 1.8f + ring * 2.0f + spark * 1.5f);
	color = lerp(color, float3(1.0f, 0.98f, 0.78f) * 2.4f, whiteCore);
	float4 textureColor = effectTexture.Sample(effectSampler, baseUv);
	color += textureColor.rgb * (flash + ring + rays) * 0.18f;

	float fade = 1.0f - smoothstep(0.68f, 1.0f, progress);
	float alpha = saturate((flash + ring * 0.92f + rays * 0.78f + spark * 0.45f) * fade);
	clip(alpha - 0.018f);
	return float4(color, alpha);
}
