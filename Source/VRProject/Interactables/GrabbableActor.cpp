// Fill out your copyright notice in the Description page of Project Settings.

#include "Interactables/GrabbableActor.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/ShapeComponent.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "Components/ChildActorComponent.h"
#include "Player/VRPlayer.h"
#include "Player/VRHand.h"
#include "Player/VRPhysicsHandleComponent.h"
#include "Player/VRFunctionLibrary.h"
#include "Player/EffectsContainer.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "GameFramework/PlayerController.h"
#include "ConstructorHelpers.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogGrabbable);

AGrabbableActor::AGrabbableActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// The grabbable mesh root component. Default Setup.
	grabbableMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GrabbableMesh"));
	grabbableMesh->SetCollisionProfileName("Interactable");
	grabbableMesh->SetSimulatePhysics(true);
	grabbableMesh->SetGenerateOverlapEvents(true);
	grabbableMesh->SetNotifyRigidBodyCollision(true);
	grabbableMesh->ComponentTags.Add(FName("Grabbable"));
	RootComponent = grabbableMesh;

	// Setup audio component for playing effects when hitting other objects.
	grabbableAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("GrabbableAudio"));
	grabbableAudio->SetupAttachment(grabbableMesh);
	grabbableAudio->bAutoActivate = false;

	// Initialise default variables.
	grabInfo = FGrabInformation();
	otherGrabInfo = FGrabInformation();
	grabbedPhysicsMaterial = false;
	snapToHand = false;
	changeMassOnGrab = false;
	considerMassWhenThrown = false;
	massWhenGrabbed = 0.5f;
	hapticIntensityMultiplier = 1.0f;
	collisionFeedbackOverride = nullptr;
	impactSoundOverride = nullptr;
	snapToHandRotationOffset = FRotator::ZeroRotator;
	snapToHandLocationOffset = FVector::ZeroVector;

	// Initialise default interface settings.
	interactableSettings.releaseDistance = 30.0f;
	interactableSettings.rumbleDistance = 10.0f;
}

void AGrabbableActor::BeginPlay()
{
	Super::BeginPlay();

	// Setup sounds for impacts.
	if (impactSoundOverride)
	{
		impactSound = impactSoundOverride;
		grabbableAudio->SetSound(impactSoundOverride);
	}
	else if (AVRPlayer* pawn = Cast<AVRPlayer>(GetWorld()->GetFirstPlayerController()->GetPawn()))
	{
		if (USoundBase* soundToUse = pawn->GetPawnEffects()->GetAudio("DefaultCollision"))
		{
			impactSound = soundToUse;
			grabbableAudio->SetSound(soundToUse);
		}
	}
	else UE_LOG(LogGrabbable, Log, TEXT("The grabbable actor %s, cannot find impact audio from override or the pawns effects container."), *GetName());

	// Get haptic effect to play on collisions.
	if (collisionFeedbackOverride) collisionFeedback = collisionFeedbackOverride;
	else if (AVRPlayer* pawn = Cast<AVRPlayer>(GetWorld()->GetFirstPlayerController()->GetPawn()))
	{
		if (UHapticFeedbackEffect_Base* feedbackEffect = pawn->GetPawnEffects()->GetFeedback("DefaultCollision")) collisionFeedback = feedbackEffect;
	}
	else UE_LOG(LogGrabbable, Log, TEXT("The grabbable actor %s, cannot find haptic effect from override or the pawns effects container."), *GetName());

	// Setup the on hit delegate to call haptic feed back on the hand. Only if there is an impact sound or haptic feedback effect set for this grabbable.
	if (collisionFeedback || impactSound)
	{
		grabbableMesh->SetNotifyRigidBodyCollision(true);
		if (!OnActorHit.IsBound()) OnActorHit.AddDynamic(this, &AGrabbableActor::OnHit);
	}

	// Ensure stabilization and velocity count is correct for all grabbables.
	grabbableMesh->BodyInstance.PositionSolverIterationCount = 15.0f;
	grabbableMesh->BodyInstance.VelocitySolverIterationCount = 5.0f;
}

