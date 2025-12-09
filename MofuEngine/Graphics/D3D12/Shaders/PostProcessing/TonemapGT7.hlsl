

#define TONE_MAPPING_UCS_ICTCP  0
#define TONE_MAPPING_UCS_JZAZBZ 1
#define TONE_MAPPING_UCS TONE_MAPPING_UCS_ICTCP
#define HDR 0

// -----------------------------------------------------------------------------
// Defines the SDR reference white level used in our tone mapping (typically 250 nits).
// -----------------------------------------------------------------------------
#define GRAN_TURISMO_SDR_PAPER_WHITE 300.0f // cd/m^2

// -----------------------------------------------------------------------------
// Gran Turismo luminance-scale conversion helpers.
// In Gran Turismo, 1.0f in the linear frame-buffer space corresponds to
// REFERENCE_LUMINANCE cd/m^2 of physical luminance (typically 100 cd/m^2).
// --------------------------------------------------------
#define REFERENCE_LUMINANCE 300.0f // cd/m^2 <-> 1.0f
#if TONE_MAPPING_UCS_ICTCP
#define EXPONENT_SCALE_FACTOR 1.0f
#else
#define EXPONENT_SCALE_FACTOR 1.7f
#endif

#define FrameBufferValueToPhysical(v) (v * REFERENCE_LUMINANCE)
#define PhysicalToFrameBufferValue(v) (v / REFERENCE_LUMINANCE)

float EotfSt2084(float n, float expScaleFactor)
{
    n = saturate(n);
    
    // Base functions from SMPTE ST 2084:2014
    // Converts from normalized PQ (0-1) to absolute luminance in cd/m^2 (linear light)
    // Assumes float input; does not handle integer encoding (Annex)
    // Assumes full-range signal (0-1)
    const float m1 = 0.1593017578125f; // (2610 / 4096) / 4
    const float m2 = 78.84375f * expScaleFactor; // (2523 / 4096) * 128
    const float c1 = 0.8359375f; // 3424 / 4096
    const float c2 = 18.8515625f; // (2413 / 4096) * 32
    const float c3 = 18.6875f; // (2392 / 4096) * 32
    const float pqC = 10000.0f; // Maximum luminance supported by PQ (cd/m^2)

    // Does not handle signal range from 2084 - assumes full range (0-1)
    float np = pow(n, 1.0f / m2);
    float l = max(np - c1, 0.f);
    
    l = l / (c2 - c3 * np);
    l = pow(l, 1.0f / m1);

    // Convert absolute luminance (cd/m^2) into the frame-buffer linear scale
    return PhysicalToFrameBufferValue(l * pqC);
}

float InverseEotfSt2084(float v, float expScaleFactor)
{
    const float m1 = 0.1593017578125f;
    const float m2 = 78.84375f * expScaleFactor;
    const float c1 = 0.8359375f;
    const float c2 = 18.8515625f;
    const float c3 = 18.6875f;
    const float pqC = 10000.0f;

    float physical = FrameBufferValueToPhysical(v);
    float y = physical / pqC;

    float ym = pow(y, m1);
    return exp2(m2 * log2(c1 + c2 * ym) - log2(1.0f + c3 * ym));
}

float3 RgbToICtCp(float3 rgb)
{
    float l = (rgb[0] * 1688.0f + rgb[1] * 2146.0f + rgb[2] * 262.0f) / 4096.0f;
    float m = (rgb[0] * 683.0f + rgb[1] * 2951.0f + rgb[2] * 462.0f) / 4096.0f;
    float s = (rgb[0] * 99.0f + rgb[1] * 309.0f + rgb[2] * 3688.0f) / 4096.0f;

    float lPQ = InverseEotfSt2084(l, EXPONENT_SCALE_FACTOR);
    float mPQ = InverseEotfSt2084(m, EXPONENT_SCALE_FACTOR);
    float sPQ = InverseEotfSt2084(s, EXPONENT_SCALE_FACTOR);

    float3 ictcp;
    ictcp.x = (2048.0f * lPQ + 2048.0f * mPQ) / 4096.0f;
    ictcp.y = (6610.0f * lPQ - 13613.0f * mPQ + 7003.0f * sPQ) / 4096.0f;
    ictcp.z = (17933.0f * lPQ - 17390.0f * mPQ - 543.0f * sPQ) / 4096.0f;
    return ictcp;
}

