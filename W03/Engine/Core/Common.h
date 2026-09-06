#pragma once
#include "CoreTypes.h"
#include "Object/Object.h"

namespace common {
	namespace structs {
		struct FViewOutput {
			std::string ObjectPicked = "";
			UObject* Object;
		};
	}

	namespace constants {
		namespace Actors {
			namespace ASpotlight {
				const float DefaultConeHeight = 12.0f;
				const float DefaultRadius = 4.5f;
				const size_t DefaultCircleVertex = 100;
			}
		}
		namespace imgui {
			const float NotificationTimer = 1.5f;
		}
	}
}
