#include "ParticleSystemComponent.h"

#include "Component/Primitive/SkinnedMeshComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Particle/Asset/ParticleSystemManager.h"
#include "Particle/ParticleEmitter.h"
#include "Particle/ParticleLODLevel.h"
#include "Particle/ParticleSystem.h"
#include "Render/Types/MinimalViewInfo.h"
#include "Serialization/Archive.h"
#include "Render/Proxy/ParticleSystemSceneProxy.h"
#include "Materials/MaterialManager.h"
#include "Particle/ParticleDynamicData.h"
#include "Particle/ParticleModule.h"
#include "Profiling/Stats/Stats.h"

#include <chrono>
#include <cstring>

UParticleSystemComponent::~UParticleSystemComponent()
{
	ClearDynamicData();
	ClearEmitterInstances();
}

FPrimitiveSceneProxy* UParticleSystemComponent::CreateSceneProxy()
{
	return new FParticleSystemSceneProxy(this);
}

void UParticleSystemComponent::BeginPlay()
{
	UPrimitiveComponent::BeginPlay();
	ResolveTemplate();
	RecreateEmitterInstances();
}

void UParticleSystemComponent::EndPlay()
{
	ClearDynamicData();
	ClearEmitterInstances();
	UPrimitiveComponent::EndPlay();
}

void UParticleSystemComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);
	if (Ar.IsLoading())
	{
		ResolveTemplate();
		RecreateEmitterInstances();
	}
}

void UParticleSystemComponent::PostDuplicate()
{
	UPrimitiveComponent::PostDuplicate();
	ResolveTemplate();
	RecreateEmitterInstances();
}

void UParticleSystemComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (std::strcmp(PropertyName, "TemplatePath") == 0 || std::strcmp(PropertyName, "Particle System") == 0)
	{
		ResolveTemplate();
		RecreateEmitterInstances();
		MarkRenderStateDirty();
	}
}

void UParticleSystemComponent::SetTemplate(UParticleSystem* InTemplate)
{
	if (Template == InTemplate)
	{
		return;
	}

	Template = InTemplate;
	TemplatePath = Template && !Template->GetSourcePath().empty() ? Template->GetSourcePath() : FString("None");
	RecreateEmitterInstances();
	MarkRenderStateDirty();
}

void UParticleSystemComponent::ResetSystem()
{
	for (FParticleEmitterInstance* EmitterInstance : EmitterInstances)
	{
		if (EmitterInstance)
		{
			EmitterInstance->Reset();
		}
	}
	RecreateEmitterInstances();
}

uint64 UParticleSystemComponent::GetTemplateMemoryBytes() const
{
	if (!Template)
	{
		return 0;
	}

	uint64 Bytes = static_cast<uint64>(Template->GetClass()->GetSize());
	for (const UParticleEmitter* Emitter : Template->GetEmitters())
	{
		if (!Emitter)
		{
			continue;
		}

		Bytes += static_cast<uint64>(Emitter->GetClass()->GetSize());
		for (const UParticleLODLevel* LODLevel : Emitter->GetLODLevels())
		{
			if (!LODLevel)
			{
				continue;
			}

			Bytes += static_cast<uint64>(LODLevel->GetClass()->GetSize());
			if (const UParticleModule* Required = LODLevel->GetRequiredModule())
			{
				Bytes += static_cast<uint64>(Required->GetClass()->GetSize());
			}
			if (const UParticleModule* TypeData = LODLevel->GetTypeDataModule())
			{
				Bytes += static_cast<uint64>(TypeData->GetClass()->GetSize());
			}
			for (const UParticleModule* Module : LODLevel->GetModules())
			{
				if (Module)
				{
					Bytes += static_cast<uint64>(Module->GetClass()->GetSize());
				}
			}
		}
	}
	return Bytes;
}

uint64 UParticleSystemComponent::GetInstanceMemoryBytes() const
{
	uint64 Bytes = static_cast<uint64>(GetClass()->GetSize());
	for (const FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance)
		{
			Bytes += sizeof(FParticleEmitterInstance);
			Bytes += Instance->GetAllocatedMemoryBytes();
		}
	}
	return Bytes;
}

uint32 UParticleSystemComponent::GetParticleDrawCallCount() const
{
	const FParticleSystemSceneProxy* Proxy = static_cast<const FParticleSystemSceneProxy*>(GetSceneProxy());
	return Proxy ? Proxy->GetParticleDrawCallCount() : 0;
}

