struct Light
{
	float4 position;
	float4 color;
};

struct Lighting
{
	float4 ambient;
	float4 surface;
	Light lights[3];
};

cbuffer cbSurfaceColor : register(b0) //Pixel Shader constant buffer slot 0 - matches slot in psBilboard.hlsl
{
	float4 surfaceColor;
}

cbuffer cbLighting : register(b1) //Pixel Shader constant buffer slot 1
{
	Lighting lighting;
}

//TODO : 0.8. Modify pixel shader input structure to match vertex shader output
struct PSInput
{
    float4 worldPos : W_POSITION;
    float4 pos : SV_POSITION;
	//float4 col : COLOR; // Remove once no longer used
    float4 nor : NORMAL;
    float4 view : W_VIEW;
};

float4 main(PSInput i) : SV_TARGET
{
	//TODO : 0.9. Calculate output color using Phong Illumination Model
    float4 col = { 0, 0, 0, 1 };
    float ka = lighting.surface[0];
    float4 kd = lighting.surface[1] * surfaceColor;
    float ks = lighting.surface[2];
    float m = lighting.surface[3];
    
    float alpha = surfaceColor.w;

    // ambient
    col += ka * lighting.ambient;
    for (int j = 0; j < 3; j++)
    {
        float4 L = normalize(lighting.lights[j].position - i.worldPos);
        float cosNL = dot(L, i.nor);
        float4 R = normalize(2 * cosNL * i.nor - L);
        float conRV = dot(i.view, R);
        
        // diffuse
        col += kd * max(0, cosNL) * lighting.lights[j].color;
        // specular
        alpha += ks * pow(max(0, conRV), m);
        col += ks * pow(max(0, conRV), m) * lighting.lights[j].color;
    }
    col.a = alpha;
    return clamp(col, 0, 1); // Replace with correct implementation
}