float3 ICtCpToRgb(float3 ictcp)
{
    float l = ictcp[0] + 0.00860904f * ictcp[1] + 0.11103f * ictcp[2];
    float m = ictcp[0] - 0.00860904f * ictcp[1] - 0.11103f * ictcp[2];
    float s = ictcp[0] + 0.560031f * ictcp[1] - 0.320627f * ictcp[2];

    float lLin = EotfSt2084(l, EXPONENT_SCALE_FACTOR);
    float mLin = EotfSt2084(m, EXPONENT_SCALE_FACTOR);
    float sLin = EotfSt2084(s, EXPONENT_SCALE_FACTOR);

    float3 rgb;
    rgb.x = max(3.43661f * lLin - 2.50645f * mLin + 0.0698454f * sLin, 0.0f);
    rgb.y = max(-0.79133f * lLin + 1.9836f * mLin - 0.192271f * sLin, 0.0f);
    rgb.z = max(-0.0259499f * lLin - 0.0989137f * mLin + 1.12486f * sLin, 0.0f);
    return rgb;
}


// Jzazbz conversion
// Muhammad Safdar, Guihua Cui, Youn Jin Kim, and Ming Ronnier Luo,
// "Perceptually uniform color space for image signals including high dynamic
// range and wide gamut," Opt. Express 25, 15131-15151 (2017)
float3
RgbToJzazbz(float3 rgb)
{
    float l = rgb[0] * 0.530004f + rgb[1] * 0.355704f + rgb[2] * 0.086090f;
    float m = rgb[0] * 0.289388f + rgb[1] * 0.525395f + rgb[2] * 0.157481f;
    float s = rgb[0] * 0.091098f + rgb[1] * 0.147588f + rgb[2] * 0.734234f;

    float lPQ = InverseEotfSt2084(l, EXPONENT_SCALE_FACTOR);
    float mPQ = InverseEotfSt2084(m, EXPONENT_SCALE_FACTOR);
    float sPQ = InverseEotfSt2084(s, EXPONENT_SCALE_FACTOR);

    float iz = 0.5f * lPQ + 0.5f * mPQ;
    
    float3 jab;
    jab.x = (0.44f * iz) / (1.0f - 0.56f * iz) - 1.6295499532821566e-11f;
    jab.y = 3.524000f * lPQ - 4.066708f * mPQ + 0.542708f * sPQ;
    jab.z = 0.199076f * lPQ + 1.096799f * mPQ - 1.295875f * sPQ;
    return jab;
}

float3
JzazbzToRgb(float3 jab)
{
    float jz = jab[0] + 1.6295499532821566e-11f;
    float iz = jz / (0.44f + 0.56f * jz);
    float a = jab[1];
    float b = jab[2];

    float l = iz + a * 1.386050432715393e-1f + b * 5.804731615611869e-2f;
    float m = iz + a * -1.386050432715393e-1f + b * -5.804731615611869e-2f;
    float s = iz + a * -9.601924202631895e-2f + b * -8.118918960560390e-1f;

    float lLin = EotfSt2084(l, EXPONENT_SCALE_FACTOR);
    float mLin = EotfSt2084(m, EXPONENT_SCALE_FACTOR);
    float sLin = EotfSt2084(s, EXPONENT_SCALE_FACTOR);

    float3 rgb;
    rgb.x = lLin * 2.990669f + mLin * -2.049742f + sLin * 0.088977f;
    rgb.y = lLin * -1.634525f + mLin * 3.145627f + sLin * -0.483037f;
    rgb.z = lLin * -0.042505f + mLin * -0.377983f + sLin * 1.448019f;
    return rgb;
}



    
#if HDR
    #define SDR_CORRECTION_FACTOR 1.0f
    #define PHYSICAL_TARGET_LUMINANCE 4000
#else
    #define SDR_CORRECTION_FACTOR (1.0f / PhysicalToFrameBufferValue(GRAN_TURISMO_SDR_PAPER_WHITE))
    #define PHYSICAL_TARGET_LUMINANCE GRAN_TURISMO_SDR_PAPER_WHITE
