#ifndef COMMON_CONSTANTS
#define COMMON_CONSTANTS

static const float PI = 3.14159265f;
static const float TAU = 6.28318548f;
static const float E = 2.71828185f;
static const float HALF_PI = 1.57079632679f;
static const float PIDIV4 = 0.78539816339f;
static const float INV_PI = 0.31830988618f;
static const float INV_TAU = 0.15915494309f;
static const float EPSILON = 1e-5f;

static const float F32_MAX = 0x7F7FFFFF;
static const float F32_EPSILON = 1.19209290e-07F;
static const float F16_MAX = 65000.0f; // max for a F16 buffer
static const float F16_LIGHT_UNIT_SCALE = 0.0009765625f; // for storing physical light units in fp16 floats (== 2^-10)
#endif