#include "AI/Spawning/EnemyDirectorSettings.h"

UEnemyDirectorSettings::UEnemyDirectorSettings()
{
	ReplicationLODLevels = {
		FEnemyReplicationLODLevel(3000.0f, 30.0f, 15.0f, 1.0f),
		FEnemyReplicationLODLevel(7000.0f, 12.0f, 6.0f, 0.5f),
		FEnemyReplicationLODLevel(15000.0f, 3.0f, 1.0f, 0.15f),
	};
}
