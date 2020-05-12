// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/VRPlayer.h"
#include "Player/VRHand.h"
#include "Player/VRMovement.h"
#include "Player/VRFunctionLibrary.h"
#include "Player/EffectsContainer.h"
#include "Camera/CameraComponent.h"
#include "MotionControllerComponent.h"
#include "XRMotionControllerBase.h"
#include "IXRTrackingSystem.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "IHeadMountedDisplay.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Materials/MaterialInstance.h"
#include "ConstructorHelpers.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogVRPlayer);

AVRPlayer::AVRPlayer()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = ETickingGroup::TG_PrePhysics;

	// Setup the movement component used for agent properties for navigation and directional movement.
	floatingMovement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
	floatingMovement->NavAgentProps.AgentRadius = 30.0f;

	// Setup capsule used for floor movement and gravity. By default this is disabled as its setup in the movement component.
	movementCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	movementCapsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	movementCapsule->SetCapsuleHalfHeight(80.0f);
	movementCapsule->SetCapsuleRadius(32.0f);
	RootComponent = movementCapsule;

	// Setup floor location.
	scene = CreateDefaultSubobject<USceneComponent>(TEXT("Scene"));
	scene->SetupAttachment(movementCapsule);
	scene->SetRelativeLocation(FVector(0.0f, 0.0f, -80.0f));

	// Setup HMD with head Collider. Disable any collisions until the HMD is defiantly being tracked to prevent physics actors being effected.
	camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	camera->SetupAttachment(scene);
	headCollider = CreateDefaultSubobject<USphereComponent>(TEXT("HeadCollider"));
	headCollider->SetCollisionProfileName("PhysicsActor");
	headCollider->InitSphereRadius(20.0f);
	headCollider->SetupAttachment(camera);

	// Initialise the physics handles.
	headHandle = CreateDefaultSubobject<UVRPhysicsHandleComponent>("HeadHandle");

	// Setup vignette.
	vignette = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Vignette"));
	vignette->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	vignette->SetupAttachment(camera);
	vignette->SetActive(false);
	vignette->SetVisibility(false);

	// Setup the effects container.
	pawnEffects = CreateDefaultSubobject<UEffectsContainer>(TEXT("PawnEffects"));

	// Add post update ticking function to this actor.
	postTick.bCanEverTick = false;
	postTick.Target = this;
	postTick.TickGroup = TG_PostUpdateWork;

	// Initialise default variables.
	BaseEyeHeight = 0.0f;
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	devModeActive = false;
	tracked = false;
	collisionEnabled = false;
	movementLocked = false;
	debug = false;
}

void AVRPlayer::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Spawn movementComponent class.
	if (movementClass && !movement)
	{
		// Setup movement spawn parameters.
		FActorSpawnParameters movementParam;
		movementParam.Owner = this;
		movementParam.Instigator = this;
		movementParam.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		// Spawn in the movement from the blueprint created template.
		movement = GetWorld()->SpawnActor<AVRMovement>(movementClass, FVector::ZeroVector, FRotator::ZeroRotator, movementParam);
		FAttachmentTransformRules movementAttatchRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, true);
		movement->AttachToComponent(scene, movementAttatchRules);
		movement->SetOwner(this);

#if WITH_EDITOR
		// Enable developer mode if the HMD headset is enabled.
		if (!UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
		{
			movement->currentMovementMode = EVRMovementMode::Developer;
			devModeActive = true;
		}
#endif
	}
}

