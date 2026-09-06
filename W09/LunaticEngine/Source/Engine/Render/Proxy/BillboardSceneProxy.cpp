#include "Render/Proxy/BillboardSceneProxy.h"

#include "Component/BillboardComponent.h"
#include "GameFramework/AActor.h"
#include "Materials/Material.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Types/FrameContext.h"
#include "Texture/Texture2D.h"

namespace
{
	constexpr float EditorBillboardScreenScale = 0.04f;

	float ComputeEditorBillboardScale(const FFrameContext& Frame, const FVector& WorldLocation)
	{
		float Scale = 1.0f;
		if (Frame.bIsOrtho)
		{
			Scale = Frame.OrthoWidth * EditorBillboardScreenScale;
		}
		else
		{
			const float Distance = FVector::Distance(Frame.CameraPosition, WorldLocation);
			Scale = Distance * EditorBillboardScreenScale;
		}

		return (Scale < 0.01f) ? 0.01f : Scale;
	}
}

FBillboardSceneProxy::FBillboardSceneProxy(UBillboardComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	ProxyFlags |= EPrimitiveProxyFlags::PerViewportUpdate;
	ProxyFlags &= ~EPrimitiveProxyFlags::ShowAABB;

	if (InComponent->IsEditorOnly())
	{
		ProxyFlags |= EPrimitiveProxyFlags::EditorOnly;
	}

	if (!InComponent->ParticipatesInRenderSpatialStructure())
	{
		ProxyFlags |= EPrimitiveProxyFlags::NeverCull;
	}
}

UBillboardComponent* FBillboardSceneProxy::GetBillboardComponent() const
{
	return static_cast<UBillboardComponent*>(GetOwner());
}

void FBillboardSceneProxy::UpdateTransform()
{
	FPrimitiveSceneProxy::UpdateTransform();

	UBillboardComponent* Comp = GetBillboardComponent();
	CachedScale = Comp->GetWorldScale();
	CachedLocation = Comp->GetWorldLocation();
	CachedWidth = Comp->GetWidth();
	CachedHeight = Comp->GetHeight();
}

void FBillboardSceneProxy::UpdateMesh()
{
	UBillboardComponent* Comp = GetBillboardComponent();
	UTexture2D* Texture = Comp ? Comp->GetTexture() : nullptr;

	if (!Texture || !Texture->GetSRV())
	{
		MeshBuffer = GetOwner()->GetMeshBuffer();
		SectionDraws.clear();
		return;
	}

	MeshBuffer = &FMeshBufferManager::Get().GetMeshBuffer(EMeshShape::TexturedQuad);

	if (!DefaultMaterial)
	{
		DefaultMaterial = UMaterial::CreateTransient(
			ERenderPass::AlphaBlend,
			EBlendState::AlphaBlend,
			EDepthStencilState::Default,
			ERasterizerState::SolidNoCull,
			FShaderManager::Get().GetOrCreate(EShaderPath::Billboard));
	}

	DefaultMaterial->SetCachedSRV(EMaterialTextureSlot::Diffuse, Texture->GetSRV());

	const uint32 IndexCount = MeshBuffer->GetIndexBuffer().GetIndexCount();
	SectionDraws.clear();
	SectionDraws.push_back({ DefaultMaterial, 0, IndexCount });
}

void FBillboardSceneProxy::UpdatePerViewport(const FFrameContext& Frame)
{
	if (!bVisible)
	{
		return;
	}

	FVector BillboardForward = Frame.CameraForward * -1.0f;
	FMatrix RotMatrix;
	RotMatrix.SetAxes(BillboardForward, Frame.CameraRight * -1.0f, Frame.CameraUp);
	FVector BillboardScale = CachedScale;
	if (UBillboardComponent* Comp = GetBillboardComponent())
	{
		if (Comp->IsEditorOnlyComponent())
		{
			const float EditorScale = ComputeEditorBillboardScale(Frame, CachedLocation)
				* Frame.RenderOptions.ActorHelperBillboardScale;
			BillboardScale *= EditorScale;
		}
	}
	BillboardScale.Y *= CachedWidth;
	BillboardScale.Z *= CachedHeight;
	FMatrix BillboardMatrix = FMatrix::MakeScaleMatrix(BillboardScale)
		* RotMatrix * FMatrix::MakeTranslationMatrix(CachedLocation);

	PerObjectConstants = FPerObjectConstants::FromWorldMatrix(BillboardMatrix);
	MarkPerObjectCBDirty();
}
