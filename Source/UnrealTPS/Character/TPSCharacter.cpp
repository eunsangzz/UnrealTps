// Copyright Epic Games, Inc. All Rights Reserved.

#include "TPSCharacter.h"

#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

ATPSCharacter::ATPSCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

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

	ConfigureDefaultInput();
}

void ATPSCharacter::BeginPlay()
{
	Super::BeginPlay();

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
	AddControllerPitchInput(LookAxisVector.Y * MouseSensitivity);
}

void ATPSCharacter::StartSprint()
{
	GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
}

void ATPSCharacter::StopSprint()
{
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

void ATPSCharacter::StartAim()
{
	SetAiming(true);
}

void ATPSCharacter::StopAim()
{
	SetAiming(false);
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

	ApplyCameraFOV();
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
