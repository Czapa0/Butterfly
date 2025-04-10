cbuffer cbWorld : register(b0) //Vertex Shader constant buffer slot 0
{
	matrix worldMatrix;
};

cbuffer cbView : register(b1) //Vertex Shader constant buffer slot 1
{
	matrix viewMatrix;
	matrix invViewMatrix;
};

cbuffer cbProj : register(b2) //Vertex Shader constant buffer slot 2
{
	matrix projMatrix;
};

struct PSInput
{
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD0;
};

PSInput main( float3 pos : POSITION )
{
	PSInput o;
	o.pos = float4(pos, 1.0f);

	//TODO : 1.31. Calculate on-screen position of billboard vertex
	float4 vPos = mul(worldMatrix, float4(pos, 1.0f));
	vPos = mul(viewMatrix, vPos);
	o.pos = mul(projMatrix, vPos);

	o.tex = pos.xy;
	return o;
}