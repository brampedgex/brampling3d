import "mod/camera.slang";

[vk::binding(0, 0)] ConstantBuffer<CameraUBO> camera;
[vk::binding(2, 0)] SamplerCube cubemap_sampler;

struct VSInput {
    float3 dir;
};

struct VSOutput {
    float4 pos : SV_Position;
    float3 dir;
};

[shader("vertex")]
VSOutput vertex_main(VSInput in) {
    // HACK: Just scale everything by 100 to make it far away.
    // We set w to 0 to prevent the position from being translated so the skybox stays centered around the camera.
    float4 pos = float4(in.dir * 100, 0);
    pos = mul(camera.view, pos);
    // Set w back to 1 or it will break
    pos.w = 1;
    pos = mul(camera.proj, pos);

    VSOutput out;
    out.pos = pos;
    out.dir = in.dir;
    return out;
}

[shader("fragment")]
float4 fragment_main(VSOutput in) : SV_Target {
    return cubemap_sampler.Sample(in.dir);
}
