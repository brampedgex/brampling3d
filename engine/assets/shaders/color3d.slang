struct VSInput {
    float3 pos;
    float3 col;
    float2 tex_coord;
};

struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};
ConstantBuffer<UniformBuffer> ubo;

struct VSOutput {
    float4 pos : SV_Position;
    float3 col;
    float2 tex_coord;
};

[shader("vertex")]
VSOutput vertex_main(VSInput in) {
    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(in.pos, 1))));
    output.col = in.col;
    output.tex_coord = in.tex_coord;
    return output;
}

Sampler2D tex_sampler;

[shader("fragment")]
float3 fragment_main(VSOutput in) : SV_TARGET {
    return tex_sampler.Sample(in.tex_coord).rgb;
}
