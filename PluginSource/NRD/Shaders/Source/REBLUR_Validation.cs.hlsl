/*
Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.

NVIDIA CORPORATION and its licensors retain all intellectual property
and proprietary rights in and to this software, related documentation
and any modifications thereto. Any use, reproduction, disclosure or
distribution of this software and related documentation without an express
license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "../Include/NRD.hlsli"
#include "STL.hlsli"

#include "../Include/REBLUR/REBLUR_Config.hlsli"
#include "../Resources/REBLUR_Validation.resources.hlsli"

#include "../Include/Common.hlsli"
#include "../Include/REBLUR/REBLUR_Common.hlsli"

#define VIEWPORT_SIZE   0.25
#define OFFSET          5

/*
Viewport layout:
 0   1   2   3
 4   5   6   7
 8   9  10  11
12  13  14  15
*/

[numthreads( 16, 16, 1 )]
NRD_EXPORT void NRD_CS_MAIN( uint2 pixelPos : SV_DispatchThreadId )
{
    if( gResetHistory != 0 )
    {
        gOut_Validation[ pixelPos ] = 0;
        return;
    }

    float2 pixelUv = float2( pixelPos + 0.5 ) * gInvScreenSize;

    float2 viewportUv = frac( pixelUv / VIEWPORT_SIZE );
    float2 viewportId = floor( pixelUv / VIEWPORT_SIZE );
    float viewportIndex = viewportId.y / VIEWPORT_SIZE + viewportId.x;

    float2 viewportUvScaled = viewportUv * gResolutionScale;

    float4 normalAndRoughness = NRD_FrontEnd_UnpackNormalAndRoughness( gIn_Normal_Roughness.SampleLevel( gNearestClamp, viewportUvScaled, 0 ) );
    float viewZ = gIn_ViewZ.SampleLevel( gNearestClamp, viewportUvScaled, 0 );
    float3 mv = gIn_Mv.SampleLevel( gNearestClamp, viewportUvScaled, 0 ) * gMvScale;
    float4 diff = gIn_Diff.SampleLevel( gNearestClamp, viewportUvScaled * float2( gDiffCheckerboard != 2 ? 0.5 : 1.0, 1.0 ), 0 );
    float4 spec = gIn_Spec.SampleLevel( gNearestClamp, viewportUvScaled * float2( gSpecCheckerboard != 2 ? 0.5 : 1.0, 1.0 ), 0 );

    // See "UnpackData1"
    float4 data1 = gIn_Data1.SampleLevel( gNearestClamp, viewportUvScaled, 0 );
    if( !gHasDiffuse )
        data1.z = data1.x;
    data1.xz *= REBLUR_MAX_ACCUM_FRAME_NUM;

    uint bits;
    float2 data2 = UnpackData2( gIn_Data2[ uint2( viewportUvScaled * gScreenSize ) ], abs( viewZ ), bits );

    float3 N = normalAndRoughness.xyz;
    float roughness = normalAndRoughness.w;

    float3 Xv = STL::Geometry::ReconstructViewPosition( viewportUv, gFrustum, abs( viewZ ), gOrthoMode );
    float3 X = STL::Geometry::RotateVector( gViewToWorld, Xv );

    bool isInf = abs( viewZ ) > gDenoisingRange;

    uint4 textState = STL::Text::Init( pixelPos, viewportId * gScreenSize * VIEWPORT_SIZE + OFFSET, 1 );

    float4 result = gOut_Validation[ pixelPos ];

    // World-space normal
    if( viewportIndex == 0 )
    {
        STL::Text::Print_ch( 'N', textState );
        STL::Text::Print_ch( 'O', textState );
        STL::Text::Print_ch( 'R', textState );
        STL::Text::Print_ch( 'M', textState );
        STL::Text::Print_ch( 'A', textState );
        STL::Text::Print_ch( 'L', textState );
        STL::Text::Print_ch( 'S', textState );

        result.xyz = N * 0.5 + 0.5;
        result.w = 1.0;
    }
    // Linear roughness
    else if( viewportIndex == 1 )
    {
        STL::Text::Print_ch( 'R', textState );
        STL::Text::Print_ch( 'O', textState );
        STL::Text::Print_ch( 'U', textState );
        STL::Text::Print_ch( 'G', textState );
        STL::Text::Print_ch( 'H', textState );
        STL::Text::Print_ch( 'N', textState );
        STL::Text::Print_ch( 'E', textState );
        STL::Text::Print_ch( 'S', textState );
        STL::Text::Print_ch( 'S', textState );

        result.xyz = normalAndRoughness.w;
        result.w = 1.0;
    }
    // View Z
    else if( viewportIndex == 2 )
    {
        STL::Text::Print_ch( 'Z', textState );
        if( viewZ < 0 )
            STL::Text::Print_ch( STL::Text::Char_Minus, textState );

        float f = 0.1 * abs( viewZ ) / ( 1.0 + 0.1 * abs( viewZ ) ); // TODO: tuned for meters
        float3 color = viewZ < 0.0 ? float3( 0, 0, 1 ) : float3( 0, 1, 0 );

        result.xyz = isInf ? float3( 1, 0, 0 ) : color * f;
        result.w = 1.0;
    }
    // MV
    else if( viewportIndex == 3 )
    {
        STL::Text::Print_ch( 'M', textState );
        STL::Text::Print_ch( 'V', textState );

        float2 viewportUvPrevExpected = STL::Geometry::GetScreenUv( gWorldToClipPrev, X );

        float2 viewportUvPrev = viewportUv + mv.xy;
        if( gIsWorldSpaceMotionEnabled )
            viewportUvPrev = STL::Geometry::GetScreenUv( gWorldToClipPrev, X + mv );

        float2 uvDelta = ( viewportUvPrev - viewportUvPrevExpected ) * gRectSize;

        result.xyz = IsInScreen( viewportUvPrev ) ? float3( abs( uvDelta ), 0 ) : float3( 0, 0, 1 );
        result.w = 1.0;
    }
    // World units
    else if( viewportIndex == 4 )
    {
        STL::Text::Print_ch( 'U', textState );
        STL::Text::Print_ch( 'N', textState );
        STL::Text::Print_ch( 'I', textState );
        STL::Text::Print_ch( 'T', textState );
        STL::Text::Print_ch( 'S', textState );
        STL::Text::Print_ch( ' ', textState );
        STL::Text::Print_ch( '&', textState );
        STL::Text::Print_ch( ' ', textState );
        STL::Text::Print_ch( 'J', textState );
        STL::Text::Print_ch( 'I', textState );
        STL::Text::Print_ch( 'T', textState );
        STL::Text::Print_ch( 'T', textState );
        STL::Text::Print_ch( 'E', textState );
        STL::Text::Print_ch( 'R', textState );

        float2 dim = float2( 0.5 * gScreenSize.y * gInvScreenSize.x, 0.5 );
        float2 remappedUv = ( viewportUv - ( 1.0 - dim ) ) / dim;

        if( all( remappedUv > 0.0 ) )
        {
            float2 dimInPixels = gScreenSize * VIEWPORT_SIZE * dim;
            float2 uv = gJitter + 0.5;
            bool isValid = all( saturate( uv ) == uv );
            int2 a = int2( saturate( uv ) * dimInPixels );
            int2 b = int2( remappedUv * dimInPixels );

            if( all( abs( a - b ) <= 1 ) && isValid )
                result.xyz = 0.66;

            if( all( abs( a - b ) <= 3 ) && !isValid )
                result.xyz = float3( 1.0, 0.0, 0.0 );
        }
        else
        {
            float roundingErrorCorrection = abs( viewZ ) * 0.001;

            result.xyz = frac( X + roundingErrorCorrection ) * float( !isInf );
        }

        result.w = 1.0;
    }
    // Virtual history
    else if( viewportIndex == 7 && gHasSpecular )
    {
        STL::Text::Print_ch( 'V', textState );
        STL::Text::Print_ch( 'I', textState );
        STL::Text::Print_ch( 'R', textState );
        STL::Text::Print_ch( 'T', textState );
        STL::Text::Print_ch( 'U', textState );
        STL::Text::Print_ch( 'A', textState );
        STL::Text::Print_ch( 'L', textState );
        STL::Text::Print_ch( ' ', textState );
        STL::Text::Print_ch( 'H', textState );
        STL::Text::Print_ch( 'I', textState );
        STL::Text::Print_ch( 'S', textState );
        STL::Text::Print_ch( 'T', textState );
        STL::Text::Print_ch( 'O', textState );
        STL::Text::Print_ch( 'R', textState );
        STL::Text::Print_ch( 'Y', textState );

        result.xyz = data2.x * float( !isInf );
        result.w = 1.0;
    }
    // Diffuse frames
    else if( viewportIndex == 8 && gHasDiffuse )
    {
        STL::Text::Print_ch( 'D', textState );
        STL::Text::Print_ch( 'I', textState );
        STL::Text::Print_ch( 'F', textState );
        STL::Text::Print_ch( 'F', textState );
        STL::Text::Print_ch( ' ', textState );
        STL::Text::Print_ch( 'F', textState );
        STL::Text::Print_ch( 'R', textState );
        STL::Text::Print_ch( 'A', textState );
        STL::Text::Print_ch( 'M', textState );
        STL::Text::Print_ch( 'E', textState );
        STL::Text::Print_ch( 'S', textState );

        float diffFrameNum = 1.0 - saturate( data1.x / max( gMaxAccumulatedFrameNum, 1.0 ) ); // map history reset to red

        result.xyz = STL::Color::ColorizeZucconi( viewportUv.y > 0.95 ? 1.0 - viewportUv.x : diffFrameNum * float( !isInf ) );
        result.w = 1.0;
    }
    // Specular frames
    else if( viewportIndex == 11 && gHasSpecular )
    {
        STL::Text::Print_ch( 'S', textState );
        STL::Text::Print_ch( 'P', textState );
        STL::Text::Print_ch( 'E', textState );
        STL::Text::Print_ch( 'C', textState );
        STL::Text::Print_ch( ' ', textState );
        STL::Text::Print_ch( 'F', textState );
        STL::Text::Print_ch( 'R', textState );
        STL::Text::Print_ch( 'A', textState );
        STL::Text::Print_ch( 'M', textState );
        STL::Text::Print_ch( 'E', textState );
        STL::Text::Print_ch( 'S', textState );

        float specFrameNum = 1.0 - saturate( data1.z / max( gMaxAccumulatedFrameNum, 1.0 ) ); // map history reset to red

        result.xyz = STL::Color::ColorizeZucconi( viewportUv.y > 0.95 ? 1.0 - viewportUv.x : specFrameNum * float( !isInf ) );
        result.w = 1.0;
    }
    // Diff hitT
    else if( viewportIndex == 12 && gHasDiffuse )
    {
        STL::Text::Print_ch( 'D', textState );
        STL::Text::Print_ch( 'I', textState );
        STL::Text::Print_ch( 'F', textState );
        STL::Text::Print_ch( 'F', textState );
        STL::Text::Print_ch( ' ', textState );
        STL::Text::Print_ch( 'H', textState );
        STL::Text::Print_ch( 'I', textState );
        STL::Text::Print_ch( 'T', textState );
        STL::Text::Print_ch( 'T', textState );

        result.xyz = ( diff.w == 0.0 ? float3( 1, 0, 0 ) : diff.w ) * float( !isInf );
        result.w = 1.0;
    }
    // Spec hitT
    else if( viewportIndex == 15 && gHasSpecular )
    {
        STL::Text::Print_ch( 'S', textState );
        STL::Text::Print_ch( 'P', textState );
        STL::Text::Print_ch( 'E', textState );
        STL::Text::Print_ch( 'C', textState );
        STL::Text::Print_ch( ' ', textState );
        STL::Text::Print_ch( 'H', textState );
        STL::Text::Print_ch( 'I', textState );
        STL::Text::Print_ch( 'T', textState );
        STL::Text::Print_ch( 'T', textState );

        result.xyz = ( spec.w == 0.0 ? float3( 1, 0, 0 ) : spec.w ) * float( !isInf );
        result.w = 1.0;
    }

    // Text
    if( STL::Text::IsForeground( textState ) )
    {
        float lum = STL::Color::Luminance( result.xyz );
        result.xyz = lerp( 0.0, 1.0 - result.xyz, saturate( abs( lum - 0.5 ) / 0.25 ) ) ;

        if( viewportIndex == 12 || viewportIndex == 15 )
            result.xyz = 0.5;
    }

    // Output
    gOut_Validation[ pixelPos ] = result;
}
