// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponBase.generated.h"

class UStaticMeshComponent;
class UWeaponData;

UCLASS()
class UNREALTPS_API AWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	AWeaponBase();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual bool Fire();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual bool Reload();

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool HasAmmo() const { return CurrentAmmo > 0; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool HasReserveAmmo() const { return ReserveAmmo > 0; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsMagazineFull() const { return CurrentAmmo >= MaxAmmo; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetFireInterval() const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	UWeaponData* GetWeaponData() const { return WeaponData; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USceneComponent> WeaponRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USceneComponent> MuzzlePoint;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Fire")
	FVector MuzzleOffset = FVector(100.0f, 0.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Data")
	TObjectPtr<UWeaponData> WeaponData;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Weapon|Ammo")
	int32 MaxAmmo = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Ammo")
	int32 CurrentAmmo = 30;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Weapon|Ammo")
	int32 ReserveAmmo = 90;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Debug")
	bool bDrawDebugTrace = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Debug")
	float DebugTraceTime = 1.5f;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	void ApplyWeaponData(bool bResetAmmo);
};