double UParticleSystemComponent::GetLastRenderBuildTimeMs() const
{
	const FParticleSystemSceneProxy* Proxy = static_cast<const FParticleSystemSceneProxy*>(GetSceneProxy());
	return Proxy ? Proxy->GetLastRenderBuildTimeMs() : 0.0;
}

void UParticleSystemComponent::RecreateEmitterInstances()
{
	ClearDynamicData();
	ClearEmitterInstances();

	if (!Template)
	{
		CachedTemplateVersion = 0;
		return;
	}

	Template->CacheSystemModuleInfo();
	CachedTemplateVersion = Template->GetVersion();
	CurrentLODIndex = 0;
	LastLODDistance = 0.0f;

	const TArray<UParticleEmitter*>& Emitters = Template->GetEmitters();
	EmitterInstances.reserve(Emitters.size());
	for (UParticleEmitter* Emitter : Emitters)
	{
		if (!Emitter || !Emitter->IsEnabled())
		{
			continue;
		}

		if (FParticleEmitterInstance* Instance = FParticleEmitterInstance::Create(this, Emitter))
		{
			EmitterInstances.push_back(Instance);
		}
	}
}

void UParticleSystemComponent::ClearEmitterInstances()
{
	for (FParticleEmitterInstance* EmitterInstance : EmitterInstances)
	{
		delete EmitterInstance;
	}
	EmitterInstances.clear();
}

void UParticleSystemComponent::ClearDynamicData()
{
	for (FDynamicEmitterDataBase* DynamicData : DynamicEmitterDataArray)
	{
		delete DynamicData;
	}
	DynamicEmitterDataArray.clear();
}

FString UParticleSystemComponent::GetInstanceNameForParticles() const
{
	if (const AActor* OwnerActor = GetOwner())
	{
		return OwnerActor->GetFName().ToString();
	}
	return FString();
}

bool UParticleSystemComponent::GetFloatParameter(FName Name, float& OutValue) const
{
	for (const FParticleFloatParameter& Parameter : FloatParameters)
	{
		if (Parameter.Name == Name)
		{
			OutValue = Parameter.Value;
			return true;
		}
	}
	return false;
}

bool UParticleSystemComponent::GetVectorParameter(FName Name, FVector& OutValue) const
{
	for (const FParticleVectorParameter& Parameter : VectorParameters)
	{
		if (Parameter.Name == Name)
		{
			OutValue = Parameter.Value;
			return true;
		}
	}
	return false;
}

bool UParticleSystemComponent::GetObjectParameter(FName Name, UObject*& OutValue) const
{
	for (const FParticleObjectParameter& Parameter : ObjectParameters)
	{
		if (Parameter.Name == Name && IsValid(Parameter.Value))
		{
			OutValue = Parameter.Value;
			return true;
		}
	}
	OutValue = nullptr;
	return false;
}

void UParticleSystemComponent::SetFloatParameter(FName Name, float Value)
{
	for (FParticleFloatParameter& Parameter : FloatParameters)
	{
		if (Parameter.Name == Name)
		{
			Parameter.Value = Value;
			return;
		}
	}

	FParticleFloatParameter Parameter;
	Parameter.Name = Name;
	Parameter.Value = Value;
	FloatParameters.push_back(Parameter);
}

void UParticleSystemComponent::SetVectorParameter(FName Name, const FVector& Value)
{
	for (FParticleVectorParameter& Parameter : VectorParameters)
	{
		if (Parameter.Name == Name)
		{
			Parameter.Value = Value;
			return;
		}
	}

	FParticleVectorParameter Parameter;
	Parameter.Name = Name;
	Parameter.Value = Value;
	VectorParameters.push_back(Parameter);
}

void UParticleSystemComponent::SetObjectParameter(FName Name, UObject* Value)
{
	for (FParticleObjectParameter& Parameter : ObjectParameters)
	{
		if (Parameter.Name == Name)
		{
			Parameter.Value = Value;
			return;
		}
	}

	FParticleObjectParameter Parameter;
	Parameter.Name = Name;
	Parameter.Value = Value;
	ObjectParameters.push_back(Parameter);
}

FParticleEmitterInstance* UParticleSystemComponent::FindEmitterInstanceByName(FName EmitterName) const
{
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance && Instance->GetEmitterTemplate()
			&& Instance->GetEmitterTemplate()->GetEmitterName() == EmitterName)
		{
			return Instance;
		}
	}
	return nullptr;
}

