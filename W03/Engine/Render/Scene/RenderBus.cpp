#include "RenderBus.h"
#include "World/Mesh/MeshManager.h"

#if DEBUG

#include <iostream>

#endif

void FRenderBus::Create(ID3D11Device* InDevice)
{
	BatchedLine.Create(InDevice);
	BatchedLineCommand.MeshBuffer = BatchedLine.GetBatchedLineBuffer();

	FontCache.SetFontData();
	FontCache.Initialize();
}

void FRenderBus::Release()
{
	BatchedLine.Release();
}


void FRenderBus::Clear()
{
	ComponentCommands.clear();
	DepthLessCommands.clear();
	OverlayCommands.clear();
	OutlineCommands.clear();
	BatchedLine.Clear();
	SubUVCommands.clear();
	TextCommands.clear();
}

void FRenderBus::AddComponentCommand(const FRenderCommand& InCommand)
{
	if (InCommand.MeshBuffer == nullptr)
	{
		return;
	}

	ComponentCommands.push_back(InCommand);
}

void FRenderBus::AddSubUVCompoenetCommand(const FRenderCommand& InCommand)
{
	if (InCommand.MeshBuffer == nullptr)
	{
		return;
	}

	SubUVCommands.push_back(InCommand);
}

void FRenderBus::AddDepthLessCommand(const FRenderCommand& InCommand)
{
	if (InCommand.MeshBuffer == nullptr)
	{
		return;
	}
	DepthLessCommands.push_back(InCommand);
}

void FRenderBus::AddTextCommand(const FRenderCommand& InCommand)
{
	TextCommands.push_back(InCommand);
}

void FRenderBus::AddOutlineCommand(const FRenderCommand& InCommand)
{
	if(InCommand.MeshBuffer == nullptr)
	{
		return;
	}
	OutlineCommands.push_back(InCommand);
}

void FRenderBus::AddOverlayCommand(const FRenderCommand& InCommand)
{
	if (InCommand.MeshBuffer == nullptr)
	{
		return;
	}
	OverlayCommands.push_back(InCommand);
}

void FRenderBus::AddBatcedLineCommand(const FRenderCommand& InCommand)
{
	if (InCommand.MeshBuffer == nullptr)
	{
		return;
	}
	BatchedLineCommand = InCommand;
}


