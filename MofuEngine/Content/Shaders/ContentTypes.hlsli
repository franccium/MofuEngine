#ifndef CONTENT_COMMON
#define CONTENT_COMMON

struct EnvironmentProcessingConstants
{
    uint CubemapsSrvIndex;
    uint EnvMapSrvIndex;
    
    uint CubeMapInSize;
    uint CubeMapOutSize;
    uint SampleCount; // used for prefiltered, but also as a sign to mirror the cubemap when its == 1
    float Roughness;
    uint OutOffset;
};


#ifdef __cplusplus
#endif

#endif