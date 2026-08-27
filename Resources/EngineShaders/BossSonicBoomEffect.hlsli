float SonicHash(float value)
{
    return frac(sin(value * 91.3458) * 47453.5453);
}

float4 ApplyBossSonicBoomEffect(float2 uv, float progress, float4 markerColor)
{
    float2 centered = uv - 0.5;
    float radius = length(centered);
    float angle = atan2(centered.y, centered.x);

    // 円周を細かく割り、ソニックブームらしい不均一な輪郭にする。
    float sector = floor((angle + 3.14159265) * 10.1859);
    float fracture = (SonicHash(sector) - 0.5) * 0.032;
    float ripple = sin(angle * 18.0 + progress * 24.0) * 0.009;
    float ringDistance = abs(radius - (0.34 + fracture + ripple));
    float mainRing = 1.0 - smoothstep(0.012, 0.045, ringDistance);
    float outerRing = 1.0 - smoothstep(0.009, 0.026, abs(radius - 0.405));

    float brokenArc = step(0.18, SonicHash(sector + floor(progress * 9.0) * 7.0));
    float fade = pow(saturate(1.0 - progress), 1.35);
    float energy = saturate(mainRing * brokenArc + outerRing * 0.42) * fade;
    clip(energy - 0.025);

    float hotCore = saturate(mainRing * 1.8);
    float3 red = lerp(float3(1.0, 0.025, 0.005), float3(1.0, 0.72, 0.18), hotCore);
    red *= 1.8 + energy * 2.2;
    return float4(red, energy * 0.82);
}
