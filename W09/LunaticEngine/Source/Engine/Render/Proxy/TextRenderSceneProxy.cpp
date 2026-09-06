#include "Render/Proxy/TextRenderSceneProxy.h"

#include "Component/TextRenderComponent.h"
#include "Materials/Material.h"
#include "Object/ObjectFactory.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Types/FrameContext.h"
#include "Resource/ResourceManager.h"

FTextRenderSceneProxy::FTextRenderSceneProxy(UTextRenderComponent* InComponent)
	: FBillboardSceneProxy(static_cast<UBillboardComponent*>(InComponent))
{
}

FTextRenderSceneProxy::~FTextRenderSceneProxy()
{
	if (TextMaterial)
	{
		UObjectManager::Get().DestroyObject(TextMaterial);
		TextMaterial = nullptr;
	}
}

void FTextRenderSceneProxy::UpdateTransform()
{
	FBillboardSceneProxy::UpdateTransform();
}

void FTextRenderSceneProxy::UpdateMesh()
{
	MeshBuffer = GetOwner()->GetMeshBuffer();
	ProxyFlags |= EPrimitiveProxyFlags::FontBatched;

	if (!TextMaterial)
	{
		TextMaterial = UMaterial::CreateTransient(
			ERenderPass::WorldText,
			EBlendState::AlphaBlend,
			EDepthStencilState::Default,
			ERasterizerState::SolidBackCull,
			FShaderManager::Get().GetOrCreate(EShaderPath::Primitive));
	}

	SectionDraws.clear();
	if (MeshBuffer && TextMaterial)
	{
		const uint32 IdxCount = MeshBuffer->GetIndexBuffer().GetIndexCount();
		SectionDraws.push_back({ TextMaterial, 0, IdxCount });
	}

	UTextRenderComponent* TextComp = GetTextRenderComponent();
	CachedText = TextComp->GetText();
	CachedFontScale = TextComp->GetFontSize();
	CachedColor = TextComp->GetColor();
	CachedFont = FResourceManager::Get().FindFont(TextComp->GetFontName());
	if (!CachedFont)
	{
		CachedFont = TextComp->GetFont();
	}
	CachedCharWidth = TextComp->GetCharWidth();
	CachedCharHeight = TextComp->GetCharHeight();
}

UTextRenderComponent* FTextRenderSceneProxy::GetTextRenderComponent() const
{
	return static_cast<UTextRenderComponent*>(GetOwner());
}

void FTextRenderSceneProxy::UpdatePerViewport(const FFrameContext& Frame)
{
	if (CachedText.empty() || !CachedFont || !CachedFont->IsLoaded())
	{
		bVisible = false;
		return;
	}

	if (!Frame.RenderOptions.ShowFlags.bBillboardText)
	{
		bVisible = false;
		return;
	}

	if (!bVisible)
	{
		return;
	}

	UTextRenderComponent* TextComp = GetTextRenderComponent();
	if (TextComp && TextComp->IsBillboardEnabled())
	{
		FVector BillboardForward = Frame.CameraForward * -1.0f;
		FMatrix RotMatrix;
		RotMatrix.SetAxes(BillboardForward, Frame.CameraRight * -1.0f, Frame.CameraUp);
		CachedTextWorldMatrix = FMatrix::MakeScaleMatrix(CachedScale)
			* RotMatrix * FMatrix::MakeTranslationMatrix(CachedLocation);
		CachedTextRight = Frame.CameraRight;
		CachedTextUp = Frame.CameraUp;
	}
	else
	{
		CachedTextWorldMatrix = GetOwner()->GetWorldMatrix();
		CachedTextRight = TextComp ? TextComp->GetRightVector() : FVector(0.0f, 1.0f, 0.0f);
		CachedTextUp = TextComp ? TextComp->GetUpVector() : FVector(0.0f, 0.0f, 1.0f);
	}

	int32 Len = 0;
	for (size_t i = 0; i < CachedText.length(); ++i)
	{
		if ((CachedText[i] & 0xC0) != 0x80)
		{
			Len++;
		}
	}

	if (Len > 0)
	{
		const float TotalLocalWidth = Len * CachedCharWidth;
		const float CenterY = TotalLocalWidth * -0.5f;

		FMatrix ScaleMatrix = FMatrix::MakeScaleMatrix(FVector(1.0f, TotalLocalWidth, CachedCharHeight));
		FMatrix TransMatrix = FMatrix::MakeTranslationMatrix(FVector(0.0f, CenterY, 0.0f));
		FMatrix OutlineMatrix = (ScaleMatrix * TransMatrix) * CachedTextWorldMatrix;
		PerObjectConstants = FPerObjectConstants::FromWorldMatrix(OutlineMatrix);
	}
	else
	{
		PerObjectConstants = FPerObjectConstants::FromWorldMatrix(FMatrix::Identity);
	}

	MarkPerObjectCBDirty();
}
