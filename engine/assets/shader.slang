struct VSInput {
    float2 pos;
    float3 col;
};

struct VSOutput {
    float4 pos : SV_Position;
    float3 col;
};

[shader("vertex")]
VSOutput vertex_main(VSInput in) {
    VSOutput output;
    output.pos = float4(in.pos, 0, 1);
    output.col = in.col;
    return output;
}

[shader("fragment")]
float3 fragment_main(VSOutput in) : SV_TARGET {
    return in.col;
}