void AVRPlayer::BeginPlay()
{
	Super::BeginPlay();
	
	// Register the secondary post update tick function in the world on level start.
	postTick.bCanEverTick = true;
	postTick.RegisterTickFunction(GetWorld()->PersistentLevel);

	// Setup the device ID for the current HMD used...
	hmdDevice.SystemName = UHeadMountedDisplayFunctionLibrary::GetHMDDeviceName();
	hmdDevice.DeviceId = 0; // HMD..

	// Spawn the left and the right hand in on begin play as they are in there own class. (Issues were occurring when doing this in the constructor I assume due to it not recompiling the hand code).
	FActorSpawnParameters spawnHandParams;
	FAttachmentTransformRules handAttatchRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, true);
	spawnHandParams.Owner = this;
	spawnHandParams.Instigator = this;
	spawnHandParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	// Spawn the hands using the template classes made in BP.
	leftHand = GetWorld()->SpawnActor<AVRHand>(leftHandClass, FVector::ZeroVector, FRotator::ZeroRotator, spawnHandParams);
	leftHand->AttachToComponent(scene, handAttatchRules);
	leftHand->SetOwner(this);
	rightHand = GetWorld()->SpawnActor<AVRHand>(rightHandClass, FVector::ZeroVector, FRotator::ZeroRotator, spawnHandParams);
	rightHand->AttachToComponent(scene, handAttatchRules);
	rightHand->SetOwner(this);

	// Setup hands and movement and any pointers they need and developer adjustments.
	movement->SetupMovement(this);
	leftHand->SetupHand(rightHand, this, devModeActive);
	rightHand->SetupHand(leftHand, this, devModeActive);

	// Setup physics handling of the head collider.
	headCollider->SetSimulatePhysics(true);
	headHandle->CreateJointAndFollowLocationWithRotation(headCollider, (UPrimitiveComponent*)camera, NAME_None, camera->GetComponentLocation(), camera->GetComponentRotation());

	// Create array for the actors to be ignored from certain collision traces in the hand and pawn classes.
	actorsToIgnore.Add(this);
	actorsToIgnore.Add(leftHand);
	actorsToIgnore.Add(rightHand);

	// Set the tracking origin for the HMD to be the floor. To support PSVR check if its that headset and set tracking origin to eye level and add the default player height. Also add way to rotate.
	UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::Floor);
}

void AVRPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update the hands tick function from this class.
	if (leftHand && leftHand->active) leftHand->Tick(DeltaTime);
	if (rightHand && rightHand->active) rightHand->Tick(DeltaTime);
}

void AVRPlayer::PostUpdateTick(float DeltaTime)
{
	// Only check to update movement if it is not locked by play state or other input.
	if (!movementLocked)
	{
		// If there is a hand moving, update the vr movement component.
		if (movingHand)	movement->UpdateMovement(movingHand);
		// End the movement if its still set in the movement class.
		else if (movement->currentMovingHand) movement->UpdateMovement(movement->currentMovingHand, true);
	}

	// Update the current collision properties based from the tracking of the HMD and then each hand to prevent physics actors being affected by repositioning these components.
	if (!devModeActive) UpdateHardwareTrackingState();
}

void AVRPlayer::Teleported()
{
	headHandle->TeleportGrabbedComp();

	// Teleport hands and objects in the hands.
	if (leftHand) leftHand->TeleportHand();
	if (rightHand) rightHand->TeleportHand();
}

void AVRPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Player pawn action bindings.
	PlayerInputComponent->BindAction("TriggerLeft", IE_Pressed, this, &AVRPlayer::TriggerLeftPressed);
	PlayerInputComponent->BindAction("TriggerLeft", IE_Released, this, &AVRPlayer::TriggerLeftReleased);
	PlayerInputComponent->BindAction("TriggerRight", IE_Pressed, this, &AVRPlayer::TriggerRightPressed);
	PlayerInputComponent->BindAction("TriggerRight", IE_Released, this, &AVRPlayer::TriggerRightReleased);
	PlayerInputComponent->BindAction("ThumbMiddleL", IE_Pressed, this, &AVRPlayer::ThumbLeftPressed);
	PlayerInputComponent->BindAction("ThumbMiddleL", IE_Released, this, &AVRPlayer::ThumbLeftReleased);
	PlayerInputComponent->BindAction("ThumbMiddleR", IE_Pressed, this, &AVRPlayer::ThumbRightPressed);
	PlayerInputComponent->BindAction("ThumbMiddleR", IE_Released, this, &AVRPlayer::ThumbRightReleased);

	// Player pawn axis bindings.
	PlayerInputComponent->BindAxis("TriggerL", this, &AVRPlayer::TriggerLeftAxis);
	PlayerInputComponent->BindAxis("TriggerR", this, &AVRPlayer::TriggerRightAxis);
	PlayerInputComponent->BindAxis("ThumbstickLeft_X", this, &AVRPlayer::ThumbstickLeftX);
	PlayerInputComponent->BindAxis("ThumbstickLeft_Y", this, &AVRPlayer::ThumbstickLeftY);
	PlayerInputComponent->BindAxis("ThumbstickRight_X", this, &AVRPlayer::ThumbstickRightX);
	PlayerInputComponent->BindAxis("ThumbstickRight_Y", this, &AVRPlayer::ThumbstickRightY);
	PlayerInputComponent->BindAxis("SqueezeL", this, &AVRPlayer::SqueezeL);
	PlayerInputComponent->BindAxis("SqueezeR", this, &AVRPlayer::SqueezeR);
}