void AGrabbableActor::OnHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit)
{
	if (OtherActor)
	{
		// Prevents any hitting components that are either balanced on the grabbable or the grabbable balanced on the hand from calling impact sounds and haptic effects.
		if (UPrimitiveComponent* hittingComp = Cast<UPrimitiveComponent>(Hit.Component))
		{
			if (FMath::IsNearlyEqual(hittingComp->GetPhysicsLinearVelocity().Size(), grabbableMesh->GetPhysicsLinearVelocity().Size(), 15.0f)) return;
		}

		// Check if the hit actor is a hand, therefor rumble the hand.
		if (AVRHand* hand = Cast<AVRHand>(OtherActor))
		{
			// Calculate intensity and play bot haptic and the sound if they are currently set.
			float rumbleIntesity = FMath::Clamp(hand->handVelocity.Size() / 250.0f, 0.0f, 1.0f);
			if (collisionFeedback) hand->PlayFeedback(collisionFeedback, rumbleIntesity * hapticIntensityMultiplier);
			//if (impactSound) hand->PlaySound(impactSound, rumbleIntesity);

			// If a hand is holding this class goto the area in this function that rumbles the holding hand.
			if (grabInfo.handRef) goto RumbleHoldingHand;
		}
		// Play impact sound at location of grabbable if it is hit by something other than an actor.
		else
		{
			RumbleHoldingHand:
			float impulseSize = FMath::Abs(NormalImpulse.Size());
			float currentZ = grabbableMesh->GetComponentLocation().Z;

			// If the impulse is big enough and the grabbable is not rolling/sliding along the floor.
 			if (!FMath::IsNearlyEqual(currentZ, lastZ, 0.1f) && grabbableMesh->GetPhysicsLinearVelocity().Size() >= 50.0f)
 			{	
				// Get volume from current intensity and check if the sound should be played.
				float rumbleIntesity = FMath::Clamp(impulseSize / (1200.0f * grabbableMesh->GetMass()), 0.1f, 1.0f);

				// If the audio has a valid sound to play. Make sure the sound was not played within the past 0.3 seconds.
				if (rumbleIntesity > lastRumbleIntensity && lastImpactSoundTime <= GetWorld()->GetTimeSeconds() + 0.3f)
				{
					// If sound is intialised play it.
					if (grabbableAudio->Sound)
					{
						// Don't play again for set amount of time.
						lastImpactSoundTime = GetWorld()->GetTimeSeconds();
						lastRumbleIntensity = rumbleIntesity;

						// Adjust the audio and Play sound effect.
						grabbableAudio->SetVolumeMultiplier(rumbleIntesity);
						grabbableAudio->Play();

						// Set timer to set lastRumbleIntensity back to 0.0f once the sound has played.
						FTimerDelegate timerDel;
						timerDel.BindUFunction(this, TEXT("ResetLastRumbleIntensity"));
						GetWorld()->GetTimerManager().ClearTimer(lastRumbleHandle);
						GetWorld()->GetTimerManager().SetTimer(lastRumbleHandle, timerDel, grabbableAudio->Sound->GetDuration(), true);
					}
				}
 			}
		}
	}	
}

void AGrabbableActor::ResetLastRumbleIntensity()
{
	lastRumbleIntensity = 0.0f;
}

void AGrabbableActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Save last Z.
	lastZ = grabbableMesh->GetComponentLocation().Z;
}

