// PlayerBeam.obj専用の青白い流動発光表現。
float PlayerBeamHash(float2 p) {
	p = frac(p * float2(91.73f, 317.17f));
	p += dot(p, p + 31.41f);
	return frac(p.x * p.y);
}

float4 ApplyPlayerBeamEffect(
	Texture2D beamTexture, SamplerState beamSampler, float2 baseUv,
	float time, float4 effectColor) {
	float2 uv = baseUv;
	float fastFlow = frac(uv.y * 6.5f - time * 5.2f);
	float reverseFlow = frac(uv.y * 2.4f + time * 1.7f);
	float noise = PlayerBeamHash(floor(float2(uv.x * 22.0f, reverseFlow * 36.0f)));
	float streak = 0.55f + 0.45f * sin(fastFlow * 6.2831853f + noise * 4.0f);
	float centerDistance = abs(uv.x - 0.5f) * 2.0f;
	float pulseWidth = 0.80f + sin(time * 10.5f + uv.y * 13.0f) * 0.07f;
	float body = 1.0f - smoothstep(pulseWidth, 1.0f, centerDistance);
	float core = 1.0f - smoothstep(0.0f, 0.20f, centerDistance);
	float edge = smoothstep(0.48f, 0.86f, centerDistance) * body;
	float4 textureColor = beamTexture.Sample(beamSampler, uv);
	float3 outerColor = float3(0.0f, 0.22f, 1.0f);
	float3 middleColor = float3(0.0f, 0.92f, 1.0f);
	float3 beamColor = lerp(outerColor, middleColor, 1.0f - centerDistance);
	beamColor = lerp(beamColor, float3(0.90f, 1.0f, 1.0f), core);
	beamColor *= 1.20f + streak * 0.82f + edge * 0.50f;
	beamColor += textureColor.rgb * 0.16f;
	float alpha = saturate(0.34f + body * 0.64f + core * 0.24f);
	return float4(beamColor, alpha);
}
