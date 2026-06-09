// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponComponent.generated.h"

class AWeaponBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWeaponFiredSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWeaponReloadedSignature);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UNREALTPS_API UWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponComponent();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void StartFire();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void StopFire();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void Reload();

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool CanFire() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<AWeaponBase> CurrentWeapon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<AWeaponBase> DefaultWeaponClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FName WeaponSocketName = TEXT("WeaponSocket");

	UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
	FOnWeaponFiredSignature OnWeaponFired;

	UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
	FOnWeaponReloadedSignature OnWeaponReloaded;

private:
	void EquipDefaultWeapon();
	void FireOnce();
	void StartFireTimer();

	FTimerHandle FireTimerHandle;
	bool bWantsToFire = false;
	bool bIsReloading = false;
	double LastFireTime = -1000.0;
};
