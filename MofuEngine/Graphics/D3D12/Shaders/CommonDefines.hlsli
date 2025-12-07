#ifndef COMMON_DEFINES
#define COMMON_DEFINES

#define IS_DLSS_ENABLED 1
#define IS_SSSR_ENABLED 1
#define NEED_MOTION_VECTORS IS_DLSS_ENABLED || IS_SSSR_ENABLED
#define SSILVB_ONLY_AO 1

#define RAYTRACING 0
#define PATHTRACE_MAIN 0
#define PATHTRACE_SHADOWS 0

#ifdef __cplusplus
static_assert(!(RAYTRACING && (PATHTRACE_MAIN&& PATHTRACE_SHADOWS)), "Path tracing cannot be enabled with both main and shadow raytracing at the same time");
#endif

#define PARTICLE_SYSTEM_ON 1
#endif