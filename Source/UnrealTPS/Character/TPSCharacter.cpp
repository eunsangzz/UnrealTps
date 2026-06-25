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
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "../Components/HealthComponent.h"
#include "../Components/WeaponComponent.h"
#include "../TPSGameMode.h"
#include "../Weapon/WeaponBase.h"
#include "../Weapon/WeaponData.h"

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

	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> BelicaIdleAnimation(
		TEXT("/Game/ParagonLtBelica/Characters/Heroes/Belica/Animations/HeroSelect_Idle.HeroSelect_Idle"));
	IdleAnimation = BelicaIdleAnimation.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> BelicaMoveAnimation(
		TEXT("/Game/ParagonLtBelica/Characters/Heroes/Belica/Animations/Jog_Fwd.Jog_Fwd"));
	MoveAnimation = BelicaMoveAnimation.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> BelicaFireMontage(
		TEXT("/Game/ParagonLtBelica/Characters/Heroes/Belica/Animations/Primary_Fire_Med_Montage.Primary_Fire_Med_Montage"));
	FireMontage = BelicaFireMontage.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> BelicaFireAnimation(
		TEXT("/Game/ParagonLtBelica/Characters/Heroes/Belica/Animations/Primary_Fire_Med.Primary_Fire_Med"));
	FireAnimation = BelicaFireAnimation.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> BelicaReloadAnimation(
		TEXT("/Game/ParagonLtBelica/Characters/Heroes/Belica/Animations/Q_Ability.Q_Ability"));
	ReloadAnimation = BelicaReloadAnimation.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> BelicaAimLoop(
		TEXT("/Game/ParagonLtBelica/Characters/Heroes/Belica/Animations/Rmb_Loop.Rmb_Loop"));
	AimLoopAnimation = BelicaAimLoop.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> BelicaDodgeAnimation(
		TEXT("/Game/ParagonLtBelica/Characters/Heroes/Belica/Animations/E_Ability.E_Ability"));
	DodgeAnimation = BelicaDodgeAnimation.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> BelicaDeathAnimation(
		TEXT("/Game/ParagonLtBelica/Characters/Heroes/Belica/Animations/Death_A.Death_A"));
	DeathAnimation = BelicaDeathAnimation.Object;

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
	DamageFlashLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("DamageFlashLight"));
	DamageFlashLight->SetupAttachment(RootComponent);
	DamageFlashLight->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));
	DamageFlashLight->SetLightColor(FLinearColor::Red);
	DamageFlashLight->SetIntensity(3500.0f);
	DamageFlashLight->SetAttenuationRadius(300.0f);
	DamageFlashLight->SetVisibility(false);

	DefaultGroundFriction = GetCharacterMovement()->GroundFriction;
	DefaultBrakingDeceleration = GetCharacterMovement()->BrakingDecelerationWalking;

	ConfigureDefaultInput();
}

void ATPSCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateCamera(DeltaTime);
	UpdateRecoil(DeltaTime);
	UpdateAimOffsets();
	UpdateLocomotionAnimation();

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

	RestoreLocomotionAnimation();

	if (WeaponComponent)
	{
		WeaponComponent->OnWeaponFired.AddDynamic(this, &ATPSCharacter::HandleWeaponFired);
		WeaponComponent->OnWeaponReloaded.AddDynamic(this, &ATPSCharacter::HandleWeaponReloaded);
	}

	if (HealthComponent)
	{
		HealthComponent->OnDamaged.AddDynamic(this, &ATPSCharacter::HandleDamaged);
		HealthComponent->OnDeath.AddDynamic(this, &ATPSCharacter::HandleDeath);
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
	EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &ATPSCharacter::StopMove);
	EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ATPSCharacter::Look);
	EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ATPSCharacter::Jump);
	EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	EnhancedInputComponent->BindAction(DodgeAction, ETriggerEvent::Started, this, &ATPSCharacter::Dodge);
	EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &ATPSCharacter::StartSprint);
	EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &ATPSCharacter::StopSprint);
	EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Started, this, &ATPSCharacter::HandleAimClick);
	EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Started, this, &ATPSCharacter::StartFire);
	EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Completed, this, &ATPSCharacter::StopFire);
	EnhancedInputComponent->BindAction(FireModeAction, ETriggerEvent::Started, this, &ATPSCharacter::ToggleFireMode);
	EnhancedInputComponent->BindAction(ReloadAction, ETriggerEvent::Started, this, &ATPSCharacter::Reload);
	EnhancedInputComponent->BindAction(RestartAction, ETriggerEvent::Started, this, &ATPSCharacter::RestartCombat);
}

void ATPSCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();

	if (!bCombatActive || IsDead() || !Controller || MovementVector.IsNearlyZero())
	{
		return;
	}

	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	CachedMoveDirection = (ForwardDirection * MovementVector.Y + RightDirection * MovementVector.X).GetSafeNormal();

	if (bIsDodging)
	{
		return;
	}

	AddMovementInput(ForwardDirection, MovementVector.Y);
	AddMovementInput(RightDirection, MovementVector.X);
}

void ATPSCharacter::StopMove(const FInputActionValue& Value)
{
	CachedMoveDirection = FVector::ZeroVector;
}

void ATPSCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (!bCombatActive || IsDead() || !Controller || LookAxisVector.IsNearlyZero())
	{
		return;
	}

	AddControllerYawInput(LookAxisVector.X * MouseSensitivity);
	AddControllerPitchInput(-LookAxisVector.Y * MouseSensitivity);
}

void ATPSCharacter::Dodge()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!bCombatActive || !bCanDodge || bIsDodging || !Movement || !Movement->IsMovingOnGround()
		|| (HealthComponent && HealthComponent->IsDead()))
	{
		return;
	}

	FVector DodgeDirection = CachedMoveDirection;
	if (DodgeDirection.IsNearlyZero())
	{
		DodgeDirection = GetActorForwardVector();
	}
	DodgeDirection.Z = 0.0f;
	DodgeDirection.Normalize();

	bIsDodging = true;
	bCanDodge = false;
	GetWorldTimerManager().ClearTimer(AimClickTimerHandle);
	SetAiming(false);
	StopFire();

	SetActorRotation(DodgeDirection.Rotation());
	Movement->StopMovementImmediately();
	Movement->GroundFriction = 0.0f;
	Movement->BrakingDecelerationWalking = 0.0f;
	LaunchCharacter(DodgeDirection * DodgeSpeed, true, false);

	if (HealthComponent)
	{
		HealthComponent->SetInvulnerable(true);
	}

	bool bPlayedDodgeAnimation = false;
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		if (DodgeAnimation && !GetMesh()->GetSingleNodeInstance())
		{
			AnimInstance->StopAllMontages(0.1f);
			AnimInstance->PlaySlotAnimationAsDynamicMontage(
				DodgeAnimation,
				TEXT("DefaultSlot"),
				0.05f,
				0.1f,
				DodgeAnimationPlayRate);
			bPlayedDodgeAnimation = true;
		}
	}

	if (!bPlayedDodgeAnimation)
	{
		PlayCharacterAnimation(DodgeAnimation, true, DodgeAnimationPlayRate);
	}

	GetWorldTimerManager().SetTimer(
		DodgeTimerHandle,
		this,
		&ATPSCharacter::EndDodge,
		DodgeDuration,
		false);
	GetWorldTimerManager().SetTimer(
		DodgeCooldownTimerHandle,
		this,
		&ATPSCharacter::ResetDodgeCooldown,
		DodgeCooldown,
		false);
}

void ATPSCharacter::StartSprint()
{
	if (!bCombatActive || IsDead())
	{
		return;
	}

	bIsSprinting = true;

	if (!bIsAiming && !bIsDodging)
	{
		GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
	}
}

