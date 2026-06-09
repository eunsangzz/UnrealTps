// Copyright Epic Games, Inc. All Rights Reserved.

#include "WeaponComponent.h"

#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "../Weapon/WeaponBase.h"

UWeaponComponent::UWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	EquipDefaultWeapon();
}

void UWeaponComponent::StartFire()
{
	bWantsToFire = true;
	FireOnce();
	StartFireTimer();
}

void UWeaponComponent::StopFire()
{
	bWantsToFire = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FireTimerHandle);
	}
}

void UWeaponComponent::Reload()
{
	if (!CurrentWeapon)
	{
		return;
	}

	bIsReloading = true;
	const bool bReloaded = CurrentWeapon->Reload();
	bIsReloading = false;

	if (bReloaded)
	{
		OnWeaponReloaded.Broadcast();
	}
}

bool UWeaponComponent::CanFire() const
{
	if (!CurrentWeapon || bIsReloading || !CurrentWeapon->HasAmmo())
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	return World->GetTimeSeconds() - LastFireTime >= CurrentWeapon->GetFireInterval();
}

void UWeaponComponent::EquipDefaultWeapon()
{
	if (CurrentWeapon || !GetOwner())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TSubclassOf<AWeaponBase> WeaponClass = DefaultWeaponClass;
	if (!WeaponClass)
	{
		WeaponClass = AWeaponBase::StaticClass();
	}
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.Instigator = Cast<APawn>(GetOwner());

	CurrentWeapon = World->SpawnActor<AWeaponBase>(WeaponClass, SpawnParams);
	if (!CurrentWeapon)
	{
		return;
	}

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	USceneComponent* AttachTarget = OwnerCharacter ? OwnerCharacter->GetMesh() : GetOwner()->GetRootComponent();

	CurrentWeapon->AttachToComponent(
		AttachTarget,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		WeaponSocketName);
}

void UWeaponComponent::FireOnce()
{
	if (!CanFire())
	{
		return;
	}

	if (CurrentWeapon->Fire())
	{
		LastFireTime = GetWorld()->GetTimeSeconds();
		OnWeaponFired.Broadcast();
	}
}

void UWeaponComponent::StartFireTimer()
{
	if (!bWantsToFire || !CurrentWeapon)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			FireTimerHandle,
			this,
			&UWeaponComponent::FireOnce,
			CurrentWeapon->GetFireInterval(),
			true);
	}
}
