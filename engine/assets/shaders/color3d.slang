struct CameraUBO {
    float4x4 view;
    float4x4 proj;
};

struct CubeUBO {
    float4x4 model;
};

[vk::binding(0, 0)]
ConstantBuffer<CameraUBO> camera;

[vk::binding(0, 1)]
ConstantBuffer<CubeUBO> cube;

struct VSInput {
    float3 pos;
    float3 col;
    float2 tex_coord;
};

struct VSOutput {
    float4 pos : SV_Position;
    float3 col;
    float2 tex_coord;
};

[shader("vertex")]
VSOutput vertex_main(VSInput in) {
    float4 pos = float4(in.pos, 1);
    pos = mul(cube.model, pos);
    pos = mul(camera.view, pos);
    pos = mul(camera.proj, pos);

    VSOutput output;
    output.pos = pos;
    output.col = in.col;
    output.tex_coord = in.tex_coord;

    return output;
}

[vk::binding(1, 0)]
Sampler2D tex_sampler;

[shader("fragment")]
float3 fragment_main(VSOutput in) : SV_TARGET {
    float2 uv = in.tex_coord;
    return tex_sampler.Sample(uv).rgb * in.col;
}
