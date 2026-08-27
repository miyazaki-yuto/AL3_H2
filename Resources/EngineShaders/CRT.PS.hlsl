Texture2D<float4> sceneTexture : register(t0);
Texture2D<float4> spriteLayerTexture : register(t1);
SamplerState sceneSampler : register(s0);

cbuffer CRTParameters : register(b0) {
    float2 resolution;
    float time;
    float curvature;
    float scanlineStrength;
    float rgbOffsetPixels;
    float vignetteStrength;
    float noiseStrength;
    float horizontalJitterPixels;
    float rollingNoiseStrength;
    float flickerStrength;
    float phosphorMaskStrength;
    float phosphorMaskScale;
    float enabled;
    float damageEffect;
    float successfulHitEffect;
    float transitionProgress;
};

struct PixelShaderInput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

float Random(float2 position) {
    return frac(sin(dot(position, float2(12.9898f, 78.233f))) * 43758.5453f);
}

float4 SampleComposited(float2 uv) {
    const float4 scene = sceneTexture.Sample(sceneSampler, uv);
    const float4 sprite = spriteLayerTexture.Sample(sceneSampler, uv);
    // Spriteは透明な描画先へ通常アルファブレンド済みなのでRGBは乗算済み。
    return float4(sprite.rgb + scene.rgb * (1.0f - sprite.a), 1.0f);
}

