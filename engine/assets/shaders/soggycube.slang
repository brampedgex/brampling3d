import "mod/camera.slang";

struct CubeUBO {
    float4x4 model;
};

[vk::binding(0, 0)] ConstantBuffer<CameraUBO> camera;
[vk::binding(1, 0)] Sampler2D tex_sampler;

[vk::binding(0, 1)] ConstantBuffer<CubeUBO> cube;

struct VSInput {
    float3 pos;
    float3 normal;
    float2 tex_coord;
};

struct VSOutput {
    float4 pos : SV_Position;
    float3 normal;
    float2 tex_coord;
};

[shader("vertex")]
VSOutput vertex_main(VSInput in) {
    float4 pos = float4(in.pos, 1);
    pos = mul(cube.model, pos);
    pos = mul(camera.view, pos);
    pos = mul(camera.proj, pos);

    float3 normal = in.normal;
    normal = normalize(mul(cube.model, float4(normal, 0)).xyz);

    VSOutput output;
    output.pos = pos;
    output.normal = normal;
    output.tex_coord = in.tex_coord;

    return output;
}

static const float3 LIGHT_DIR = normalize(float3(-0.52, 0.76, -0.38));

[shader("fragment")]
float3 fragment_main(VSOutput in) : SV_TARGET {
    float2 uv = in.tex_coord;
    float3 albedo = tex_sampler.Sample(uv).rgb;

    float diffuse = max(dot(in.normal, LIGHT_DIR), 0);

    static const float AMBIENT = 0.15;

    return albedo * lerp(AMBIENT, 1, diffuse);
}
