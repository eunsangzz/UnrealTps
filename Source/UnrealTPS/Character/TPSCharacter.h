// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "TPSCharacter.generated.h"

class UCameraComponent;
class UHealthComponent;
class UAnimationAsset;
class UAnimMontage;
class UAnimSequenceBase;
class UInputAction;
class UInputMappingContext;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UWeaponComponent;
class USpringArmComponent;

UCLASS()
class UNREALTPS_API ATPSCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ATPSCharacter();

	virtual void Tick(float DeltaTime) override;
	virtual void Jump() override;

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetAiming(bool bNewAiming);

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetScoped(bool bNewScoped);

	UFUNCTION(BlueprintPure, Category = "Aiming")
	bool IsAiming() const { return bIsAiming; }

	UFUNCTION(BlueprintPure, Category = "Aiming")
	float GetAimYaw() const { return AimYaw; }

	UFUNCTION(BlueprintPure, Category = "Aiming")
	float GetAimPitch() const { return AimPitch; }

	UFUNCTION(BlueprintPure, Category = "Damage")
	float GetDamageFlashAlpha() const { return DamageFlashAlpha; }

	UFUNCTION(BlueprintPure, Category = "UI")
	UHealthComponent* GetHealthComponent() const { return HealthComponent; }

	UFUNCTION(BlueprintPure, Category = "UI")
	UWeaponComponent* GetWeaponComponent() const { return WeaponComponent; }

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void HandleCombatEnded();

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void Move(const FInputActionValue& Value);
	void StopMove(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Dodge();
	void StartSprint();
	void StopSprint();
	void HandleAimClick();
	void ResolveSingleAimClick();
	void StartFire();
	void StopFire();
	void ToggleFireMode();
	void Reload();
	void RestartCombat();

	UFUNCTION()
	void HandleWeaponFired();

	UFUNCTION()
	void HandleWeaponReloaded();

	UFUNCTION()
	void HandleDamaged(UHealthComponent* Component, float DamageAmount, AController* InstigatedBy, AActor* DamageCauser);

	UFUNCTION()
	void HandleDeath(UHealthComponent* Component, AController* InstigatedBy, AActor* DamageCauser);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UWeaponComponent> WeaponComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	TObjectPtr<UHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
	TObjectPtr<UPointLightComponent> DamageFlashLight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	float DefaultFOV = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	float AimFOV = 65.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	float ScopeFOV = 35.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	float MouseSensitivity = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	float CameraArmLength = 350.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	FVector ShoulderOffset = FVector(0.0f, 75.0f, 60.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	FVector ScopeCameraOffset = FVector(10.0f, 0.0f, 70.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "1.0"))
	float AimTransitionSpeed = 14.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Recoil", meta = (ClampMin = "0.0"))
	float RecoilPitchMin = 0.7f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Recoil", meta = (ClampMin = "0.0"))
	float RecoilPitchMax = 1.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Recoil", meta = (ClampMin = "0.0"))
	float RecoilYaw = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Recoil", meta = (ClampMin = "0.1"))
	float RecoilRecoverySpeed = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	float CameraProbeSize = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float WalkSpeed = 420.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float SprintSpeed = 720.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float AimWalkSpeed = 260.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float JumpZVelocity = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge", meta = (ClampMin = "100.0"))
	float DodgeSpeed = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge", meta = (ClampMin = "0.05"))
	float DodgeDuration = 0.45f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge", meta = (ClampMin = "0.0"))
	float DodgeCooldown = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge", meta = (ClampMin = "0.1"))
	float DodgeAnimationPlayRate = 1.35f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aiming")
	float AimYaw = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aiming")
	float AimPitch = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> DodgeAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> AimAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> FireAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ReloadAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> FireModeAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> RestartAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	int32 InputMappingPriority = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (ClampMin = "0.1", ClampMax = "0.5"))
	float AimDoubleClickTime = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> FireMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequenceBase> FireAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequenceBase> IdleAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequenceBase> MoveAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequenceBase> ReloadAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequenceBase> AimLoopAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequenceBase> DodgeAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequenceBase> DeathAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.01"))
	float DamageFlashDuration = 0.2f;

private:
	void ConfigureDefaultInput();
	void UpdateCamera(float DeltaTime);
	void UpdateRecoil(float DeltaTime);
	void ApplyRecoil();
	void ApplyScopedVisibility();
	void ApplyAimingState();
	void StartDamageFlash();
	void EndDamageFlash();
	void SetDamageMaterialParameters(const FLinearColor& Color, float Strength);
	void PlayDeathAnimation();
	void PlayCharacterAnimation(UAnimationAsset* Animation, bool bRestoreLocomotion, float PlayRate = 1.0f);
	void RestoreLocomotionAnimation();
	void UpdateLocomotionAnimation();
	void UpdateAimOffsets();
	void ApplyAimAnimation();
	void EndDodge();
	void ResetDodgeCooldown();

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveAimMontage;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DamageMaterials;

	FVector CachedMoveDirection = FVector::ZeroVector;
	float RecoilPitchOffset = 0.0f;
	float RecoilYawOffset = 0.0f;
	float DamageFlashAlpha = 0.0f;
	float DefaultGroundFriction = 8.0f;
	float DefaultBrakingDeceleration = 2000.0f;
	FTimerHandle DodgeTimerHandle;
	FTimerHandle DodgeCooldownTimerHandle;
	FTimerHandle AimClickTimerHandle;
	FTimerHandle DamageFlashTimerHandle;
	FTimerHandle AnimationTimerHandle;
	bool bIsDodging = false;
	bool bCanDodge = true;
	bool bIsSprinting = false;
	bool bIsAiming = false;
	bool bIsScoped = false;
	bool bCombatActive = true;
	bool bPlayingActionAnimation = false;
};