void AGrabbableActor::PickupPhysicsHandle(FGrabInformation info)
{
	// Play sound and haptic effects.
	float rumbleIntesity = FMath::Clamp(info.handRef->handVelocity.Size() / 250.0f, 0.0f, 1.0f);
	if (collisionFeedback)
	{
		info.handRef->PlayFeedback(collisionFeedback, rumbleIntesity * hapticIntensityMultiplier);
	}

	// Adjust the audio and Play sound effect.
	grabbableAudio->SetVolumeMultiplier(rumbleIntesity);
	grabbableAudio->Play();

	// Create joint between hand and physics object and enable physics to handle any collisions.
	FVector locationToGrab = info.targetComponent->GetComponentLocation();
	FRotator rotationToGrab = info.targetComponent->GetComponentRotation();

	// Grab. Increase stabilization while grabbed to prevent visual errors or snapping...
	info.handRef->grabHandle->CreateJointAndFollowLocationWithRotation(grabbableMesh, info.targetComponent, NAME_None, locationToGrab, rotationToGrab, interactableSettings.physicsData);
	grabbableMesh->SetSimulatePhysics(true);

#if DEVELOPMENT
	if (debug) UE_LOG(LogGrabbable, Log, TEXT("The grabbable actor %s, has been grabbed by PhysicsHandle."), *GetName());
#endif
}

void AGrabbableActor::DropPhysicsHandle(FGrabInformation info)
{
	if (info.handRef)
	{
		// Destroy the joint created by the physics handle.
		info.handRef->grabHandle->DestroyJoint();

#if DEVELOPMENT
		if (debug) UE_LOG(LogGrabbable, Log, TEXT("The grabbable actor %s, has been dropped by PhysicsHandle."), *GetName());
#endif
	}
}

bool AGrabbableActor::IsActorGrabbed()
{
	return grabInfo.handRef != nullptr;
}

bool AGrabbableActor::IsActorGrabbedWithTwoHands()
{
	return otherGrabInfo.handRef != nullptr;
}

void AGrabbableActor::Grabbed_Implementation(AVRHand* hand)
{
	// Call delegate for being grabbed.
	OnMeshGrabbed.Broadcast(hand);

	// Check the grab has not been canceled through the delegate being called.
	if (!cancelGrab)
	{
		// If already grabbed grab with otherHand if in two handed mode also.
		if (interactableSettings.twoHandedGrabbing && IsActorGrabbed())
		{
			// Save a reference to the hand.
			otherGrabInfo.handRef = hand;
			otherGrabInfo.targetComponent = (UPrimitiveComponent*)hand->handRoot;

			// Grab the component with two hands.
			PickupPhysicsHandle(grabInfo);
			PickupPhysicsHandle(otherGrabInfo);
		}
		else
		{
			// Save a reference to the hand.
			grabInfo.handRef = hand;
			grabInfo.targetComponent = (UPrimitiveComponent*)hand->handRoot;

			// Dettatch this component from any component or actor before grabbing.
			if (grabbableMesh->GetAttachParent())
			{
				grabbableMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			}

			// Grab via physics handle.
			PickupPhysicsHandle(grabInfo);

			// Apply mass change if enabled.
			if (changeMassOnGrab) grabbableMesh->SetMassOverrideInKg(NAME_None, massWhenGrabbed, true);

			// Apply and save last physics material.
			if (grabbedPhysicsMaterial && physicsMaterialWhileGrabbed)
			{
				grabbableMesh->GetBodyInstance()->SetPhysMaterialOverride(physicsMaterialWhileGrabbed);
			}
#if DEVELOPMENT
			else if (debug) UE_LOG(LogGrabbable, Warning, TEXT("Cannot update physics material on grab as the physicsMaterialWhileGrabbed is null in the grabbable actor %s."), *GetName());
#endif

			// Save the current original pickup transforms depending on the type of grab.
			if (snapToHand)
			{
				// SETUP...
			}
		}

		// Add the hand to the array of ignored actors for collision checks.
		ignoredActors.Add(hand);
	}
	else cancelGrab = false;
}

