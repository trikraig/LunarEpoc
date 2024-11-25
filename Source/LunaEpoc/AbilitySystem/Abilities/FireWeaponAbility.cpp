// Project Luna Epoch 


#include "FireWeaponAbility.h"

#include "LunaEpoc/LunaCharacter.h"

UFireWeaponAbility::UFireWeaponAbility()
{
}

void UFireWeaponAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                         const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                         const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	const ALunaCharacter* LunaCharacter = Cast<ALunaCharacter>(GetAvatarActorFromActorInfo());
	if (LunaCharacter && LunaCharacter->GetWeapon())
	{
	    FireWeaponLogic();
	}
    
    K2_EndAbility();
}

void UFireWeaponAbility::FireWeaponLogic() const
{
    const ALunaCharacter* LunaCharacter = Cast<ALunaCharacter>(GetAvatarActorFromActorInfo());
    if (!LunaCharacter)
    {
        UE_LOG(LogTemp, Warning, TEXT("FireWeaponLogic: Invalid character!"));
        return;
    }

    // Get placeholder weapon from character.
    AWeapon* Weapon = LunaCharacter->GetWeapon();
    if (!Weapon)
    {
        UE_LOG(LogTemp, Warning, TEXT("FireWeaponLogic: Weapon not found!"));
        return;
    }

    // Reduce ammo count
    if (Weapon->AmmoCount <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("FireWeaponLogic: Out of ammo!"));
        return;
    }
    
    Weapon->AmmoCount--;

    // Get start and end locations for the line trace
    FVector Start = Weapon->GetActorLocation();
    FVector ForwardVector = Weapon->GetActorForwardVector();
    FVector End = Start + (ForwardVector * 1000.0f);

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(LunaCharacter); // Ignore the character during the trace
    QueryParams.AddIgnoredActor(Weapon);        // Ignore the weapon itself

    // Perform the line trace
    if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, QueryParams))
    {
        if (AActor* HitActor = HitResult.GetActor())
        {
            UE_LOG(LogTemp, Log, TEXT("FireWeaponLogic: Hit Actor: %s"), *HitActor->GetName());
        }
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("FireWeaponLogic: No hit detected."));
    }

    // Draw debug line - placeholder until vfx in place.
    DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 2.0f, 0, 2.0f);
}