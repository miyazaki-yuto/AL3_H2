float BeamImpactHash(float value)
{
    return frac(sin(value * 127.13) * 43758.5453);
}

float4 ApplyBeamImpactEffect(float2 uv, float time, float4 markerColor)
{
    float2 p = (uv - 0.5) * 2.0;
    float radius = length(p);
    float angle = atan2(p.y, p.x);
    float isPlayer = step(markerColor.r, markerColor.b);

    // 高密度な着弾コアと、外へ繰り返し走る二重衝撃リング。
    float flicker = 0.78 + 0.22 * sin(time * 47.0 + angle * 7.0);
    float core = 1.0 - smoothstep(0.0, 0.24 + flicker * 0.045, radius);
    float ringPhase = frac(time * 2.8);
    float ringRadius = lerp(0.18, 0.92, ringPhase);
    float ring = 1.0 - smoothstep(0.025, 0.075, abs(radius - ringRadius));
    float secondRadius = 0.14 + frac(ringPhase + 0.48) * 0.78;
    float secondRing = 1.0 - smoothstep(0.018, 0.052, abs(radius - secondRadius));

    // 回転するエネルギー弧と、放射状に飛び散る短い火花。
    float arcWave = sin(angle * 5.0 - time * 31.0 + radius * 10.0);
    float arcs = smoothstep(0.58, 0.94, arcWave) *
        (1.0 - smoothstep(0.30, 0.72, radius)) * smoothstep(0.14, 0.30, radius);
    float sector = floor((angle + 3.14159265) * 3.82);
    float sparkAngle = abs(frac((angle + 3.14159265) * 3.82 + time * 1.7) - 0.5);
    float sparkLength = 0.42 + BeamImpactHash(sector) * 0.48;
    float sparks = (1.0 - smoothstep(0.018, 0.075, sparkAngle)) *
        smoothstep(0.20, 0.35, radius) *
        (1.0 - smoothstep(sparkLength, sparkLength + 0.13, radius));

    float3 bossOuter = float3(1.0, 0.018, 0.003);
    float3 bossInner = float3(1.0, 0.74, 0.16);
    float3 playerOuter = float3(0.0, 0.34, 1.0);
    float3 playerInner = float3(0.55, 0.98, 1.0);
    float3 outerColor = lerp(bossOuter, playerOuter, isPlayer);
    float3 innerColor = lerp(bossInner, playerInner, isPlayer);
    float3 finalColor = outerColor * (ring + secondRing + sparks * 1.4 + arcs);
    finalColor += innerColor * (core * 2.5 + arcs * 0.8);
    finalColor *= 1.25 + flicker * 0.85;

    float alpha = saturate(
        core * 0.95 + ring * 0.80 + secondRing * 0.42 + arcs * 0.72 + sparks * 0.68);
    alpha *= 1.0 - smoothstep(0.94, 1.0, radius);
    clip(alpha - 0.025);
    return float4(finalColor, alpha);
}