#endif
#define FRAME_BUFFER_LUMINANCE_TARGET PhysicalToFrameBufferValue(PHYSICAL_TARGET_LUMINANCE)
#if TONE_MAPPING_UCS_JZAZBZ
static const float RgbToJzazbzX(float3 rgb)
{
    const float l = rgb[0] * 0.530004f + rgb[1] * 0.355704f + rgb[2] * 0.086090f;
    const float m = rgb[0] * 0.289388f + rgb[1] * 0.525395f + rgb[2] * 0.157481f;
    const float lPQ = InverseEotfSt2084(l, EXPONENT_SCALE_FACTOR);
    const float mPQ = InverseEotfSt2084(m, EXPONENT_SCALE_FACTOR);
    const float iz = 0.5f * lPQ + 0.5f * mPQ;

    return (0.44f * iz) / (1.0f - 0.56f * iz) - 1.6295499532821566e-11f;
}
static const float FRAME_BUFFER_LUMINANCE_TARGET_UCS = RgbToJzazbzX(FRAME_BUFFER_LUMINANCE_TARGET);
#else
static const float RgbToICtCPX(float3 rgb)
{
    const float l = (rgb[0] * 1688.0f + rgb[1] * 2146.0f + rgb[2] * 262.0f) / 4096.0f;
    const float m = (rgb[0] * 683.0f + rgb[1] * 2951.0f + rgb[2] * 462.0f) / 4096.0f;
    const float lPQ = InverseEotfSt2084(l, EXPONENT_SCALE_FACTOR);
    const float mPQ = InverseEotfSt2084(m, EXPONENT_SCALE_FACTOR);
    return (2048.0f * lPQ + 2048.0f * mPQ) / 4096.0f;
}
static const float FRAME_BUFFER_LUMINANCE_TARGET_UCS = RgbToICtCPX(FRAME_BUFFER_LUMINANCE_TARGET);
#endif

#define BLEND_RATIO 0.6f
#define FADE_START 0.98f
#define FADE_END 1.16f

struct GT7ToneMapCurve
{
    float PeakIntensity;
    float Alpha;
    float MidPoint;
    float LinearSection;
    float ToeStrength;
    float kA;
    float kB;
    float kC;
};

ConstantBuffer<GT7ToneMapCurve> Curve : register(b0, space3);

float EvaluateCurve(float x)
{
    if (x < 0.f) return 0.f;
    else if(x < Curve.LinearSection * Curve.PeakIntensity)
    {
        const float midPoint = Curve.MidPoint;
        const float weightLinear = smoothstep(0.f, midPoint, x);
        const float weightToe = 1.f - weightLinear;
        const float toeMapped = midPoint * pow(x / midPoint, Curve.ToeStrength);
        return weightToe * toeMapped + weightLinear * x;
    }
    else
    {
        return Curve.kA + Curve.kB * exp(x * Curve.kC);
    }
}

// input: linear Rec.2020 RGB (frame buffer values)
// Output: tone-mapped RGB (frame buffer values);
//         - in SDR mode: mapped to [0, 1], ready for sRGB OETF
//         - in HDR mode: mapped to [0, framebufferLuminanceTarget_], ready for PQ inverse-EOTF
// Note: framebufferLuminanceTarget_ represents the display's target peak luminance converted to a frame buffer value.
//       The returned values are suitable for applying the appropriate OETF to generate final output signal.
float3 ApplyTonemap(float3 colorRGB)
{
#if TONE_MAPPING_UCS_JZAZBZ
    float3 ucs = RgbToJzazbz(colorRGB);
#else
    float3 ucs = RgbToICtCp(colorRGB);
#endif
    float3 skewedRGB = float3(EvaluateCurve(colorRGB.x), EvaluateCurve(colorRGB.y), EvaluateCurve(colorRGB.z));
    
#if TONE_MAPPING_UCS_JZAZBZ
    float3 skewedUCS = RgbToJzazbz(skewedRGB);
#else
    float3 skewedUCS = RgbToICtCp(skewedRGB);
#endif
    const float chromaScale = 1.f - smoothstep(FADE_START, FADE_END, ucs.x / FRAME_BUFFER_LUMINANCE_TARGET_UCS);

    const float3 scaledUCS = float3(skewedUCS.x, ucs.y * chromaScale, ucs.z * chromaScale);

#if TONE_MAPPING_UCS_JZAZBZ
    float3 scaledRGB = JzazbzToRgb(scaledUCS);
#else
    float3 scaledRGB = ICtCpToRgb(scaledUCS);
#endif
    
    float3 blended = (1.f - BLEND_RATIO) * skewedRGB + BLEND_RATIO * scaledRGB;
    float3 outRGB = SDR_CORRECTION_FACTOR * min(blended, FRAME_BUFFER_LUMINANCE_TARGET);
    
    return outRGB;
}
