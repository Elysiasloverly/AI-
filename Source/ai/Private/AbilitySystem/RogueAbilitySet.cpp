#include "AbilitySystem/RogueAbilitySet.h"

#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"

void URogueAbilitySet::GiveToAbilitySystem(UAbilitySystemComponent* ASC, UObject* SourceObject, TArray<FGameplayAbilitySpecHandle>* OutHandles) const
{
	if (ASC == nullptr)
	{
		return;
	}

	if (OutHandles != nullptr)
	{
		OutHandles->Reset();
		OutHandles->Reserve(Entries.Num());
	}

	for (const FRogueAbilitySetEntry& Entry : Entries)
	{
		if (Entry.AbilityClass == nullptr)
		{
			// 占位：让 OutHandles 与 Entries 保持同序同长，
			// 调用方就能用固定下标取句柄（例如"第 0 条是接触、第 1 条是远程"）。
			if (OutHandles != nullptr)
			{
				OutHandles->Add(FGameplayAbilitySpecHandle());
			}
			continue;
		}

		const FGameplayAbilitySpec Spec(Entry.AbilityClass, Entry.Level, Entry.InputID, SourceObject);
		const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);

		if (OutHandles != nullptr)
		{
			OutHandles->Add(Handle);
		}
	}
}
