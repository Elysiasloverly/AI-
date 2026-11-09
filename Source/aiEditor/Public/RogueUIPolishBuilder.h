#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RogueUIPolishBuilder.generated.h"
class UMaterialInterface;class UWidgetBlueprint;class UFont;class UFontFace;class UTexture2D;class UMaterial;
/** One-time editor typography and menu composition; saved WBP remains editable. */
UCLASS()
class URogueUIPolishBuilder:public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Refine four saved battle HUDs in place; preserve bindings and graph behavior. */
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|UI") static bool PolishBattleHUD(UWidgetBlueprint* Blueprint,UMaterialInterface* Frame,UMaterialInterface* Fill);
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|UI") static UFont* MakeFont(const FString& Name,UFontFace* Regular,UFontFace* Semibold);
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|UI") static bool PolishTypography(UWidgetBlueprint* Blueprint,UFont* Body,UFont* Heading);
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|UI") static bool BuildMainMenu(UWidgetBlueprint* Blueprint,UFont* Body,UFont* Heading,UTexture2D* Logo);
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|UI") static void ConfigureMenuBackdrop(UMaterial* Material);
};
