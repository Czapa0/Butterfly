cbuffer cbSurfaceColor : register(b0) //Pixel Shader constant buffer slot 0
{
	float4 surfaceColor;
}

struct PSInput
{
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD0;
};

float4 main(PSInput i) : SV_TARGET
{
	//TODO : 1.32. Calculate billboard pixel color
	float a = 2;
	float d = sqrt(i.tex.x * i.tex.x + i.tex.y * i.tex.y);

	float4 col = surfaceColor * max(0, 1 - d / (0.5 * a));
	
	return clamp(col, 0, 1); //Replace with correct implementation
}