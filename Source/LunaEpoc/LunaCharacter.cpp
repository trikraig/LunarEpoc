// Copyright Epic Games, Inc. All Rights Reserved.

#include "LunaCharacter.h"

//game
#include "LunaEpoc/AbilitySystem/Components/LunaAbilitySystemComponent.h"
#include "Player/LunaPlayerState.h"
//engine
#include "LunaEpoc.h"
#include "LunaPlayerController.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Materials/Material.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "Player/Components/TargetingComponent.h"
#include "UI/HUD/LunaHUD.h"
#include "UObject/ConstructorHelpers.h"

ALunaCharacter::ALunaCharacter()
{
	// Set size for player capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;

	//Movement handled in ULunaCharacterMovementComponent.

	// Create a camera boom...
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetUsingAbsoluteRotation(true); // Don't want arm to rotate when character does
	CameraBoom->TargetArmLength = 800.f;
	CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));
	CameraBoom->bDoCollisionTest = false; // Don't want to pull camera in when it collides with level

	// Create a camera...
	TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCameraComponent->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Activate ticking in order to update the cursor every frame.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	//Temporary until inventory system is working.
	Temp_Weapon = CreateDefaultSubobject<UChildActorComponent>(TEXT("Temp_Weapon"));
	Temp_Weapon->SetupAttachment(RootComponent);

	//Targeting Component
	TargetingComponent = CreateDefaultSubobject<UTargetingComponent>(TEXT("TargetingComponent"));

	//AIStimulusSource
	StimuliSource = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("StimuliSource"));

	//Interaction Collider
	InteractionCollider = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionCollider"));
	InteractionCollider->SetupAttachment(RootComponent); // Attach to the player's root component
	InteractionCollider->SetSphereRadius(300.f);         // Set the interaction range
	InteractionCollider->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionCollider->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionCollider->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);

	//Inventory Component.
	InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));
}

void ALunaCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if(GetPlayerState<ALunaPlayerState>())
	{
		// Init ability actor info for the Server.
		InitAbilityActorInfo();

		AddCharacterAbilities();
	}
}

void ALunaCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if(GetPlayerState<ALunaPlayerState>())
	{
		// Init ability actor info for the Client.
		InitAbilityActorInfo();

		// Bind player input to the AbilitySystemComponent. Also called in SetupPlayerInputComponent because of a potential race condition.
		BindASCInput();
	}
}

void ALunaCharacter::InitAbilityActorInfo()
{
	ALunaPlayerState* LunaPlayerState = GetPlayerState<ALunaPlayerState>();
	check(LunaPlayerState)
	LunaPlayerState->GetAbilitySystemComponent()->InitAbilityActorInfo(LunaPlayerState, this);
	AbilitySystemComponent = Cast<ULunaAbilitySystemComponent>(LunaPlayerState->GetAbilitySystemComponent());
	AttributeSet = LunaPlayerState->GetAttributeSet();
}

void ALunaCharacter::RotateToMouse(float DeltaSeconds)
{
	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return;
	}

	FHitResult HitResult;

	ETraceTypeQuery TraceType = UEngineTypes::ConvertToTraceType(ECC_GameTraceChannel1);
	
	// Assuming ECC_GameTraceChannel1 is your custom collision channel
	if (!PlayerController->GetHitResultUnderCursorByChannel(
			TraceType,  // Custom collision channel
			false,                  // Trace complex geometry (set to true if you want more accurate tracing)
			HitResult))
	{
		return;
	}

	// Keep the character's Z position the same to avoid looking up/down at objects
	FVector TargetLocation = HitResult.Location;
	TargetLocation.Z = GetActorLocation().Z;

	// Calculate target rotation in FRotator form
	FRotator TargetRotation = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), TargetLocation);

	// Convert to FQuat for smooth interpolation
	FQuat CurrentQuat = GetActorRotation().Quaternion();
	FQuat TargetQuat = TargetRotation.Quaternion();

	// Smoothly interpolate using Slerp
	FQuat NewQuat = FQuat::Slerp(CurrentQuat, TargetQuat, DeltaSeconds * RotationSpeed);

	// Convert back to FRotator and apply
	FRotator NewRotation = NewQuat.Rotator();
	SetActorRotation(NewRotation);
}

void ALunaCharacter::BindASCInput()
{
	if (!ASCInputBound && AbilitySystemComponent.IsValid() && IsValid(InputComponent))
	{
		const FTopLevelAssetPath AbilityEnumAssetPath = FTopLevelAssetPath(FName("/Script/LunaEpoc"), FName("EGDAbilityInputID"));
		AbilitySystemComponent->BindAbilityActivationToInputComponent(
			InputComponent,
			FGameplayAbilityInputBinds(
				FString("ConfirmTarget"),
				FString("CancelTarget"),
				AbilityEnumAssetPath,
				static_cast<int32>(EGDAbilityInputID::Confirm),
				static_cast<int32>(EGDAbilityInputID::Cancel)
			)
		);

		ASCInputBound = true;
	}
}

AWeapon* ALunaCharacter::GetWeapon() const
{
	if (Temp_Weapon && Temp_Weapon->GetChildActor())
	{
		return Cast<AWeapon>(Temp_Weapon->GetChildActor());
	}
	return nullptr;
}

// ReSharper disable once CppMemberFunctionMayBeStatic
bool ALunaCharacter::IsAlive() const
{
	// to check via attribute system if health remaining.
	return true;
}

void ALunaCharacter::BeginPlay()
{
	Super::BeginPlay();

	StimuliSource->RegisterForSense(UAISense_Sight::StaticClass());
	StimuliSource->RegisterForSense(UAISense_Hearing::StaticClass());

	//Setup HUD overlay.
	if (ALunaPlayerController* PlayerController = Cast<ALunaPlayerController>(GetController()))
	{
		if (ALunaHUD* HUD = Cast<ALunaHUD>(PlayerController->GetHUD()))
		{
			HUD->InitOverlay(PlayerController, GetPlayerState(), GetAbilitySystemComponent(), GetAttributeSet());
		}
	}
}

void ALunaCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	RotateToMouse(DeltaSeconds);
}