void ATPSCharacter::StopSprint()
{
	bIsSprinting = false;

	if (!bIsDodging)
	{
		GetCharacterMovement()->MaxWalkSpeed = bIsAiming ? AimWalkSpeed : WalkSpeed;
	}
}

void ATPSCharacter::HandleAimClick()
{
	if (!bCombatActive || IsDead())
	{
		return;
	}

	if (GetWorldTimerManager().IsTimerActive(AimClickTimerHandle))
	{
		GetWorldTimerManager().ClearTimer(AimClickTimerHandle);

		if (bIsScoped)
		{
			SetAiming(false);
		}
		else
		{
			SetScoped(true);
		}
		return;
	}

	GetWorldTimerManager().SetTimer(
		AimClickTimerHandle,
		this,
		&ATPSCharacter::ResolveSingleAimClick,
		AimDoubleClickTime,
		false);
}

void ATPSCharacter::ResolveSingleAimClick()
{
	if (!bCombatActive || IsDead())
	{
		return;
	}

	if (bIsAiming)
	{
		SetAiming(false);
	}
	else
	{
		SetAiming(true);
	}
}

void ATPSCharacter::StartFire()
{
	if (bCombatActive && !IsDead() && WeaponComponent)
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

void ATPSCharacter::ToggleFireMode()
{
	if (bCombatActive && !IsDead() && WeaponComponent)
	{
		WeaponComponent->ToggleFireMode();
	}
}

void ATPSCharacter::Reload()
{
	if (bCombatActive && !IsDead() && WeaponComponent)
	{
		WeaponComponent->Reload();
	}
}

void ATPSCharacter::RestartCombat()
{
	if (ATPSGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATPSGameMode>() : nullptr)
	{
		GameMode->RestartCombat();
	}
}

void ATPSCharacter::HandleWeaponFired()
{
	ApplyRecoil();

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		if (FireMontage && !GetMesh()->GetSingleNodeInstance())
		{
			AnimInstance->Montage_Play(FireMontage);
			return;
		}
	}

	PlayCharacterAnimation(FireAnimation, true);
}

void ATPSCharacter::HandleDamaged(
	UHealthComponent* Component,
	float DamageAmount,
	AController* InstigatedBy,
	AActor* DamageCauser)
{
	StartDamageFlash();
}

void ATPSCharacter::HandleDeath(
	UHealthComponent* Component,
	AController* InstigatedBy,
	AActor* DamageCauser)
{
	GetWorldTimerManager().ClearTimer(DodgeTimerHandle);
	GetWorldTimerManager().ClearTimer(DodgeCooldownTimerHandle);
	GetWorldTimerManager().ClearTimer(AimClickTimerHandle);
	GetWorldTimerManager().ClearTimer(DamageFlashTimerHandle);
	GetWorldTimerManager().ClearTimer(AnimationTimerHandle);

	StopFire();
	bIsDodging = false;
	bCanDodge = false;
	bIsSprinting = false;
	bIsAiming = false;
	bIsScoped = false;
	ActiveAimMontage = nullptr;

	ApplyScopedVisibility();
	EndDamageFlash();

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlayDeathAnimation();

	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		PlayerController->SetIgnoreMoveInput(true);
		PlayerController->SetIgnoreLookInput(true);
	}

	if (CameraBoom && FollowCamera)
	{
		CameraBoom->TargetArmLength = CameraArmLength;
		CameraBoom->SocketOffset = ShoulderOffset;
		FollowCamera->SetFieldOfView(DefaultFOV);
	}
}

void ATPSCharacter::HandleWeaponReloaded()
{
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		if (ReloadAnimation && !GetMesh()->GetSingleNodeInstance())
		{
			AnimInstance->PlaySlotAnimationAsDynamicMontage(
				ReloadAnimation,
				TEXT("DefaultSlot"),
				0.15f,
				0.2f);
			return;
		}
	}

	PlayCharacterAnimation(ReloadAnimation, true);
}

