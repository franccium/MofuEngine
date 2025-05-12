#ifndef COMMON_TYPES
#define COMMON_TYPES

struct GlobalShaderData
{
};

struct PerObjectData
{
};

#ifdef __cplusplus
static_assert((sizeof(PerObjectData) % 16) == 0, "The PerObjectData struct has to be formatted in 16-byte chunks without any implicit padding.");
#endif

#endif