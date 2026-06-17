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

	MuzzlePoint = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzlePoint"));
	MuzzlePoint->SetupAttachment(WeaponMesh);
	MuzzlePoint->SetRelativeLocation(MuzzleOffset);
}

void AWeaponBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (MuzzlePoint)
	{
		MuzzlePoint->SetRelativeLocation(MuzzleOffset);
	}
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

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FVector ViewLocation = GetActorLocation();
	FRotator ViewRotation = GetActorRotation();
	if (InstigatorController)
	{
		InstigatorController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WeaponFire), true);
	QueryParams.AddIgnoredActor(this);
	if (OwnerPawn)
	{
		QueryParams.AddIgnoredActor(OwnerPawn);
	}

	const FVector AimTraceEnd = ViewLocation + ViewRotation.Vector() * FireRange;
	FHitResult AimHit;
	const bool bAimHit = World->LineTraceSingleByChannel(
		AimHit,
		ViewLocation,
		AimTraceEnd,
		ECC_Visibility,
		QueryParams);
	const FVector AimPoint = bAimHit ? AimHit.ImpactPoint : AimTraceEnd;

	const FVector MuzzleLocation = MuzzlePoint ? MuzzlePoint->GetComponentLocation() : GetActorLocation();
	const FVector MuzzleToAim = AimPoint - MuzzleLocation;
	const FVector ShotDirection = MuzzleToAim.GetSafeNormal();
	const FVector ShotTraceEnd = MuzzleLocation + ShotDirection * FireRange;

	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(
		Hit,
		MuzzleLocation,
		ShotTraceEnd,
		ECC_Visibility,
		QueryParams);
	const FVector TraceImpactPoint = bHit ? Hit.ImpactPoint : ShotTraceEnd;

	if (bDrawDebugTrace)
	{
		DrawDebugLine(World, MuzzleLocation, TraceImpactPoint, bHit ? FColor::Red : FColor::Green, false, DebugTraceTime, 0, 1.5f);
		DrawDebugSphere(World, MuzzleLocation, 5.0f, 8, FColor::Yellow, false, DebugTraceTime);
		DrawDebugSphere(World, TraceImpactPoint, 8.0f, 12, bHit ? FColor::Red : FColor::Green, false, DebugTraceTime);
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
