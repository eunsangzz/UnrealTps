// Copyright Epic Games, Inc. All Rights Reserved.

#include "WeaponBase.h"

#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogTPSWeapon, Log, All);

AWeaponBase::AWeaponBase()
{
	PrimaryActorTick.bCanEverTick = false;

	WeaponRoot = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponRoot"));
	SetRootComponent(WeaponRoot);

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(WeaponRoot);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AWeaponBase::BeginPlay()
{
	Super::BeginPlay();

	CurrentAmmo = FMath::Clamp(CurrentAmmo, 0, MaxAmmo);
}

bool AWeaponBase::Fire()
{
	if (!HasAmmo())
	{
		return false;
	}

	--CurrentAmmo;

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	AController* InstigatorController = OwnerPawn ? OwnerPawn->GetController() : nullptr;

	FVector ViewLocation = GetActorLocation();
	FRotator ViewRotation = GetActorRotation();
	if (InstigatorController)
	{
		InstigatorController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}

	const FVector TraceStart = ViewLocation;
	const FVector ShotDirection = ViewRotation.Vector();
	const FVector TraceEnd = TraceStart + ShotDirection * FireRange;

	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WeaponFire), true);
	QueryParams.AddIgnoredActor(this);
	if (OwnerPawn)
	{
		QueryParams.AddIgnoredActor(OwnerPawn);
	}

	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
	const FVector TraceImpactPoint = bHit ? Hit.ImpactPoint : TraceEnd;

	if (bDrawDebugTrace)
	{
		DrawDebugLine(GetWorld(), TraceStart, TraceImpactPoint, bHit ? FColor::Red : FColor::Green, false, DebugTraceTime, 0, 1.5f);
		DrawDebugSphere(GetWorld(), TraceImpactPoint, 8.0f, 12, bHit ? FColor::Red : FColor::Green, false, DebugTraceTime);
	}

	if (bHit)
	{
		AActor* HitActor = Hit.GetActor();
		UE_LOG(LogTPSWeapon, Log, TEXT("Hit Actor: %s, Impact: %s"), *GetNameSafe(HitActor), *Hit.ImpactPoint.ToString());

		if (GEngine)
		{
			const FString HitMessage = FString::Printf(TEXT("Hit: %s | Impact: %s"), *GetNameSafe(HitActor), *Hit.ImpactPoint.ToCompactString());
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Yellow, HitMessage);
		}

		UGameplayStatics::ApplyDamage(HitActor, Damage, InstigatorController, this, nullptr);
	}

	return true;
}

bool AWeaponBase::Reload()
{
	if (IsMagazineFull() || !HasReserveAmmo())
	{
		return false;
	}

	const int32 NeededAmmo = MaxAmmo - CurrentAmmo;
	const int32 AmmoToLoad = FMath::Min(NeededAmmo, ReserveAmmo);

	CurrentAmmo += AmmoToLoad;
	ReserveAmmo -= AmmoToLoad;

	return AmmoToLoad > 0;
}

float AWeaponBase::GetFireInterval() const
{
	return FireRate > 0.0f ? 1.0f / FireRate : 0.1f;
}
