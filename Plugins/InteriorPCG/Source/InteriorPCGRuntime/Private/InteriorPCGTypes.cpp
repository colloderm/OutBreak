#include "InteriorPCGTypes.h"

bool FInteriorPCGAssetEntry::HasValidAssetReference() const
{
	return AssetKind == EInteriorPCGAssetKind::StaticMesh ? !StaticMesh.IsNull() : !ActorClass.IsNull();
}

bool FInteriorPCGPresetItem::HasValidAssetReference() const
{
	return AssetKind == EInteriorPCGAssetKind::StaticMesh ? !StaticMesh.IsNull() : !ActorClass.IsNull();
}

FGuid FInteriorPCGPlacementMath::MakeStableGuid(FRandomStream& Stream)
{
	return FGuid(Stream.GetUnsignedInt(), Stream.GetUnsignedInt(), Stream.GetUnsignedInt(), Stream.GetUnsignedInt());
}

float FInteriorPCGPlacementMath::ResolveYaw(const EInteriorPCGRotationMode Mode, const float YawStepDegrees, FRandomStream& Stream)
{
	switch (Mode)
	{
	case EInteriorPCGRotationMode::Fixed:
		return 0.0f;

	case EInteriorPCGRotationMode::SteppedRandomYaw:
	{
		const float SafeStep = FMath::Clamp(YawStepDegrees, 1.0f, 360.0f);
		const int32 StepCount = FMath::Max(1, FMath::FloorToInt(360.0f / SafeStep));
		return static_cast<float>(Stream.RandRange(0, StepCount - 1)) * SafeStep;
	}

	case EInteriorPCGRotationMode::RandomYaw:
	default:
		return Stream.FRandRange(0.0f, 360.0f);
	}
}

FVector2D FInteriorPCGPlacementMath::NormalizeLocalXY(const FVector& LocalPosition, const FBox& LocalBounds)
{
	const FVector Size = LocalBounds.GetSize();
	return FVector2D(
		Size.X > UE_DOUBLE_SMALL_NUMBER ? (LocalPosition.X - LocalBounds.Min.X) / Size.X : 0.5,
		Size.Y > UE_DOUBLE_SMALL_NUMBER ? (LocalPosition.Y - LocalBounds.Min.Y) / Size.Y : 0.5);
}

FVector FInteriorPCGPlacementMath::DenormalizeLocalXY(const FVector2D& NormalizedPosition, const FBox& LocalBounds, const double LocalZ)
{
	const FVector Size = LocalBounds.GetSize();
	return FVector(
		LocalBounds.Min.X + NormalizedPosition.X * Size.X,
		LocalBounds.Min.Y + NormalizedPosition.Y * Size.Y,
		LocalZ);
}
