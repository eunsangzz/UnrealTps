// Copyright Epic Games, Inc. All Rights Reserved.

#include "TPSCharacter.h"

#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "UObject/ConstructorHelpers.h"
#include "../Components/HealthComponent.h"
#include "../Components/WeaponComponent.h"

ATPSCharacter::ATPSCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->JumpZVelocity = JumpZVelocity;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.0f;
	GetCharacterMovement()->GroundFriction = 8.0f;

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BelicaMesh(
		TEXT("/Game/ParagonLtBelica/Characters/Heroes/Belica/Meshes/Belica.Belica"));
	if (BelicaMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(BelicaMesh.Object);
		GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -96.0f));
		GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	}

	static ConstructorHelpers::FClassFinder<UAnimInstance> BelicaAnimBlueprint(
		TEXT("/Game/ParagonLtBelica/Characters/Heroes/Belica/LtBelica_AnimBlueprint"));
	if (BelicaAnimBlueprint.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(BelicaAnimBlueprint.Class);
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> BelicaFireMontage(
		TEXT("/Game/ParagonLtBelica/Characters/Heroes/Belica/Animations/Primary_Fire_Med_Montage.Primary_Fire_Med_Montage"));
	FireMontage = BelicaFireMontage.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> BelicaReloadAnimation(
		TEXT("/Game/ParagonLtBelica/Characters/Heroes/Belica/Animations/Q_Ability.Q_Ability"));
	ReloadAnimation = BelicaReloadAnimation.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> BelicaAimLoop(
		TEXT("/Game/ParagonLtBelica/Characters/Heroes/Belica/Animations/Rmb_Loop.Rmb_Loop"));
	AimLoopAnimation = BelicaAimLoop.Object;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = CameraArmLength;
	CameraBoom->SocketOffset = ShoulderOffset;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = true;
	CameraBoom->ProbeSize = CameraProbeSize;
	CameraBoom->ProbeChannel = ECC_Camera;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->FieldOfView = DefaultFOV;

	WeaponComponent = CreateDefaultSubobject<UWeaponComponent>(TEXT("WeaponComponent"));
	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));

	ConfigureDefaultInput();
}

void ATPSCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateAimOffsets();

	if (bIsAiming)
	{
		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		if (AnimInstance && (!ActiveAimMontage || !AnimInstance->Montage_IsPlaying(ActiveAimMontage))
			&& !AnimInstance->IsAnyMontagePlaying())
		{
			ApplyAimAnimation();
		}
	}
}

void ATPSCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (WeaponComponent)
	{
		WeaponComponent->OnWeaponFired.AddDynamic(this, &ATPSCharacter::HandleWeaponFired);
		WeaponComponent->OnWeaponReloaded.AddDynamic(this, &ATPSCharacter::HandleWeaponReloaded);
	}

	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				Subsystem->AddMappingContext(DefaultMappingContext, InputMappingPriority);
			}
		}
	}
}

void ATPSCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);

	EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ATPSCharacter::Move);
	EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ATPSCharacter::Look);
	EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ATPSCharacter::Jump);
	EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &ATPSCharacter::StartSprint);
	EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &ATPSCharacter::StopSprint);
	EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Started, this, &ATPSCharacter::StartAim);
	EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, &ATPSCharacter::StopAim);
	EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Started, this, &ATPSCharacter::StartFire);
	EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Completed, this, &ATPSCharacter::StopFire);
	EnhancedInputComponent->BindAction(ReloadAction, ETriggerEvent::Started, this, &ATPSCharacter::Reload);
}

void ATPSCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();

	if (!Controller || MovementVector.IsNearlyZero())
	{
		return;
	}

	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, MovementVector.Y);
	AddMovementInput(RightDirection, MovementVector.X);
}

void ATPSCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (!Controller || LookAxisVector.IsNearlyZero())
	{
		return;
	}

	AddControllerYawInput(LookAxisVector.X * MouseSensitivity);
	AddControllerPitchInput(-LookAxisVector.Y * MouseSensitivity);
}

void ATPSCharacter::StartSprint()
{
	bIsSprinting = true;

	if (!bIsAiming)
	{
		GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
	}
}

void ATPSCharacter::StopSprint()
{
	bIsSprinting = false;
	GetCharacterMovement()->MaxWalkSpeed = bIsAiming ? AimWalkSpeed : WalkSpeed;
}

void ATPSCharacter::StartAim()
{
	SetAiming(true);
}

void ATPSCharacter::StopAim()
{
	SetAiming(false);
}

void ATPSCharacter::StartFire()
{
	if (WeaponComponent)
	{
		WeaponComponent->StartFire();
	}
}

void ATPSCharacter::StopFire()
{
	if (WeaponComponent)
	{
		WeaponComponent->StopFire();
	}
}

void ATPSCharacter::Reload()
{
	if (WeaponComponent)
	{
		WeaponComponent->Reload();
	}
}

void ATPSCharacter::HandleWeaponFired()
{
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		if (FireMontage)
		{
			AnimInstance->Montage_Play(FireMontage);
		}
	}
}

void ATPSCharacter::HandleWeaponReloaded()
{
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		if (ReloadAnimation)
		{
			AnimInstance->PlaySlotAnimationAsDynamicMontage(
				ReloadAnimation,
				TEXT("DefaultSlot"),
				0.15f,
				0.2f);
		}
	}
}

void ATPSCharacter::Jump()
{
	Super::Jump();
}

