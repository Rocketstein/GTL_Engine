#pragma once

/*
	
	는 Renderer에게 Draw Call 요청을 vector의 형태로 전달하는 역할을 합니다.
	Renderer가 RenderBus에 담긴 Draw Call 요청들을 처리할 수 있게 합니다.
*/

//	TODO : CoreType.h 경로 변경 요구
#include "Core/CoreTypes.h"
#include "Render/Scene/RenderCommand.h"
#include "Render/Scene/BatchedLine.h"
#include "Engine/Fonts/FontCache.h"

class FRenderBus
{
private:
	TArray<FRenderCommand> ComponentCommands;
	TArray<FRenderCommand> DepthLessCommands;
	TArray<FRenderCommand> SubUVCommands;
	//	Array로 하지 않아도 되지만, 추후를 위한 확장성을 고려하여 추가하였습니다.
	TArray<FRenderCommand> OutlineCommands;
	TArray<FRenderCommand> OverlayCommands;

	TArray<FRenderCommand> TextCommands;
	TArray<FRenderCommand> FontCommands;
	FFontCache FontCache;
	FMatrix CachedView;
	FMatrix CachedProjection;
	//	일괄적으로 처리하기 때문에 하나의 커맨드를 두었습니다.
	FRenderCommand			BatchedLineCommand;
	FBatchedLine			BatchedLine;


public:
	void Create(ID3D11Device* InDevice);
	void Release();
	void Clear();
	//	Draw Call 요청을 RenderBus에 추가하는 함수
	void AddComponentCommand(const FRenderCommand& InCommand);
	void AddSubUVCompoenetCommand(const FRenderCommand& InCommand);
	void AddDepthLessCommand(const FRenderCommand& InCommand);
	void AddTextCommand(const FRenderCommand& InCommand);
	void AddOutlineCommand(const FRenderCommand& InCommand);
	void AddOverlayCommand(const FRenderCommand& InCommand);
	void AddBatcedLineCommand(const FRenderCommand& InCommand);

	const TArray<FRenderCommand>& GetComponentCommands() const { return ComponentCommands; }
	const TArray<FRenderCommand>& GetDepthLessCommands() const { return DepthLessCommands; }
	const TArray<FRenderCommand>& GetSelectionOutlineCommands() const { return OutlineCommands; }
	const TArray<FRenderCommand>& GetOverlayCommands() const { return OverlayCommands; }
	const FRenderCommand& GetBatchLineCommand() const { return BatchedLineCommand;  }
	FBatchedLine*		  GetBatchedLine()  { return &BatchedLine; }

	const TArray<FRenderCommand>& GetFontCommands() const { return FontCommands; }
	const TArray<FRenderCommand>& GetSubUVCommands() const { return SubUVCommands; }
	const TArray<FRenderCommand>& GetTextCommands() const { return TextCommands; }
	FFontCache&	    GetFontCache() { return FontCache; }

	void  SetCachedView(const FMatrix& View){ CachedView = View; }
	void  SetCachedProjection(const FMatrix& Projection) { CachedProjection = Projection; }
	const FMatrix&	GetCachedView()	const { return CachedView; }
	const FMatrix&	GetCachedProjection() const	{ return CachedProjection; }
};

