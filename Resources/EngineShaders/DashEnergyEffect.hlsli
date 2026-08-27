// Player/Boss突進中のモデル表面を流れるエネルギー表現。
float DashEnergyHash(float2 p) {
	p = frac(p * float2(123.34f, 345.45f));
	p += dot(p, p + 34.345f);
	return frac(p.x * p.y);
}

float4 ApplyDashEnergyEffect(
	Texture2D effectTexture, SamplerState effectSampler, float2 baseUv,
	float time, bool isBoss) {
	float4 textureColor = effectTexture.Sample(effectSampler, baseUv);
	clip(textureColor.a - 0.02f);

	float flow = frac(baseUv.y * 5.0f - time * 3.8f +
		0.10f * sin(baseUv.x * 24.0f + time * 9.0f));
	float streak = 1.0f - smoothstep(0.04f, 0.22f, abs(flow - 0.5f));
	float fineFlow = 0.5f + 0.5f * sin(
		baseUv.x * 42.0f + baseUv.y * 17.0f - time * 18.0f);
	float noise = DashEnergyHash(floor(baseUv * 18.0f + time * 3.0f));
	float pulse = 0.78f + 0.22f * sin(time * 20.0f);

	float3 coreColor = isBoss
		? float3(1.0f, 0.015f, 0.01f)
		: float3(0.0f, 0.48f, 1.0f);
	float3 edgeColor = isBoss
		? float3(1.0f, 0.38f, 0.04f)
		: float3(0.0f, 0.95f, 1.0f);
	float energy = streak * (1.25f + fineFlow * 0.55f) + noise * 0.16f;
	float3 outputColor = textureColor.rgb * (0.38f + pulse * 0.30f);
	outputColor += coreColor * energy * 1.45f;
	outputColor += edgeColor * (fineFlow * 0.34f + streak * 0.65f);
	outputColor = lerp(outputColor, float3(1.0f, 1.0f, 1.0f) * 1.8f,
		saturate(streak * fineFlow * 0.30f));
	return float4(outputColor, textureColor.a);
}