void ATPSCharacter::SetAiming(bool bNewAiming)
{
	bIsAiming = bNewAiming;

	if (!bIsAiming)
	{
		bIsScoped = false;
	}

	ApplyAimingState();
	ApplyCameraFOV();
	ApplyAimAnimation();
}

void ATPSCharacter::SetScoped(bool bNewScoped)
{
	bIsScoped = bNewScoped;

	if (bIsScoped)
	{
		bIsAiming = true;
	}

	ApplyCameraFOV();
}

void ATPSCharacter::ConfigureDefaultInput()
{
	MoveAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Move"));
	MoveAction->ValueType = EInputActionValueType::Axis2D;
	MoveAction->AccumulationBehavior = EInputActionAccumulationBehavior::Cumulative;

	LookAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Look"));
	LookAction->ValueType = EInputActionValueType::Axis2D;

	JumpAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Jump"));
	JumpAction->ValueType = EInputActionValueType::Boolean;

	SprintAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Sprint"));
	SprintAction->ValueType = EInputActionValueType::Boolean;

	AimAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Aim"));
	AimAction->ValueType = EInputActionValueType::Boolean;

	FireAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Fire"));
	FireAction->ValueType = EInputActionValueType::Boolean;

	ReloadAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Reload"));
	ReloadAction->ValueType = EInputActionValueType::Boolean;

	DefaultMappingContext = CreateDefaultSubobject<UInputMappingContext>(TEXT("IMC_Default"));

	UInputModifierSwizzleAxis* MoveSwizzle = CreateDefaultSubobject<UInputModifierSwizzleAxis>(TEXT("MoveSwizzle"));
	MoveSwizzle->Order = EInputAxisSwizzle::YXZ;

	UInputModifierNegate* MoveNegate = CreateDefaultSubobject<UInputModifierNegate>(TEXT("MoveNegate"));
	MoveNegate->bX = true;
	MoveNegate->bY = false;
	MoveNegate->bZ = false;

	UInputModifierNegate* MoveSwizzledNegate = CreateDefaultSubobject<UInputModifierNegate>(TEXT("MoveSwizzledNegate"));
	MoveSwizzledNegate->bX = true;
	MoveSwizzledNegate->bY = false;
	MoveSwizzledNegate->bZ = false;

	FEnhancedActionKeyMapping& MoveForward = DefaultMappingContext->MapKey(MoveAction, EKeys::W);
	MoveForward.Modifiers.Add(MoveSwizzle);

	FEnhancedActionKeyMapping& MoveBackward = DefaultMappingContext->MapKey(MoveAction, EKeys::S);
	MoveBackward.Modifiers.Add(MoveSwizzle);
	MoveBackward.Modifiers.Add(MoveSwizzledNegate);

	FEnhancedActionKeyMapping& MoveLeft = DefaultMappingContext->MapKey(MoveAction, EKeys::A);
	MoveLeft.Modifiers.Add(MoveNegate);

	DefaultMappingContext->MapKey(MoveAction, EKeys::D);
	DefaultMappingContext->MapKey(LookAction, EKeys::Mouse2D);
	DefaultMappingContext->MapKey(JumpAction, EKeys::SpaceBar);
	DefaultMappingContext->MapKey(SprintAction, EKeys::LeftShift);
	DefaultMappingContext->MapKey(AimAction, EKeys::RightMouseButton);
	DefaultMappingContext->MapKey(FireAction, EKeys::LeftMouseButton);
	DefaultMappingContext->MapKey(ReloadAction, EKeys::R);
}

void ATPSCharacter::ApplyCameraFOV()
{
	if (!FollowCamera)
	{
		return;
	}

	if (bIsScoped)
	{
		FollowCamera->SetFieldOfView(ScopeFOV);
	}
	else if (bIsAiming)
	{
		FollowCamera->SetFieldOfView(AimFOV);
	}
	else
	{
		FollowCamera->SetFieldOfView(DefaultFOV);
	}
}

void ATPSCharacter::ApplyAimingState()
{
	if (bIsAiming)
	{
		bUseControllerRotationYaw = true;
		GetCharacterMovement()->bOrientRotationToMovement = false;
		GetCharacterMovement()->MaxWalkSpeed = AimWalkSpeed;
	}
	else
	{
		bUseControllerRotationYaw = false;
		GetCharacterMovement()->bOrientRotationToMovement = true;
		GetCharacterMovement()->MaxWalkSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
	}
}

void ATPSCharacter::UpdateAimOffsets()
{
	if (!Controller)
	{
		AimYaw = 0.0f;
		AimPitch = 0.0f;
		return;
	}

	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator ActorRotation = GetActorRotation();
	const FRotator DeltaRotation = (ControlRotation - ActorRotation).GetNormalized();

	AimYaw = DeltaRotation.Yaw;
	AimPitch = DeltaRotation.Pitch;
}

void ATPSCharacter::ApplyAimAnimation()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!AnimInstance || !AimLoopAnimation)
	{
		return;
	}

	if (bIsAiming)
	{
		if (!ActiveAimMontage || !AnimInstance->Montage_IsPlaying(ActiveAimMontage))
		{
			ActiveAimMontage = AnimInstance->PlaySlotAnimationAsDynamicMontage(
				AimLoopAnimation,
				TEXT("DefaultSlot"),
				0.2f,
				0.2f,
				1.0f,
				100);
		}
	}
	else if (ActiveAimMontage)
	{
		AnimInstance->Montage_Stop(0.2f, ActiveAimMontage);
		ActiveAimMontage = nullptr;
	}
}