void AGrabbableActor::Released_Implementation(AVRHand* hand)
{
	// If two handed grabbing is enabled and the other hand has grabbed this component.
	AVRHand* newHandRef = nullptr;
	if (interactableSettings.twoHandedGrabbing && otherGrabInfo.handRef)
	{
		// if its the first hand to grab releasing the component while a second hand is grabbing this grabbable.
		if (grabInfo.handRef == hand)
		{
			newHandRef = otherGrabInfo.handRef;
		}
		// Otherwise if the hand releasing is the second hand to grab this grabbable actor release second hand and return.
		else
		{
			// Release second hand and reset to null.
			DropPhysicsHandle(otherGrabInfo);
			otherGrabInfo.Reset();
			return;
		}	
	}

	// Release component.
	DropPhysicsHandle(grabInfo);

	// Broadcast to delegate.
	OnMeshReleased.Broadcast(hand);

	// Remove mass change if enabled.
	if (changeMassOnGrab)
	{
		grabbableMesh->SetMassOverrideInKg(NAME_None, 0.0f, false);
	}

	// Reset the physics material override to what it was before grabbed.
	if (grabbedPhysicsMaterial && physicsMaterialWhileGrabbed)
	{
		grabbableMesh->GetBodyInstance()->SetPhysMaterialOverride(nullptr);
	}
#if DEVELOPMENT
	else if (debug) UE_LOG(LogGrabbable, Warning, TEXT("Cannot update physics material on grab as the physicsMaterialWhileGrabbed is null in the grabbable actor %s."), *GetName());
#endif

	// Check if we should adjust the velocity based on how heavy the component is.
	if (considerMassWhenThrown)
	{
		float currentMass = grabbableMesh->GetMass();
		if (currentMass > 1.0f)
		{
			// Only take a maximum of 75% velocity from the current.
			float massMultiplier = 1 - FMath::Clamp(FMath::Clamp(currentMass, 0.0f, 20.0f) / 20.0f, 0.0f, 0.6f);
			grabbableMesh->SetPhysicsLinearVelocity(grabbableMesh->GetPhysicsLinearVelocity() * massMultiplier);
			grabbableMesh->SetPhysicsAngularVelocityInRadians(grabbableMesh->GetPhysicsAngularVelocityInRadians() * massMultiplier);
		}
	}

	// Reset values to default. Do last.
	ignoredActors.Remove(hand);
	grabInfo.Reset();
	otherGrabInfo.Reset();

	// If there was a newHand to be grabbing this comp do so.
	if (newHandRef) newHandRef->ForceGrab(this);
}

void AGrabbableActor::Dragging_Implementation(float deltaTime)
{
	if (IsActorGrabbed())
	{
		// Update the distance between the grabbable and the hand to handle releasing if distance is too big.
		FTransform targetTransform = grabInfo.handRef->grabHandle->GetGrabbedTargetTransform();
		FVector currentRelativePickupOffset = targetTransform.GetLocation() - grabbableMesh->GetComponentLocation();
		lastHandGrabDistance = interactableSettings.handDistance;
		interactableSettings.handDistance = currentRelativePickupOffset.Size();

		// Update current grabbables velocity change over time.
		lastFrameVelocity = currentFrameVelocity;
		currentFrameVelocity = grabInfo.handRef->handVelocity.Size();
		currentVelocityChange = FMath::Abs((lastFrameVelocity - currentFrameVelocity) / GetWorld()->GetDeltaSeconds());
	}
}

void AGrabbableActor::Overlapping_Implementation(AVRHand* hand)
{
	IInteractionInterface::Overlapping_Implementation(hand);

	//...
}

void AGrabbableActor::EndOverlapping_Implementation(AVRHand* hand)
{
	IInteractionInterface::EndOverlapping_Implementation(hand);

	//...
}

void AGrabbableActor::Teleported_Implementation()
{
	//...
}

FInterfaceSettings AGrabbableActor::GetInterfaceSettings_Implementation()
{
	return interactableSettings;
}

void AGrabbableActor::SetInterfaceSettings_Implementation(FInterfaceSettings newInterfaceSettings)
{
	interactableSettings = newInterfaceSettings;
}
