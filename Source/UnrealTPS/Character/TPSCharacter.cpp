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
#include "../Weapon/WeaponBase.h"

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

	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> BelicaDodgeAnimation(
		TEXT("/Game/ParagonLtBelica/Characters/Heroes/Belica/Animations/E_Ability.E_Ability"));
	DodgeAnimation = BelicaDodgeAnimation.Object;

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

	if (HealthComponent)
	{
		HealthComponent->OnDamaged.AddDynamic(this, &ATPSCharacter::HandleDamaged);
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

	if (!Controller || LookAxisVector.IsNearlyZero())
	{
		return;
	}

	AddControllerYawInput(LookAxisVector.X * MouseSensitivity);
	AddControllerPitchInput(-LookAxisVector.Y * MouseSensitivity);
}

void ATPSCharacter::Dodge()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!bCanDodge || bIsDodging || !Movement || !Movement->IsMovingOnGround()
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

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		if (DodgeAnimation)
		{
			AnimInstance->StopAllMontages(0.1f);
			AnimInstance->PlaySlotAnimationAsDynamicMontage(
				DodgeAnimation,
				TEXT("DefaultSlot"),
				0.05f,
				0.1f,
				DodgeAnimationPlayRate);
		}
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

void ATPSCharacter::ToggleFireMode()
{
	if (WeaponComponent)
	{
		WeaponComponent->ToggleFireMode();
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
	ApplyRecoil();

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		if (FireMontage)
		{
			AnimInstance->Montage_Play(FireMontage);
		}
	}
}

void ATPSCharacter::HandleDamaged(
	UHealthComponent* Component,
	float DamageAmount,
	AController* InstigatedBy,
	AActor* DamageCauser)
{
	StartDamageFlash();
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
	ApplyScopedVisibility();
	ApplyAimAnimation();
}

void ATPSCharacter::SetScoped(bool bNewScoped)
{
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
	const float PitchKick = FMath::FRandRange(RecoilPitchMin, RecoilPitchMax) * AimScale;
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
