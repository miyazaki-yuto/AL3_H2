float GroundSpearHash(float2 p)
{
    p = frac(p * float2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return frac(p.x * p.y);
}

float GroundSpearRock(float2 p, float2 center, float radius, float wobble)
{
    float2 local = p - center;
    float angle = atan2(local.y, local.x);
    float roughRadius = radius * (1.0 + sin(angle * 5.0 + wobble) * 0.14 + sin(angle * 9.0) * 0.07);
    return 1.0 - smoothstep(roughRadius * 0.78, roughRadius, length(local));
}

float4 ApplyBossGroundSpearWarningEffect(float2 uv, float progress, float4 markerColor)
{
    float2 p = (uv - 0.5) * 2.0;
    float radius = length(p);
    float angle = atan2(p.y, p.x);
    float circleMask = 1.0 - smoothstep(0.88, 1.0, radius);

    // 地面全体が槍に押され、中心から何度も盛り上がる脈動。
    float pulse = 0.5 + 0.5 * sin(progress * 58.0 - radius * 19.0);
    float heave = (1.0 - smoothstep(0.08, 0.92, radius)) * (0.28 + progress * 0.72);
    float warningRingRadius = lerp(0.18, 0.86, progress);
    float warningRing = 1.0 - smoothstep(0.025, 0.075, abs(radius - warningRingRadius));

    // 中心から伸びる割れ目。直前ほど本数と明るさが増す。
    float angularCells = abs(frac((angle + 3.14159265) * 1.91) - 0.5);
    float crackWidth = lerp(0.035, 0.095, progress);
    float cracks = 1.0 - smoothstep(crackWidth, crackWidth * 2.4, angularCells);
    cracks *= smoothstep(0.10, 0.28, radius) * (1.0 - smoothstep(0.72, 0.96, radius));
    cracks *= step(GroundSpearHash(float2(floor(angle * 8.0), 3.0)), progress + 0.22);

    // 岩塊を円周上に並べ、進行に合わせて内側からぼこぼこと出現させる。
    float rocks = 0.0;
    float rockHighlight = 0.0;
    [unroll]
    for (int i = 0; i < 8; ++i) {
        float fi = (float)i;
        float seed = GroundSpearHash(float2(fi, 8.17));
        float rockAngle = fi * 0.78539816 + seed * 0.42;
        float ring = lerp(0.24, 0.69, frac(seed * 4.73));
        float appear = saturate(progress * 2.25 - fi * 0.075);
        float bob = sin(progress * 44.0 + fi * 2.7) * 0.025 * appear;
        float2 center = float2(cos(rockAngle), sin(rockAngle)) * (ring + bob);
        float rockRadius = lerp(0.07, 0.16, seed) * appear;
        float rock = GroundSpearRock(p, center, rockRadius, seed * 13.0 + progress * 8.0);
        rocks = max(rocks, rock);
        float2 lightOffset = float2(-0.025, -0.035);
        rockHighlight = max(rockHighlight,
            GroundSpearRock(p, center + lightOffset, rockRadius * 0.62, seed * 9.0) * rock);
    }

    float urgency = smoothstep(0.58, 1.0, progress);
    float3 darkGround = float3(0.16, 0.018, 0.006);
    float3 hotGround = float3(0.92, 0.055, 0.004);
    float3 color = lerp(darkGround, hotGround, heave * (0.55 + pulse * 0.45));
    color += float3(1.8, 0.16, 0.005) * (cracks * (0.45 + urgency) + warningRing * 0.75);
    color = lerp(color, float3(0.22, 0.07, 0.025), rocks * 0.92);
    color += float3(0.72, 0.23, 0.055) * rockHighlight * (0.55 + urgency * 0.65);
    color += float3(1.0, 0.34, 0.015) * pulse * urgency * heave * 0.55;

    float alpha = circleMask * saturate(0.30 + heave * 0.35 + warningRing * 0.55 + cracks * 0.62 + rocks * 0.72);
    clip(alpha - 0.025);
    return float4(color * (1.15 + urgency * 0.8), alpha);
}
