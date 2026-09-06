#include "LockOnMarkerComponent.h"
#include "Render/Proxy/BillboardSceneProxy.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"

// ============================================================
// FLockOnMarkerSceneProxy — 빌보드 프록시 + NeverCull
// ============================================================
// 빌보드와 동일한 메시/패스 라우팅을 쓰되, NeverCull 로 occlusion/frustum
// 컬링을 면제한다. 이로써 마커가 적 메시 뒤에 있어도 수집 단계에서 버려지지 않고,
// EditorIcon 오버레이 패스의 NoDepth 상태로 항상 위에 그려진다.
class FLockOnMarkerSceneProxy : public FBillboardSceneProxy
{
public:
	FLockOnMarkerSceneProxy(UBillboardComponent* InComponent)
		: FBillboardSceneProxy(InComponent)
	{
		ProxyFlags |= EPrimitiveProxyFlags::NeverCull;
	}

	ERenderPass GetRenderPass() const override { return ERenderPass::GameOverlay; }

	void UpdateMesh() override
	{
		FBillboardSceneProxy::UpdateMesh();

		for (FMeshSectionDraw& Section : SectionDraws)
		{
			Section.PassOverride = ERenderPass::GameOverlay;
		}
	}
};

FPrimitiveSceneProxy* ULockOnMarkerComponent::CreateSceneProxy()
{
	return new FLockOnMarkerSceneProxy(this);
}
