#include "Core/RogueCombatFeedback.h"

#include "UI/RogueHUD.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace RogueCombatFeedback
{
	void AddDamageNumber(AActor* WorldContextActor, const FVector& WorldLocation, float Damage, bool bIsBossDamage)
	{
		if (!IsValid(WorldContextActor) || Damage <= 0.0f)
		{
			return;
		}

		APlayerController* PlayerController = UGameplayStatics::GetPlayerController(WorldContextActor, 0);
		if (PlayerController == nullptr)
		{
			return;
		}

		ARogueHUD* RogueHUD = Cast<ARogueHUD>(PlayerController->GetHUD());
		if (RogueHUD == nullptr)
		{
			return;
		}

		RogueHUD->AddDamageNumber(WorldLocation, Damage, bIsBossDamage);
	}
}
