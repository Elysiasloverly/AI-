#include "Player/RogueCharacterVisualComponent.h"
#include "Player/RogueFloatingComponent.h"
#include "GameFramework/Actor.h"
URogueCharacterVisualComponent::URogueCharacterVisualComponent()
{
    PrimaryComponentTick.bCanEverTick=false;
}
void URogueCharacterVisualComponent::SetFloatingEnabled(bool bEnabled)
{
    if (GetOwner())
    {
        if (URogueFloatingComponent* Floating=GetOwner()->FindComponentByClass<URogueFloatingComponent>()) { Floating->SetFloatingEnabled(bEnabled); }
    }
}