float4 main(PixelShaderInput input) : SV_TARGET {
    if (enabled < 0.5f && transitionProgress <= 0.0f) {
        return SampleComposited(input.texcoord);
    }

    const float shutdown = saturate(transitionProgress);
    const float verticalCollapse = smoothstep(0.0f, 0.78f, shutdown);
    const float horizontalCollapse = smoothstep(0.76f, 1.0f, shutdown);
    const float bandHeight = lerp(1.0f, 0.014f, verticalCollapse);
    const float bandWidth = lerp(1.0f, 0.004f, horizontalCollapse);
    const float2 outputCentered = input.texcoord * 2.0f - 1.0f;
    if (abs(outputCentered.y) > bandHeight ||
        abs(outputCentered.x) > bandWidth ||
        shutdown >= 0.999f) {
        // 収束線の周囲に短く残光を出し、最後は完全な黒へ落とす。
        const float lineDistance = abs(outputCentered.y) / max(bandHeight, 0.001f);
        const float lineGlow =
            exp(-lineDistance * 7.0f) *
            lerp(
                1.0f,
                exp(-abs(outputCentered.x) / max(bandWidth, 0.001f) * 2.0f),
                horizontalCollapse) *
            (1.0f - smoothstep(0.90f, 1.0f, shutdown)) *
            smoothstep(0.35f, 0.75f, shutdown);
        return float4(lineGlow.xxx * 0.72f, 1.0f);
    }
    const float2 transitionUv =
        outputCentered / float2(bandWidth, bandHeight) * 0.5f + 0.5f;

    float2 centered = transitionUv * 2.0f - 1.0f;
    centered.x *= 1.0f + curvature * centered.y * centered.y;
    centered.y *= 1.0f + curvature * centered.x * centered.x;
    float2 uv = centered * 0.5f + 0.5f;
    if (any(uv < 0.0f) || any(uv > 1.0f)) {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    // ブラウン管の水平同期がわずかに乱れるような、走査線単位の横揺れ。
    // 毎フレーム完全に値を変えず24Hzで更新し、機械的すぎない動きにする。
    const float scanGroup = floor(uv.y * resolution.y * 0.5f);
    const float jitterFrame = floor(time * 24.0f);
    const float lineRandom = Random(float2(scanGroup, jitterFrame)) - 0.5f;
    const float lineWave = sin(
        uv.y * resolution.y * 0.045f + time * 17.0f);

    // 画面をゆっくり上へ流れる水平ノイズ帯。古い映像信号の同期ズレを模倣する。
    const float rollingPosition = frac(uv.y + time * 0.18f);
    const float rollingDistance = (rollingPosition - 0.5f) / 0.075f;
    const float rollingBand = exp(-rollingDistance * rollingDistance);
    const float rollingRandom =
        Random(float2(scanGroup * 0.37f, floor(time * 18.0f))) - 0.5f;
    const float damageLineJitter =
        (Random(float2(scanGroup * 2.13f, floor(time * 60.0f))) - 0.5f) *
        damageEffect * 7.0f;
    const float hitPraise = saturate(successfulHitEffect) * (1.0f - damageEffect);
    const float normalJitterSuppression = 1.0f - hitPraise * 0.45f;
    const float jitter =
        (lineRandom * 0.72f + lineWave * 0.10f +
         rollingRandom * rollingBand * 1.65f) * horizontalJitterPixels *
        normalJitterSuppression +
        damageLineJitter;
    uv.x += jitter / resolution.x;
    if (uv.x < 0.0f || uv.x > 1.0f) {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    const float damageAmount = damageEffect * damageEffect;
    const float edgeDistance = length(centered);
    const float2 chromaticDirection =
        edgeDistance > 0.0001f ? centered / edgeDistance : float2(1.0f, 0.0f);
    // 被ダメージ時は通常のRGBずれを大幅に広げ、時間変化と画面端方向の
    // 両方へ色を分離する。砂嵐ではなく映像信号そのものがずれる表現。
    const float2 damageChromaticPixels =
        float2(
            18.0f + sin(time * 52.0f) * 5.0f,
            cos(time * 37.0f) * 4.0f) * damageAmount +
        chromaticDirection * edgeDistance * 14.0f * damageAmount;
    const float2 rgbOffset =
        float2(rgbOffsetPixels / resolution.x, 0.0f) +
        damageChromaticPixels / resolution;
    float3 color;
    color.r = SampleComposited(saturate(uv + rgbOffset)).r;
    color.g = SampleComposited(saturate(
        uv + float2(
            sin(time * 31.0f) * 3.0f,
            cos(time * 29.0f) * 1.5f) * damageAmount / resolution)).g;
    color.b = SampleComposited(saturate(uv - rgbOffset)).b;

    const float scanWave = 0.5f + 0.5f * sin(uv.y * resolution.y * 3.14159265f);
    color *= 1.0f - scanlineStrength * scanWave;

    const float maskPhase = fmod(
        floor(uv.x * resolution.x / max(phosphorMaskScale, 1.0f)), 3.0f);
    float3 phosphorMask = float3(1.0f, 1.0f, 1.0f);
    if (maskPhase < 1.0f) {
        phosphorMask = float3(1.0f, 0.82f, 0.82f);
    } else if (maskPhase < 2.0f) {
        phosphorMask = float3(0.82f, 1.0f, 0.82f);
    } else {
        phosphorMask = float3(0.82f, 0.82f, 1.0f);
    }
    color *= lerp(float3(1.0f, 1.0f, 1.0f), phosphorMask, phosphorMaskStrength);

    const float vignette = 1.0f - vignetteStrength *
        smoothstep(0.35f, 1.25f, edgeDistance);
    color *= vignette;

    // 高速な粒子ノイズに、数走査線でまとまって動く横筋ノイズを混ぜる。
    const float grain = Random(
        floor(uv * resolution) +
        float2(floor(time * 30.0f), floor(time * 47.0f))) - 0.5f;
    const float horizontalNoise = Random(float2(
        floor(uv.y * resolution.y / 3.0f), floor(time * 20.0f))) - 0.5f;
    const float trackingNoise = Random(float2(
        floor(uv.x * resolution.x * 0.35f), floor(time * 55.0f))) - 0.5f;
    const float movingNoise = grain * 0.62f + horizontalNoise * 0.38f +
        trackingNoise * rollingBand * rollingNoiseStrength;
    // 命中を褒めている間は通常CRTノイズを弱め、映像が一瞬澄むようにする。
    const float hitNoiseSuppression = 1.0f - hitPraise * 0.72f;
    color += movingNoise * noiseStrength * hitNoiseSuppression;
    color += rollingBand * noiseStrength * rollingNoiseStrength * 0.22f *
        hitNoiseSuppression;

    // 色収差に短い信号の横ずれを加え、被弾の衝撃をはっきり見せる。
    const float damageSignalPulse =
        sin(uv.y * resolution.y * 0.11f + time * 72.0f) *
        damageAmount;
    color.r *= 1.0f + max(damageSignalPulse, 0.0f) * 0.18f;
    color.b *= 1.0f + max(-damageSignalPulse, 0.0f) * 0.22f;
    const float damageEdge = smoothstep(0.15f, 1.15f, edgeDistance);
    const float redAmount = damageAmount * (0.16f + damageEdge * 0.24f);
    color *= float3(
        1.0f + redAmount * 0.55f,
        1.0f - redAmount * 0.58f,
        1.0f - redAmount * 0.72f);
    color.r += redAmount * 0.30f;

    const float flicker = 1.0f - flickerStrength *
        (0.5f + 0.5f * sin(time * 38.0f));
    color *= flicker;

    // 横線へ潰れる瞬間だけ白く過露光し、ブラウン管の消灯感を作る。
    const float shutdownFlash =
        smoothstep(0.42f, 0.72f, shutdown) *
        (1.0f - smoothstep(0.78f, 0.94f, shutdown));
    color *= 1.0f + shutdownFlash * 2.4f;
    color += shutdownFlash * 0.12f;
    color *= 1.0f - smoothstep(0.92f, 1.0f, shutdown);

    return float4(saturate(color), 1.0f);
}