void AVRPlayer::TriggerLeftPressed()
{
	if (leftHand && leftHand->active)
	{
		leftHand->TriggerPressed();
	}
}

void AVRPlayer::TriggerLeftReleased()
{
	if (leftHand && leftHand->active)
	{
		leftHand->TriggerReleased();
	}
}

void AVRPlayer::TriggerRightPressed()
{
	if (rightHand && rightHand->active)
	{
		rightHand->TriggerPressed();
	}
}

void AVRPlayer::TriggerRightReleased()
{
	if (rightHand && rightHand->active)
	{
		rightHand->TriggerReleased();
	}
}

void AVRPlayer::ThumbLeftPressed()
{
	if (leftHand)
	{
		// If gripping use the interaction functionality.
		if (leftHand->objectInHand) leftHand->Interact(true);
		// Otherwise activate current movement mode.
		else if (movement->canMove && leftHand->active)
		{
			bool moveEnabled = false;
			switch (movement->currentMovementMode)
			{
			case EVRMovementMode::Developer:
			case EVRMovementMode::Teleport:
			case EVRMovementMode::SwingingArms:
			case EVRMovementMode::Lean:
				moveEnabled = true;
				break;
			}

			// Move if enabled.
			if (moveEnabled)
			{
				movingHand = leftHand;
			}
		}
	}
}

void AVRPlayer::ThumbLeftReleased()
{
	if (leftHand)
	{
		// If gripping use the interaction functionality.
		if (leftHand->objectInHand) leftHand->Interact(false);
		// Otherwise activate current movement mode.
		else if (movement->canMove && leftHand->active)
		{
			bool moveEnabled = false;
			switch (movement->currentMovementMode)
			{
			case EVRMovementMode::Developer:
			case EVRMovementMode::Teleport:
			case EVRMovementMode::SwingingArms:
			case EVRMovementMode::Lean:
				moveEnabled = true;
				break;
			}

			// Stop moving if moving is enabled.
			if (moveEnabled)
			{
				if (movingHand && movingHand == leftHand)
				{
					movingHand = nullptr;
				}
			}
		}
	}
}

void AVRPlayer::ThumbRightPressed()
{
	if (rightHand)
	{
		// If the right hand is locked to or grabbing something.
		if (rightHand->objectInHand) rightHand->Interact(true);
		// Otherwise activate current movement mode.
		else if (movement->canMove && rightHand->active)
		{
			bool moveEnabled = false;
			switch (movement->currentMovementMode)
			{
			case EVRMovementMode::Developer:
			case EVRMovementMode::Teleport:
			case EVRMovementMode::SwingingArms:
			case EVRMovementMode::Lean:
				moveEnabled = true;
				break;
			}

			// Move if enabled.
			if (moveEnabled)
			{
				movingHand = rightHand;
			}
		}
	}
}

void AVRPlayer::ThumbRightReleased()
{
	if (rightHand)
	{
		// If the right hand is locked to or grabbing something.
		if (rightHand->objectInHand) rightHand->Interact(false);
		// Otherwise activate current movement mode.
		else if (movement->canMove && rightHand->active)
		{
			bool moveEnabled = false;
			switch (movement->currentMovementMode)
			{
			case EVRMovementMode::Developer:
			case EVRMovementMode::Teleport:
			case EVRMovementMode::SwingingArms:
			case EVRMovementMode::Lean:
				moveEnabled = true;
				break;
			}

			// Stop moving if moving is enabled.
			if (moveEnabled)
			{
				if (movingHand && movingHand == rightHand)
				{
					movingHand = nullptr;
				}
			}
		}
	}
}

void AVRPlayer::SqueezeL(float val)
{
	if (leftHand && leftHand->active) leftHand->Squeeze(val);
}

void AVRPlayer::SqueezeR(float val)
{
	if (rightHand && rightHand->active) rightHand->Squeeze(val);
}