void UParticleSystemComponent::ReportParticleEvent(const FParticleEventData& EventData)
{
	ParticleEvents.push_back(EventData);
}

void UParticleSystemComponent::ClearParticleEvents()
{
	ParticleEvents.clear();
}

void UParticleSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UPrimitiveComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateBoneAttachment();
	AdvanceSimulation(DeltaTime);
}

void UParticleSystemComponent::UpdateBoneAttachment()
{
	const FString BoneName = AttachBoneName.ToString();
	if (BoneName.empty())
	{
		return;
	}

	const USkinnedMeshComponent* ParentMesh = Cast<USkinnedMeshComponent>(GetParent());
	if (!ParentMesh)
	{
		return;
	}

	const int32 BoneIndex = ParentMesh->FindBoneIndex(BoneName);
	if (BoneIndex >= 0)
	{
		SetWorldLocation(ParentMesh->GetBoneLocationByIndex(BoneIndex));
	}
}

void UParticleSystemComponent::AdvanceSimulation(float DeltaTime)
{
	ClearParticleEvents();
	LastSimulationTimeMs = 0.0;

	if (!IsActive() || !Template)
	{
		return;
	}

	const auto StartTime = std::chrono::steady_clock::now();
	SCOPE_STAT_CAT("ParticleSimulation", "Particles");

	if (CachedTemplateVersion != Template->GetVersion())
	{
		RecreateEmitterInstances();
	}

	UpdateLODSelection();
	for (FParticleEmitterInstance* EmitterInstance : EmitterInstances)
	{
		if (!EmitterInstance)
		{
			continue;
		}

		EmitterInstance->SetCurrentLODIndex(CurrentLODIndex);
		EmitterInstance->Tick(DeltaTime);
	}

	const TArray<FParticleEventData> EventsSnapshot = ParticleEvents;
	for (FParticleEmitterInstance* EmitterInstance : EmitterInstances)
	{
		if (EmitterInstance)
		{
			EmitterInstance->ProcessParticleEvents(EventsSnapshot);
		}
	}

	RebuildDynamicData();
	const auto EndTime = std::chrono::steady_clock::now();
	LastSimulationTimeMs = std::chrono::duration<double, std::milli>(EndTime - StartTime).count();
}

void UParticleSystemComponent::ResolveTemplate()
{
	if (TemplatePath.IsNull())
	{
		Template = nullptr;
		return;
	}

	Template = FParticleSystemManager::Get().Load(TemplatePath.ToString());
	if (Template)
	{
		TemplatePath.SetCachedObject(Template);
	}
}

void UParticleSystemComponent::UpdateLODSelection()
{
	if (!Template)
	{
		CurrentLODIndex = 0;
		LastLODDistance = 0.0f;
		return;
	}

	LastLODDistance = CalculateLODDistance();
	CurrentLODIndex = Template->SelectLODIndexByDistance(LastLODDistance);
}

float UParticleSystemComponent::CalculateLODDistance() const
{
	if (const UWorld* World = GetWorld())
	{
		FMinimalViewInfo POV;
		if (World->GetActivePOV(POV))
		{
			return FVector::Distance(GetWorldLocation(), POV.Location);
		}
	}
	return 0.0f;
}

void UParticleSystemComponent::RebuildDynamicData()
{
	ClearDynamicData();

	for (int32 EmitterIndex = 0; EmitterIndex < static_cast<int32>(EmitterInstances.size()); ++EmitterIndex)
	{
		const FParticleEmitterInstance* EmitterInstance = EmitterInstances[EmitterIndex];
		if (!EmitterInstance || EmitterInstance->GetActiveParticleCount() <= 0)
		{
			continue;
		}

		if (FDynamicEmitterDataBase* DynamicData = EmitterInstance->CreateDynamicData(EmitterIndex))
		{
			DynamicEmitterDataArray.push_back(DynamicData);
		}
	}

	if (FParticleSystemSceneProxy* Proxy = static_cast<FParticleSystemSceneProxy*>(GetSceneProxy()))
	{
		// 확장성을 고려해 소유권을 proxy로 넘깁니다.
		// PSC는 simulation/instance 소유, proxy는 render snapshot 소유만 담당한다.
		Proxy->UpdateDynamicData(std::move(DynamicEmitterDataArray));
		DynamicEmitterDataArray.clear();
		MarkProxyDirty(EDirtyFlag::Mesh);
	}
}