void ATPSCharacter::Jump()
{
	if (!bCombatActive || IsDead())
	{
		return;
	}

	Super::Jump();
}

bool ATPSCharacter::IsDead() const
{
	return HealthComponent && HealthComponent->IsDead();
}

void ATPSCharacter::HandleCombatEnded()
{
	GetWorldTimerManager().ClearTimer(DodgeTimerHandle);
	GetWorldTimerManager().ClearTimer(DodgeCooldownTimerHandle);
	GetWorldTimerManager().ClearTimer(AimClickTimerHandle);
	GetWorldTimerManager().ClearTimer(AnimationTimerHandle);
	StopFire();
	bIsDodging = false;
	bIsSprinting = false;

	if (!IsDead())
	{
		SetAiming(false);
	}

	bCombatActive = false;
	bCanDodge = false;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		PlayerController->SetIgnoreMoveInput(true);
		PlayerController->SetIgnoreLookInput(true);
	}
}

void ATPSCharacter::SetAiming(bool bNewAiming)
{
	if (!bCombatActive || IsDead())
	{
		return;
	}

	bIsAiming = bNewAiming;

	if (!bIsAiming)
	{
		bIsScoped = false;
	}

	ApplyAimingState();
	ApplyScopedVisibility();
	ApplyAimAnimation();
}

void ATPSCharacter::SetScoped(bool bNewScoped)
{
	if (!bCombatActive || IsDead())
	{
		return;
	}

	bIsScoped = bNewScoped;

	if (bIsScoped)
	{
		bIsAiming = true;
	}

	ApplyAimingState();
	ApplyScopedVisibility();
	ApplyAimAnimation();
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

	DodgeAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Dodge"));
	DodgeAction->ValueType = EInputActionValueType::Boolean;

	SprintAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Sprint"));
	SprintAction->ValueType = EInputActionValueType::Boolean;

	AimAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Aim"));
	AimAction->ValueType = EInputActionValueType::Boolean;

	FireAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Fire"));
	FireAction->ValueType = EInputActionValueType::Boolean;

	ReloadAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Reload"));
	ReloadAction->ValueType = EInputActionValueType::Boolean;

	FireModeAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_FireMode"));
	FireModeAction->ValueType = EInputActionValueType::Boolean;

	RestartAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Restart"));
	RestartAction->ValueType = EInputActionValueType::Boolean;

	DefaultMappingContext = CreateDefaultSubobject<UInputMappingContext>(TEXT("IMC_Default"));

	UInputModifierSwizzleAxis* MoveSwizzle = CreateDefaultSubobject<UInputModifierSwizzleAxis>(TEXT("MoveSwizzle"));
	MoveSwizzle->Order = EInputAxisSwizzle::YXZ;

	UInputModifierNegate* MoveNegate = CreateDefaultSubobject<UInputModifierNegate>(TEXT("MoveNegate"));
	MoveNegate->bX = true;
	MoveNegate->bY = false;
	MoveNegate->bZ = false;

	UInputModifierNegate* MoveSwizzledNegate = CreateDefaultSubobject<UInputModifierNegate>(TEXT("MoveSwizzledNegate"));
	MoveSwizzledNegate->bX = false;
	MoveSwizzledNegate->bY = true;
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
	DefaultMappingContext->MapKey(DodgeAction, EKeys::SpaceBar);
	DefaultMappingContext->MapKey(SprintAction, EKeys::LeftShift);
	DefaultMappingContext->MapKey(AimAction, EKeys::RightMouseButton);
	DefaultMappingContext->MapKey(FireAction, EKeys::LeftMouseButton);
	DefaultMappingContext->MapKey(FireModeAction, EKeys::B);
	DefaultMappingContext->MapKey(ReloadAction, EKeys::R);
	DefaultMappingContext->MapKey(RestartAction, EKeys::Enter);
}

