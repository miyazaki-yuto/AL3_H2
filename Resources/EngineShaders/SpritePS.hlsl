#include "Sprite.hlsli"

SamplerState smp : register(s0);

float4 main(VSOutput input) : SV_TARGET { 
    Texture2D tex = ResourceDescriptorHeap[input.textureDescriptorIndex];
    float4 output = tex.Sample(smp, input.uv) * input.color;
    // A fully transparent texel must not write its hidden RGB into the scene
    // render target.  This is especially visible after the CRT post effect.
    clip(output.a - (1.0 / 255.0));
    return output;
}
