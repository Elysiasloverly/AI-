#pragma once
#include "CoreMinimal.h"

/** Editor tools and development checks share the per-enemy package convention. Gameplay uses configured class references. */
namespace RogueEnemyAssetPaths
{
    inline FString EnemyClassPath(const FString& Type)
    {
        const FString Folder=Type==TEXT("Base")?TEXT("Common"):Type;
        return FString::Printf(TEXT("/Game/BP/Enemy/%s/BP_Enemy%s.BP_Enemy%s_C"),*Folder,*Type,*Type);
    }
    inline FString ProjectileClassPath(const FString& Type)
    {
        const FString Folder=Type.IsEmpty()||Type==TEXT("Boss")?TEXT("Common"):Type;
        const FString Suffix=Type.IsEmpty()?TEXT(""):TEXT("_")+Type;
        return FString::Printf(TEXT("/Game/BP/Enemy/%s/BP_EnemyProjectile%s.BP_EnemyProjectile%s_C"),*Folder,*Suffix,*Suffix);
    }
}