void ATPSCharacter::UpdateCamera(float DeltaTime)
{
	if (!FollowCamera || !CameraBoom)
	{
		return;
	}

	const float TargetArmLength = bIsScoped ? 0.0f : CameraArmLength;
	const FVector TargetOffset = bIsScoped ? ScopeCameraOffset : ShoulderOffset;
	const float TargetFOV = bIsScoped ? ScopeFOV : (bIsAiming ? AimFOV : DefaultFOV);

	CameraBoom->TargetArmLength = FMath::FInterpTo(
		CameraBoom->TargetArmLength,
		TargetArmLength,
		DeltaTime,
		AimTransitionSpeed);
	CameraBoom->SocketOffset = FMath::VInterpTo(
		CameraBoom->SocketOffset,
		TargetOffset,
		DeltaTime,
		AimTransitionSpeed);
	FollowCamera->SetFieldOfView(FMath::FInterpTo(
		FollowCamera->FieldOfView,
		TargetFOV,
		DeltaTime,
		AimTransitionSpeed));
}

void ATPSCharacter::UpdateRecoil(float DeltaTime)
{
	if (!Controller || (FMath::IsNearlyZero(RecoilPitchOffset) && FMath::IsNearlyZero(RecoilYawOffset)))
	{
		return;
	}

	const float NewPitchOffset = FMath::FInterpTo(RecoilPitchOffset, 0.0f, DeltaTime, RecoilRecoverySpeed);
	const float NewYawOffset = FMath::FInterpTo(RecoilYawOffset, 0.0f, DeltaTime, RecoilRecoverySpeed);

	AddControllerPitchInput(RecoilPitchOffset - NewPitchOffset);
	AddControllerYawInput(NewYawOffset - RecoilYawOffset);

	RecoilPitchOffset = NewPitchOffset;
	RecoilYawOffset = NewYawOffset;
}

void ATPSCharacter::ApplyRecoil()
{
	if (!Controller)
	{
		return;
	}

	const float AimScale = bIsScoped ? 0.55f : (bIsAiming ? 0.75f : 1.0f);
	const UWeaponData* WeaponData = WeaponComponent && WeaponComponent->CurrentWeapon
		? WeaponComponent->CurrentWeapon->GetWeaponData()
		: nullptr;
	const float RecoilPitch = WeaponData ? WeaponData->RecoilPitch : 1.0f;
	const float RecoilYaw = WeaponData ? WeaponData->RecoilYaw : 0.35f;
	const float PitchKick = RecoilPitch * AimScale;
	const float YawKick = FMath::FRandRange(-RecoilYaw, RecoilYaw) * AimScale;

	AddControllerPitchInput(-PitchKick);
	AddControllerYawInput(YawKick);
	RecoilPitchOffset += PitchKick;
	RecoilYawOffset += YawKick;
}

void ATPSCharacter::StartDamageFlash()
{
	DamageFlashAlpha = 0.45f;
	SetDamageMaterialParameters(FLinearColor::Red, 1.0f);
	DamageFlashLight->SetVisibility(true);

	GetWorldTimerManager().SetTimer(
		DamageFlashTimerHandle,
		this,
		&ATPSCharacter::EndDamageFlash,
		DamageFlashDuration,
		false);
}

void ATPSCharacter::EndDamageFlash()
{
	DamageFlashAlpha = 0.0f;
	SetDamageMaterialParameters(FLinearColor::White, 0.0f);
	DamageFlashLight->SetVisibility(false);
}

