#include "Game/Expedition/OBHelicopterSpawnLog.h"

#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarOBHelicopterSpawnLog(
		TEXT("ob.HelicopterSpawn.Log"),
		1,
		TEXT("Helicopter spawn/insertion output logging: 0=off, 1=on."),
		ECVF_Default);
}

bool OBHelicopterSpawnLog::IsEnabled()
{
	return CVarOBHelicopterSpawnLog.GetValueOnAnyThread() != 0;
}
