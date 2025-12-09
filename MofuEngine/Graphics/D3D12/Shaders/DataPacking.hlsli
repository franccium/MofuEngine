#ifndef DATA_PACKING
#define DATA_PACKING

uint2 F3NormtoU2FixedPoint(float3 v)
{
    const float maxVal = (1 << 20) - 1;
    uint3 vUint = uint3(0, 0, 0);
	
    for (uint i = 0; i < 3; i++)
    {
		// Absolute number after the point
        vUint[i] = uint(round(abs(v[i] * maxVal)));
		// Sign
        vUint[i] |= (v[i] > 0 ? (1 << 20) : 0);
    }

	// out layout: [(21 bits of X - 11 bits of Y), (unused bit, remaining 10 bits of Y, 21 bits of Z)]
    return uint2((vUint.x << 11) | (vUint.y >> 10), ((vUint.y & ((1 << 10) - 1)) << 21) | vUint.z);
}

float3 U2FixedPointToF3Norm(uint2 v)
{
    const float maxVal = (1 << 20) - 1;
	// in layout: [(21 bits of X - 11 bits of Y), (unused bit, remaining 10 bits of Y, 21 bits of Z)]
    uint3 vUint = uint3(0, 0, 0);
    vUint[0] = v.x >> 11;
    vUint[1] = ((v.x & (1 << 11) - 1) << 10) | (v.y >> 21);
    vUint[2] = (v.y & (1 << 21) - 1);
	
	// Rebuild the number: value * sign
    float3 ret;
    for (uint i = 0; i < 3; i++)
        ret[i] = (float(vUint[i] & ((1 << 20) - 1)) / maxVal) * ((vUint[i] >> 20) > 0 ? 1 : -1);

    return ret;
}

uint Pack2xFloat1(float low, float high)
{
    return uint((f32tof16(low) & 0xFFFF) | ((f32tof16(high) & 0xFFFF) << 16));
}

float2 Unpack2xFloat1(uint v)
{
    return float2(f16tof32(v & 0xFFFF), f16tof32((v >> 16) & 0xFFFF));
}

uint F4NormToUnorm4x8(float4 v)
{
    uint4 u = uint4(round(saturate(v) * 255.0));
    return (0x000000FF & u.x) | ((u.y << 8) & 0x0000FF00) | ((u.z << 16) & 0x00FF0000) | ((u.w << 24) & 0xFF000000);
}

float4 Unorm4x8ToF4Norm(uint p)
{
    return float4(float(p & 0x000000FF) / 255.0, float((p & 0x0000FF00) >> 8) / 255.0,
		float((p & 0x00FF0000) >> 16) / 255.0, float((p & 0xFF000000) >> 24) / 255.0);
}

#endif