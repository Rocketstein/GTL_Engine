#pragma once

#include "Render/Types/RenderTypes.h"
#include "Render/Types/RenderStateTypes.h"
#include "Math/Vector.h"
#include "Core/CoreTypes.h"
#include "Render/Types/MaterialTextureSlot.h"

class FShader;
class FConstantBuffer;
struct ID3D11ShaderResourceView;
struct ID3D11Buffer;

// DrawCommand용 통합 지오메트리 버퍼 — Static MeshBuffer와 Dynamic Buffer를 단일 타입으로 취급
// 버퍼 자원 + 드로우 범위(어디부터 얼마나 그릴 것인가)를 함께 보관
struct FDrawCommandBuffer
{
	ID3D11Buffer* VB       = nullptr;
	uint32        VBStride = 0;
	ID3D11Buffer* IB       = nullptr;

	uint32 FirstIndex  = 0;              // 인덱스 시작 오프셋
	uint32 IndexCount  = 0;              // DrawIndexed 인덱스 수
	uint32 VertexCount = 0;              // IB 없을 때 Draw(VertexCount, 0)
	int32  BaseVertex  = 0;              // DrawIndexed BaseVertexLocation

	bool HasBuffers() const { return VB != nullptr; }
};

// 렌더 상태 — DepthStencil / Blend / Rasterizer를 한 단위로 묶어 비교·복사
struct FDrawCommandRenderState
{
	EDepthStencilState       DepthStencil = EDepthStencilState::Default;
	EBlendState              Blend        = EBlendState::Opaque;
	ERasterizerState         Rasterizer   = ERasterizerState::SolidBackCull;
	D3D11_PRIMITIVE_TOPOLOGY Topology     = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
};

// 셰이더 리소스 바인딩 — PerShaderCB + SRVs (per-material/section 단위로 갱신)
struct FDrawCommandBindings
{
	FConstantBuffer*          PerShaderCB[2] = {};                            // [0]=b2, [1]=b3
	ID3D11ShaderResourceView* SRVs[(int)(EMaterialTextureSlot::Max)] = {};   // t0 ~ t7
};

/*
	FDrawCommand — 드로우콜 1개에 필요한 모든 정보를 캡슐화합니다.
	UE5의 FMeshDrawCommand 패턴을 차용하여,
	PSO 상태 + Geometry + Bindings + 정렬 키를 하나의 구조체로 통합합니다.
*/
struct FDrawCommand
{
	// ===== 핵심 식별 =====
	ERenderPass  Pass      = ERenderPass::Opaque;
	FShader*     Shader    = nullptr;

	// ===== 렌더 상태 =====
	FDrawCommandRenderState RenderState;

	// ===== Geometry =====
	FDrawCommandBuffer Buffer;                        // VB + IB + 드로우 범위 (HasBuffers() == false → SV_VertexID 기반 드로우)

	// ===== Bindings =====
	FConstantBuffer*    PerObjectCB = nullptr;        // b1: Model + Color (per-proxy)
	FDrawCommandBindings Bindings;                    // PerShaderCB + SRVs (per-material)

	// ===== Sort =====
	uint64 SortKey = 0;                              // 정렬 키 (Pass → Shader → MeshBuffer → SRV)

	// Fullscreen triangle 초기화 (PostProcess 등 SV_VertexID 기반 드로우)
	void InitFullscreenTriangle(FShader* InShader, ERenderPass InPass, const FDrawCommandRenderState& InRenderState)
	{
		Shader      = InShader;
		Pass        = InPass;
		RenderState = InRenderState;
		Buffer.VertexCount = 3;
	}

	// Cmd의 Pass/Shader/Buffer.VB/Bindings.SRVs[Diffuse]로부터 SortKey 자동 생성
	void BuildSortKey(uint16 UserBits = 0)
	{
		SortKey = ComputeSortKey(Pass, Shader, Buffer.VB,
			Bindings.SRVs[(int)EMaterialTextureSlot::Diffuse], UserBits);
	}

	// ===== SortKey 생성 유틸리티 (정적) =====
	// Pass(8bit) | ShaderHash(16bit) | MeshHash(16bit) | SRVHash(16bit) | UserBits(8bit)
	static uint64 ComputeSortKey(ERenderPass InPass, const FShader* InShader,
		const void* InMeshId, const ID3D11ShaderResourceView* InSRV,
		uint16 UserBits = 0)
	{
		auto PtrHash16 = [](const void* Ptr) -> uint16
		{
			// 포인터를 16비트로 축소 — 상태 전환 그룹핑용이므로 충돌 허용
			uintptr_t Val = reinterpret_cast<uintptr_t>(Ptr);
			return static_cast<uint16>((Val >> 4) ^ (Val >> 20));
		};

		uint64 Key = 0;
		Key |= (static_cast<uint64>(InPass) & 0xFF) << 56;           // [63:56] Pass
		Key |= (static_cast<uint64>(PtrHash16(InShader))) << 40;     // [55:40] Shader
		Key |= (static_cast<uint64>(PtrHash16(InMeshId))) << 24;      // [39:24] MeshBuffer
		Key |= (static_cast<uint64>(PtrHash16(InSRV))) << 8;         // [23:8]  SRV
		Key |= (static_cast<uint64>(UserBits) & 0xFF);              // [7:0]   User
		return Key;
	}

	// UI는 텍스처 상태 변화보다 z-order 보장이 더 중요하므로 UserBits를 앞에 둔다.
	// Pass(8bit) | UI Z(16bit) | Shader(16bit) | SRV(16bit) | User(8bit)
	static uint64 ComputeUISortKey(ERenderPass InPass, uint16 PackedZOrder,
		const FShader* InShader, const ID3D11ShaderResourceView* InSRV)
	{
		auto PtrHash16 = [](const void* Ptr) -> uint16
		{
			uintptr_t Val = reinterpret_cast<uintptr_t>(Ptr);
			return static_cast<uint16>((Val >> 4) ^ (Val >> 20));
		};

		uint64 Key = 0;
		Key |= (static_cast<uint64>(InPass) & 0xFF) << 56;            // [63:56] Pass
		Key |= (static_cast<uint64>(PackedZOrder) & 0xFFFF) << 40;   // [55:40] UI Z
		Key |= (static_cast<uint64>(PtrHash16(InShader))) << 24;     // [39:24] Shader
		Key |= (static_cast<uint64>(PtrHash16(InSRV))) << 8;        // [23:8]  SRV
		return Key;
	}
};
