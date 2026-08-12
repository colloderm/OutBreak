#pragma once

#include "CoreMinimal.h"

namespace OBHelicopterSpawnLog
{
	OUTBREAK_API bool IsEnabled();
}

#define OB_HELICOPTER_SPAWN_LOG(...) \
	UE_CLOG(OBHelicopterSpawnLog::IsEnabled(), __VA_ARGS__)
