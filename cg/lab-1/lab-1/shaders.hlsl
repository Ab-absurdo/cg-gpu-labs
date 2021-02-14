cbuffer ConstantBuffer : register(b0)
{
    matrix World;
    matrix View;
    matrix Projection;
    matrix Translation;
}

/* vertex attributes go here to input to the vertex shader */
struct vs_in {
    float4 position_local : POS;
    float4 color_local : COL;
};

/* outputs from vertex shader go here. can be interpolated to pixel shader */
struct vs_out {
    float4 position_clip : SV_POSITION; // required output of VS
    float4 color : COLOR0;
};

vs_out vs_main(vs_in input) {
    vs_out output = (vs_out)0; // zero the memory first
    output.position_clip = mul(input.position_local, World);
    output.position_clip = mul(output.position_clip, Translation);
    output.position_clip = mul(output.position_clip, View);
    output.position_clip = mul(output.position_clip, Projection);
    output.color = input.color_local;
    return output;
}

float4 ps_main(vs_out input) : SV_TARGET{
    return input.color; // must return an RGBA colour
}
