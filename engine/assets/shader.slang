struct VSInput {
    float3 pos;
    float3 col;
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
};

[shader("vertex")]
VSOutput vertex_main(VSInput in) {
    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(in.pos, 1))));
    output.col = in.col;
    return output;
}

[shader("fragment")]
float3 fragment_main(VSOutput in) : SV_TARGET {
    return float3(1, 0, 0);
}