void ATPSCharacter::SetDamageMaterialParameters(const FLinearColor& Color, float Strength)
{
	USkeletalMeshComponent* CharacterMesh = GetMesh();
	if (!CharacterMesh)
	{
		return;
	}

	if (DamageMaterials.IsEmpty())
	{
		for (int32 MaterialIndex = 0; MaterialIndex < CharacterMesh->GetNumMaterials(); ++MaterialIndex)
		{
			if (UMaterialInstanceDynamic* Material = CharacterMesh->CreateAndSetMaterialInstanceDynamic(MaterialIndex))
			{
				DamageMaterials.Add(Material);
			}
		}
	}

	for (UMaterialInstanceDynamic* Material : DamageMaterials)
	{
		if (!Material)
		{
			continue;
		}

		if (Strength <= 0.0f)
		{
			Material->ClearParameterValues();
			continue;
		}

		Material->SetVectorParameterValue(TEXT("DamageColor"), Color);
		Material->SetVectorParameterValue(TEXT("TintColor"), Color);
		Material->SetVectorParameterValue(TEXT("Color"), Color);
		Material->SetScalarParameterValue(TEXT("DamageAmount"), Strength);
		Material->SetScalarParameterValue(TEXT("HitFlash"), Strength);
	}
}

void ATPSCharacter::PlayDeathAnimation()
{
	if (!DeathAnimation)
	{
		return;
	}

	if (GetMesh()->GetSingleNodeInstance())
	{
		PlayCharacterAnimation(DeathAnimation, false);
		return;
	}

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->StopAllMontages(0.05f);
		AnimInstance->PlaySlotAnimationAsDynamicMontage(
			DeathAnimation,
			TEXT("DefaultSlot"),
			0.05f,
			0.2f);
	}
}

void ATPSCharacter::PlayCharacterAnimation(UAnimationAsset* Animation, bool bRestoreLocomotion, float PlayRate)
{
	if (!Animation)
	{
		return;
	}

	bPlayingActionAnimation = true;
	GetWorldTimerManager().ClearTimer(AnimationTimerHandle);
	GetMesh()->PlayAnimation(Animation, false);

	if (UAnimSingleNodeInstance* SingleNodeInstance = GetMesh()->GetSingleNodeInstance())
	{
		SingleNodeInstance->SetPlayRate(PlayRate);
	}

	if (bRestoreLocomotion)
	{
		GetWorldTimerManager().SetTimer(
			AnimationTimerHandle,
			this,
			&ATPSCharacter::RestoreLocomotionAnimation,
			FMath::Max(0.1f, Animation->GetPlayLength()),
			false);
	}
}

void ATPSCharacter::RestoreLocomotionAnimation()
{
	bPlayingActionAnimation = false;
	UpdateLocomotionAnimation();
}

void ATPSCharacter::UpdateLocomotionAnimation()
{
	if (bPlayingActionAnimation || IsDead())
	{
		return;
	}

	UAnimationAsset* DesiredAnimation = GetVelocity().SizeSquared2D() > FMath::Square(5.0f)
		? MoveAnimation.Get()
		: IdleAnimation.Get();
	if (!DesiredAnimation)
	{
		return;
	}

	if (UAnimSingleNodeInstance* SingleNodeInstance = GetMesh()->GetSingleNodeInstance())
	{
		if (SingleNodeInstance->GetAnimationAsset() == DesiredAnimation)
		{
			return;
		}
	}

	GetMesh()->PlayAnimation(DesiredAnimation, true);
}

void ATPSCharacter::ApplyScopedVisibility()
{
	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		CharacterMesh->SetOwnerNoSee(bIsScoped);
	}

	if (WeaponComponent && WeaponComponent->CurrentWeapon && WeaponComponent->CurrentWeapon->WeaponMesh)
	{
		WeaponComponent->CurrentWeapon->WeaponMesh->SetOwnerNoSee(bIsScoped);
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

void ATPSCharacter::EndDodge()
{
	bIsDodging = false;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->GroundFriction = DefaultGroundFriction;
		Movement->BrakingDecelerationWalking = DefaultBrakingDeceleration;
		Movement->Velocity *= 0.25f;
		Movement->MaxWalkSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
	}

	if (HealthComponent)
	{
		HealthComponent->SetInvulnerable(false);
	}
}

void ATPSCharacter::ResetDodgeCooldown()
{
	bCanDodge = true;
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
