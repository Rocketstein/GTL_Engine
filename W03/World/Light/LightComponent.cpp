#include "LightComponent.h"
#include "SimpleJSON/json.hpp"

DEFINE_CLASS(ULightComponent, USceneComponent)

void ULightComponent::Serialize(json::JSON& j) const {
	USceneComponent::Serialize(j);
	j["Intensity"]  = Intensity;
	j["bIsEnabled"] = bIsEnabled;
	json::JSON c = json::Object();
	c["R"] = Color.X; c["G"] = Color.Y; c["B"] = Color.Z; c["A"] = Color.W;
	j["Color"] = c;
}

void ULightComponent::Deserialize(const json::JSON& j) {
	USceneComponent::Deserialize(j);
	auto& jj = const_cast<json::JSON&>(j);
	SetIntensity(jj["Intensity"].ToFloat());
	SetEnabled(jj["bIsEnabled"].ToBool());
	auto& c = jj["Color"];
	SetColor(FVector4(c["R"].ToFloat(), c["G"].ToFloat(), c["B"].ToFloat(), c["A"].ToFloat()));
}



