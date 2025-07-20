#include "Common.hlsli"


// [Walter et al. 2007, "Microfacet models for refraction through rough surfaces"]
float D_GGX(float NoH, float a)
{
    float d = (NoH * a - NoH) * NoH + 1.f;
    return a * INV_PI / (d * d);
}

// [Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"]
float V_SmithGGXCorrelated(float NoV, float NoL, float a)
{
    float ggxL = NoV * sqrt((-NoL * a + NoL) * NoL + a);
    float ggxV = NoL * sqrt((-NoV * a + NoV) * NoV + a);
    return 0.5f / (ggxV + ggxL);
}

// Approximation of joint Smith term for GGX
// [Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"]
float V_SmithGGXCorrelatedApprox(float NoV, float NoL, float a)
{
    float ggxL = NoV * ((-NoL * a + NoL) + a);
    float ggxV = NoL * ((-NoV * a + NoV) + a);
    return 0.5f / (ggxV + ggxL);
}

// Sclick Fresnel
float3 F_Schlick(float3 F0, float VoH)
{
    float u = Pow5(1.f - VoH);
    float3 F90Approx = saturate(50.f * F0.g);
    return F90Approx * u + (1.f - u) * F0;
}

float3 F_Schlick(float u, float3 F0, float3 F90)
{
    return F0 + (F90 - F0) * Pow5(1.f - u);
}

float3 Diffuse_Lambert()
{
    return INV_PI;
}

// [Burley 2012, "Physically-Based Shading at Disney"]
// slightly better visuals at grazing angles
float3 Diffuse_Burley(float NoV, float NoL, float VoH, float roughness)
{
    float u = Pow5(1.f - NoV);
    float v = Pow5(1.f - NoL);

    float FD90 = 0.5f + 2.f * VoH * VoH * roughness;
    float FdV = 1.f + (u * FD90 - u);
    float FdL = 1.f + (v * FD90 - v);
    return INV_PI * FdV * FdL;
}