void AVRPlayer::ThumbstickLeftX(float val)
{
	if (leftHand && leftHand->active)
	{
		// Update the value.
		leftHand->thumbstick.X = val;
		if (val == 0.0f) return;

		// Depending on the current movement mode allow thumbstick to move player.
		bool moveEnabled = false;
		switch (movement->currentMovementMode)
		{
		case EVRMovementMode::Joystick:
		case EVRMovementMode::SpeedRamp:
		case EVRMovementMode::Teleport:
			moveEnabled = true;
			break;
		}

		// Setup movement if needed.
		if (moveEnabled)
		{
			if (movement && movement->canMove && !leftHand->objectInHand)
			{
				if (FMath::Abs(val) >= movement->teleportDeadzone)
				{
					if (!movingHand) movingHand = leftHand;
				}
				else if (movingHand == leftHand && FMath::Abs(leftHand->thumbstick.Y) < movement->teleportDeadzone)
				{
					movingHand = nullptr;
				}
			}
		}
	}
}

void AVRPlayer::ThumbstickLeftY(float val)
{
	if (leftHand && leftHand->active)
	{
		// Update the value.
		leftHand->thumbstick.Y = val;
		if (val == 0.0f) return;

		// Depending on the current movement mode allow thumbstick to move player.
		bool moveEnabled = false;
		switch (movement->currentMovementMode)
		{
		case EVRMovementMode::Joystick:
		case EVRMovementMode::SpeedRamp:
		case EVRMovementMode::Teleport:
			moveEnabled = true;
			break;
		}

		// Setup movement if needed.
		if (moveEnabled)
		{
			if (movement && movement->canMove && !leftHand->objectInHand)
			{
				if (FMath::Abs(val) >= movement->teleportDeadzone)
				{
					if (!movingHand) movingHand = leftHand;
				}
				else if (movingHand == leftHand && FMath::Abs(leftHand->thumbstick.X) < movement->teleportDeadzone)
				{
					movingHand = nullptr;
				}
			}
		}	
	}
}

void AVRPlayer::ThumbstickRightX(float val)
{
	if (rightHand && rightHand->active)
	{
		// Update the value.
		rightHand->thumbstick.X = val;
		if (val == 0.0f) return;

		// Depending on the current movement mode allow thumbstick to move player.
		bool moveEnabled = false;
		switch (movement->currentMovementMode)
		{
		case EVRMovementMode::Joystick:
		case EVRMovementMode::SpeedRamp:
		case EVRMovementMode::Teleport:
			moveEnabled = true;
			break;
		}

		// Setup movement if needed.
		if (moveEnabled)
		{
			if (movement && movement->canMove && !rightHand->objectInHand)
			{
				if (FMath::Abs(val) >= movement->teleportDeadzone)
				{
					if (!movingHand) movingHand = rightHand;
				}
				else if (movingHand == rightHand && FMath::Abs(rightHand->thumbstick.Y) < movement->teleportDeadzone)
				{
					movingHand = nullptr;
				}
			}
		}	
	}	
}

void AVRPlayer::ThumbstickRightY(float val)
{
	if (rightHand && rightHand->active)
	{
		// Update the value.
		rightHand->thumbstick.Y = val;
		if (val == 0.0f) return;

		// Depending on the current movement mode allow thumbstick to move player.
		bool moveEnabled = false;
		switch (movement->currentMovementMode)
		{
		case EVRMovementMode::Joystick:
		case EVRMovementMode::SpeedRamp:
		case EVRMovementMode::Teleport:
			moveEnabled = true;
			break;
		}

		// Setup movement if needed.
		if (moveEnabled)
		{
			if (movement && movement->canMove && !rightHand->objectInHand)
			{
				if (FMath::Abs(val) >= movement->teleportDeadzone)
				{
					if (!movingHand) movingHand = rightHand;
				}
				else if (movingHand == rightHand && FMath::Abs(rightHand->thumbstick.X) < movement->teleportDeadzone)
				{
					movingHand = nullptr;
				}
			}
		}	
	}
}

void AVRPlayer::TriggerLeftAxis(float val)
{
	if (!devModeActive) leftHand->trigger = val;
}

void AVRPlayer::TriggerRightAxis(float val)
{
	if (!devModeActive) rightHand->trigger = val;
}

