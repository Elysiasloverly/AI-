#include "RogueEditorAssetUtils.h"

#include "EditorAssetLibrary.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"

bool URogueEditorAssetUtils::SaveAsset(UObject* Asset)
{
	if (Asset == nullptr)
	{
		return false;
	}

	Asset->MarkPackageDirty();
	if (UPackage* Package = Asset->GetOutermost())
	{
		Package->SetDirtyFlag(true);
	}

	return UEditorAssetLibrary::SaveAsset(Asset->GetPathName(), false);
}

AActor* URogueEditorAssetUtils::SpawnPreviewActor(UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, FTransform Transform)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World || !World->IsGameWorld() || !ActorClass) { return nullptr; }
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AActor>(ActorClass, Transform, Params);
}
