// PhysicsBody.hlsl — PhysicsAsset 솔리드 바디(반투명) 셰이딩.
// 정점은 이미 월드 공간(프록시에서 미리 변환). 단일 고정 키 라이트로 N·L 셰이딩해
// 시점에 무관하게 형태가 입체적으로 읽히게 한다. 색/알파는 정점 컬러로 전달.
#include "Common/ConstantBuffers.hlsli"
#include "Common/VertexLayouts.hlsli"

PS_Input_LitColor VS(VS_Input_PNC input)
{
    PS_Input_LitColor output;

    float3 worldPos = input.position;
    float4 viewPos = mul(float4(worldPos, 1.0f), View);
    output.position = mul(viewPos, Projection);

    output.normal = input.normal;   // 이미 월드 공간 법선
    output.color = input.color;
    return output;
}

float4 PS(PS_Input_LitColor input) : SV_TARGET
{
    float3 N = normalize(input.normal);

    // 월드 고정 키 라이트. 양면(SolidNoCull) 렌더이므로 abs 로 뒷면도 어둡게 죽지 않게.
    const float3 L = normalize(float3(0.3f, 0.5f, 1.0f));
    float ndotl = abs(dot(N, L));
    float lighting = 0.35f + 0.65f * ndotl;   // ambient + diffuse

    return float4(input.color.rgb * lighting, input.color.a);
}