void AVRPlayer::UpdateHardwareTrackingState()
{
	// Only allow the collision to be enabled on the player while the headset is being tracked.
	bool trackingHMD = UHeadMountedDisplayFunctionLibrary::IsDeviceTracking(hmdDevice);
	if (trackingHMD)
	{
		if (!foundHMD)
		{
			// Activate head collision while tracked.
			ActivateCollision(true);
			foundHMD = true;

			// On first tracked event, move the player to the scenes location so they are centered on the player start component.
			if (!tracked)
			{
				MovePlayerWithRotation(scene->GetComponentLocation(), scene->GetComponentRotation());
				tracked = true;
			}

#if WITH_EDITOR
			if (debug) UE_LOG(LogVRPlayer, Warning, TEXT("Found and tracking the HMD owned by %s"), *GetName());
#endif
		}
	}
	else if (foundHMD)
	{
		ActivateCollision(false);
		foundHMD = false;

#if WITH_EDITOR
		if (debug) UE_LOG(LogVRPlayer, Warning, TEXT("Lost the HMD tracking owned by %s"), *GetName());
#endif
	}

	// Update the tracked state of the controllers also.
	leftHand->UpdateControllerTrackedState();
	rightHand->UpdateControllerTrackedState();
}

void AVRPlayer::MovePlayerWithRotation(FVector newLocation, FRotator newFacingRotation)
{
	// Make sure that the newFacingRotation is applied before moving the player...
	float newCameraRotation = newFacingRotation.Yaw - (camera->GetRelativeRotation().Yaw - 180.0f);
	movementCapsule->SetWorldRotation(FRotator(0.0f, newCameraRotation - 180.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);

	// Move the player to the new location.
	MovePlayer(newLocation);
}

void AVRPlayer::MovePlayer(FVector newLocation)
{
	// Get offset difference and new capsule location and move the capsule to the specified newLocation.
	FVector newCapsuleLocation = FVector(newLocation.X, newLocation.Y, newLocation.Z + movementCapsule->GetUnscaledCapsuleHalfHeight());
	movementCapsule->SetWorldLocation(newCapsuleLocation, false, nullptr, ETeleportType::TeleportPhysics);

	// Get the relative offset of the current camera/player location to the capsule.
	FVector camaraToCapsuleOffset = movementCapsule->GetComponentTransform().InverseTransformPosition(camera->GetComponentLocation());
	camaraToCapsuleOffset.Z = 0.0f;

	// Offset the VR Scene location by the relative offset of the camera to the capsule to place the player within the movement capsule at the newLocation.	
	FVector newRoomLocation = scene->GetComponentTransform().TransformPosition(-camaraToCapsuleOffset);
	scene->SetWorldLocation(newRoomLocation);
	Teleported();
}

bool AVRPlayer::GetCollisionEnabled()
{
	return collisionEnabled;
}

void AVRPlayer::ActivateAllCollision(bool enable)
{
	if (leftHand && rightHand)
	{
		if (foundHMD || devModeActive) ActivateCollision(enable);
		if (leftHand->foundController || devModeActive) leftHand->ActivateCollision(enable);
		if (rightHand->foundController || devModeActive) rightHand->ActivateCollision(enable);
	}
	else UE_LOG(LogVRPlayer, Error, TEXT("One of the hand classes in the VRPawn %s is null. Cannot activate/de-activate collision."));
}

void AVRPlayer::ActivateCollision(bool enable)
{
	if (enable)
	{
		FTimerDelegate timerDel;
		timerDel.BindUFunction(this, FName("CollisionDelay"));
		GetWorld()->GetTimerManager().SetTimer(headColDelay, timerDel, 0.01f, true);
		collisionEnabled = true;
	}
	else
	{
		// Disable the head Collider collision along with both hands.
		headCollider->SetCollisionProfileName("PhysicsActorOverlap");
		collisionEnabled = false;
	}
}

void AVRPlayer::CollisionDelay()
{
	// Check if the given collisionComponent is currently overlapping a blocking physics collision.
	TArray<UPrimitiveComponent*> overlappingComps;
	TArray<TEnumAsByte<EObjectTypeQuery>> physicsColliders;
	physicsColliders.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));
	bool overlapping = UKismetSystemLibrary::ComponentOverlapComponents(headCollider, headCollider->GetComponentTransform(), physicsColliders, nullptr, actorsToIgnore, overlappingComps);
	
	// If no longer overlapping re-enable the collision on the head Collider to query and physics.
	if (!overlapping)
	{
		// Stop this function once the collision is re-enabled.
		headCollider->SetCollisionProfileName("PhysicsActor");
		GetWorld()->GetTimerManager().ClearTimer(headColDelay);
	}
}

UEffectsContainer* AVRPlayer::GetPawnEffects()
{
	return pawnEffects;
}

void FPostUpdateTick::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	// Call the AVRPlayer's second tick function.
	if (Target)
	{
		Target->PostUpdateTick(DeltaTime);
	}
}
