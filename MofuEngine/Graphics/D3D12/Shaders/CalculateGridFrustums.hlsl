#include "Common.hlsli"

ConstantBuffer<GlobalShaderData> GlobalData : register(b0, space0);
ConstantBuffer<LightCullingDispatchParameters> DispatchParams : register(b1, space0);
RWStructuredBuffer<ConeFrustum> GridFrustumsOut : register(u0, space0);

[numthreads(TILE_SIZE, TILE_SIZE, 1)] //FIXME: why so many
void CalculateGridFrustumsCS(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= DispatchParams.NumThreads.x || DTid.y >= DispatchParams.NumThreads.y) return;

    const float2 invViewDimensions = TILE_SIZE / float2(GlobalData.RenderSizeX, GlobalData.RenderSizeY);
    const float2 topLeft = DTid.xy * invViewDimensions;
    const float2 center = topLeft + invViewDimensions * 0.5f;
    
    float3 topLeftVS = ScreenSpacePosTo3DPos(topLeft, 0, GlobalData.InvProjection).xyz; // used to calculate cone base radius
    float3 centerVS = ScreenSpacePosTo3DPos(center, 0, GlobalData.InvProjection).xyz; // cone direction
    
    const float coneBaseRadius = distance(centerVS, topLeftVS);
    const float farClipRcp = -GlobalData.InvProjection._m33; // the inverse of cone length, used to calculate the cone's UnitRadius
    ConeFrustum frustum = { normalize(centerVS), coneBaseRadius * farClipRcp };
    
    GridFrustumsOut[(DTid.y * DispatchParams.NumThreads.x) + DTid.x] = frustum;
}