#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RogueArtCompletionBuilder.generated.h"
class UWidgetBlueprint;class URoguePresentationTheme;class UMaterialInterface;class UTexture2D;
UCLASS()
class URogueArtCompletionBuilder : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor|Art")
    static int32 RoundImages(UWidgetBlueprint* Blueprint, float Radius = 14.f);
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Art") static bool BuildOwnedItems(UWidgetBlueprint* Bar,UWidgetBlueprint* Tile,URoguePresentationTheme* Theme);
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Art") static bool AddCardIllustrations(UWidgetBlueprint* Blueprint,URoguePresentationTheme* Theme);
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Art") static bool BuildBattlePresentation(UWidgetBlueprint* Blueprint,UMaterialInterface* Seal,UMaterialInterface* Veil);
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor|Art")
    static bool AddBakaStoryArt(UWidgetBlueprint* Battle, UWidgetBlueprint* Victory, UTexture2D* Departure, UTexture2D* Ending);
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor|Art")
    static bool AddBakaRecoveryArt(UWidgetBlueprint* Defeat, UWidgetBlueprint* Revival, UTexture2D* Illustration, UMaterialInterface* CountdownSeal);
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor|Art")
    static bool AddJourneyMoments(UWidgetBlueprint* Battle, UWidgetBlueprint* Bar, UWidgetBlueprint* Victory,
        UWidgetBlueprint* Defeat, UTexture2D* Dusk, UTexture2D* Dawn, UMaterialInterface* IceBrick);
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor|Art") static bool AddCardStateSeal(UWidgetBlueprint* Blueprint, UMaterialInterface* SealMaterial);
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Art") static bool DecorateFlowPage(UWidgetBlueprint* Blueprint,UMaterialInterface* Seal);
};
