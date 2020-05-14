// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/VRHand.h"
#include "Player/VRPlayer.h"
#include "Player/HandsAnimInstance.h"
#include "Player/VRPhysicsHandleComponent.h"
#include "Player/VRFunctionLibrary.h"
#include "Player/EffectsContainer.h"
#include "XRMotionControllerBase.h"
#include "Haptics/HapticFeedbackEffect_Base.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Interactables/GrabbableActor.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Sound/SoundBase.h"
#include "WidgetInteractionComponent.h"
#include "WidgetComponent.h"

DEFINE_LOG_CATEGORY(LogHand);

AVRHand::AVRHand()
{
	// Tick for this function is ran in the pawn class.
	PrimaryActorTick.bCanEverTick = false; 

	// Setup motion controller. Default setup.
	controller = CreateDefaultSubobject<UMotionControllerComponent>("Controller");
	controller->MotionSource = FXRMotionControllerBase::LeftHandSourceId;
	controller->SetupAttachment(scene);
	controller->bDisableLowLatencyUpdate = true;
	RootComponent = controller;

	// handRoot comp.
	handRoot = CreateDefaultSubobject<USceneComponent>(TEXT("HandRoot"));
	handRoot->SetupAttachment(controller);

	// Setup hand physics. Default setup.
	handPhysics = CreateDefaultSubobject<UBoxComponent>("handBox");
	handPhysics->SetCollisionProfileName("PhysicsActor");
	handPhysics->SetGenerateOverlapEvents(true);
	handPhysics->SetNotifyRigidBodyCollision(true);
	handPhysics->SetupAttachment(handRoot);
	handPhysics->SetRelativeTransform(FTransform(FRotator(-20.0f, 0.0f, 0.0f), FVector(-7.5f, -0.4f, -1.3f), FVector(1.0f, 1.0f, 1.0f)));
	handPhysics->SetBoxExtent(FVector(9.5f, 2.9f, 5.6f));

	// Skeletal mesh component for the hand model. Default setup.
	handSkel = CreateDefaultSubobject<USkeletalMeshComponent>("handSkel");
	handSkel->SetCollisionProfileName("Hand");
	handSkel->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	handSkel->SetupAttachment(handPhysics);
	handSkel->SetRenderCustomDepth(true);
	handSkel->SetGenerateOverlapEvents(true);
	handSkel->SetCustomDepthStencilValue(1);
	handSkel->SetRelativeTransform(FTransform(FRotator(-1.0f, 0.0f, 0.0f), FVector(-10.4f, 0.45f, -0.8f), FVector(0.27f, 0.27f, 0.27f)));

	// Collider to find interactables. Default setup.
	grabCollider = CreateDefaultSubobject<UBoxComponent>("GrabCollider");
	grabCollider->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	grabCollider->SetCollisionProfileName(FName("HandOverlap"));
	grabCollider->SetupAttachment(handPhysics);
	grabCollider->SetRelativeLocation(FVector(1.6f, -2.5f, 0.0f));
	grabCollider->SetBoxExtent(FVector(8.0f, 2.3f, 5.0f));

	// Initialise the physics handles.
	handHandle = CreateDefaultSubobject<UVRPhysicsHandleComponent>("PhysicsHandle");
	grabHandle = CreateDefaultSubobject<UVRPhysicsHandleComponent>("GrabHandle");
	handHandle->reposition = true;

	// Setup widget interaction components.
	widgetOverlap = CreateDefaultSubobject<USphereComponent>("WidgetOverlap");
	widgetOverlap->SetMobility(EComponentMobility::Movable);
	widgetOverlap->SetupAttachment(handSkel);
	widgetOverlap->SetSphereRadius(3.0f);
	widgetOverlap->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	widgetOverlap->SetCollisionObjectType(ECC_Hand);
	widgetOverlap->SetCollisionResponseToAllChannels(ECR_Ignore);
	widgetOverlap->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	widgetInteractor = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("WidgetInteractor"));
	widgetInteractor->SetupAttachment(widgetOverlap);
	widgetInteractor->InteractionDistance = 30.0f;
	widgetInteractor->InteractionSource = EWidgetInteractionSource::World;
	widgetInteractor->bEnableHitTesting = true;

	// Ensure fast widget path is disabled as it optimizes away the functionality we need when building in VR.
	GSlateFastWidgetPath = 0;

	// Setup movement direction component.
	movementTarget = CreateDefaultSubobject<USceneComponent>("MovementTarget");
	movementTarget->SetMobility(EComponentMobility::Movable);
	movementTarget->SetupAttachment(handSkel);
	movementTarget->SetRelativeLocation(FVector(30.0f, -20.0f, 0.0f));

	// Initialise default variables.
	handEnum = EControllerHand::Left;
	controllerType = EVRController::Index;
	objectToGrab = nullptr;
	objectInHand = nullptr;
	grabbing = false;
	foundController = false;
	hideOnGrab = true;	
	active = true;
	collisionEnabled = false;
	thumbstick = FVector2D(0.0f, 0.0f);
	distanceFrameCount = 0;
	debug = false;
	telekineticGrab = true;
	devModeEnabled = false;
	devModeCurlAlpha = 0.0f;
	openedSinceGrabbed = true;
}

void AVRHand::BeginPlay()
{
	Super::BeginPlay();

	// Create a joint between the controller and hand.
	handPhysics->SetSimulatePhysics(true);
	handHandle->CreateJointAndFollowLocationWithRotation(handPhysics, (UPrimitiveComponent*)handRoot, NAME_None, handRoot->GetComponentLocation(), handRoot->GetComponentRotation());

	// Setup widget interaction attachments.
	widgetOverlap->AttachToComponent(handSkel, FAttachmentTransformRules::SnapToTargetNotIncludingScale, "FingerSocket");

	// Setup delegate for overlapping widget component.
	if (!widgetOverlap->OnComponentBeginOverlap.Contains(this, "WidgetInteractorOverlapBegin"))
	{
		widgetOverlap->OnComponentBeginOverlap.AddDynamic(this, &AVRHand::WidgetInteractorOverlapBegin);
	}
}

void AVRHand::SetupHand(AVRHand* oppositeHand, AVRPlayer* playerRef, bool dev)
{
	// Initialise class variables.
	player = playerRef;
	otherHand = oppositeHand;
	owningController = player->GetWorld()->GetFirstPlayerController();

	// Use dev mode to disable areas of code when in developer mode.
#if WITH_EDITOR
	devModeEnabled = dev;
#endif

	// Save the original transform of the hand for calculating offsets.
	originalHandTransform = controller->GetComponentTransform();

	// Save original information about positions and scales of hand components.
	originalSkelOffset = handSkel->GetRelativeLocation();

	// Set up the controller offsets for the current type of controller selected.
	if (!devModeEnabled) SetupControllerOffset();
}

void AVRHand::SetControllerType(EVRController type)
{
	controllerType = type;
	SetupControllerOffset();
}

void AVRHand::SetupControllerOffset()
{
	// Reset the hand transform. (Default for VIVE controller)
	handRoot->SetRelativeTransform(FTransform(FRotator(0.0f), FVector(0.0f), FVector(1.0f)));

	// Depending on the selected option change the offset of the controller...
	switch (controllerType)
	{
	case EVRController::Index:
		handRoot->AddLocalOffset(FVector(-2.4f, 0.0f, -5.3f));
		handRoot->AddLocalRotation(FRotator(-30.0f, 0.0f, 0.0f));
	break;
	case EVRController::Oculus:
		handRoot->AddLocalOffset(FVector(7.5f, 0.0f, 0.0f));
	break;
	}
}

void AVRHand::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Calculate controller velocity as its not simulating physics.
	lastHandPosition = currentHandPosition;
	currentHandPosition = controller->GetComponentLocation();
	handVelocity = (currentHandPosition - lastHandPosition) / DeltaTime;
	
	// Calculate angular velocity for the controller for when its not simulating physics.
	lastHandRotation = currentHandRotation;
	currentHandRotation = controller->GetComponentQuat();
	FQuat deltaRot = lastHandRotation.Inverse() * currentHandRotation;
	FVector axis;
	float angle;
	deltaRot.ToAxisAndAngle(axis, angle);
	angle = FMath::RadiansToDegrees(angle);
	handAngularVelocity = currentHandRotation.RotateVector((axis * angle) / DeltaTime);

	// Update finger tracking and physics collider size based off finger tracking.
	UpdateFingerTracking();

	// If an object is grabbed by the hand update the relevant functions.
	if (objectInHand)
	{
		// Execute dragging for the grabbed object.
		IInteractionInterface::Execute_Dragging(objectInHand, DeltaTime);

		// Update interactable distance for releasing over max distance.
		CheckInteractablesDistance();
	}
	else
	{
		// If nothing is grabbed yet look for interactables.
		CheckForOverlappingActors();

		// Update telekinetic grabbing if enabled.
		if (telekineticGrab)
		{
			if (trigger >= 1.0f && fingersClosedAlpha >= 0.6f)
			{
				UpdateTelekineticGrab();
			}
			else if (lerpingGrabbable)
			{
				lerpingGrabbable->grabbableMesh->SetCollisionProfileName("Interactable");
				lerpingGrabbable->grabbableMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
				lerpingGrabbable = nullptr;
			}	
		}
	}
}

void AVRHand::WidgetInteractorOverlapBegin(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// If the other component was a widget press it.
	if (UWidgetComponent* widgetOverlap = Cast<UWidgetComponent>(OtherComp))
	{
		// Rotate the widget interactor to face what we have overlapped with and press then release the pointer key.
		FVector worldDirection = widgetInteractor->GetComponentLocation() - SweepResult.Location;
		widgetInteractor->SetWorldRotation(worldDirection.Rotation());
		widgetInteractor->PressPointerKey(EKeys::LeftMouseButton);
		widgetInteractor->ReleasePointerKey(EKeys::LeftMouseButton);

		// Rumble the controller to give feedback that the button was successfully pressed.
		PlayFeedback();
	}
}

void AVRHand::TriggerPressed()
{
	// Implement dev mode grabbing...
#if WITH_EDITOR
	if (devModeEnabled)
	{
		devModeCurlAlpha = 1.0f;
	}
#endif
}

void AVRHand::TriggerReleased()
{
	// Implement dev mode releasing...
#if WITH_EDITOR
	if (devModeEnabled)
	{
		devModeCurlAlpha = 0.0f;
	}
#endif
}

void AVRHand::Grab()
{
	// The player is current grabbing.
	grabbing = true;

#if WITH_EDITOR
	// If in dev-mode ensure the trigger is 1.0f when grabbed.
	if (devModeEnabled) trigger = 1.0f;
#endif

	// If the object to grab is new grab it.
	if (objectToGrab && !objectInHand)
	{
		// Release the actor from the other hand if it has the objectToGrab grabbed and the grabbed object does NOT support two handed grabbing.
		if (otherHand && objectToGrab == otherHand->objectInHand)
		{
			FInterfaceSettings otherGrabbedObjectSettings = IInteractionInterface::Execute_GetInterfaceSettings(otherHand->objectInHand);
			if (!otherGrabbedObjectSettings.twoHandedGrabbing) otherHand->ReleaseGrabbedActor();
		}

		// Disable collision while grabbed...
		// Also hide hand if the option is enabled.
		ActivateCollision(false);
		if (hideOnGrab) handSkel->SetVisibility(false);
		
		// Update grabbed variables.
		objectInHand = objectToGrab;
		IInteractionInterface::Execute_Grabbed(objectInHand, this);
		IInteractionInterface::Execute_EndOverlapping(objectInHand, this);

		// Feedback to indicate the object has been grabbed.
		PlayFeedback(); 
	}
}

void AVRHand::ForceGrab(UObject* objectToForceGrab)
{
	// Force remove current object in the hand.
	objectInHand = nullptr;

	// Grab the object.
	if (grabbing)
	{	
		objectToGrab = objectToForceGrab;
		Grab();
	}
}

void AVRHand::Drop()
{
#if WITH_EDITOR
	// If in dev-mode ensure trigger value is 0.0f when dropped.
	if (devModeEnabled) trigger = 0.0f;
#endif

	// If there is something in the hand.
	if (objectInHand)
	{
		// Release the object.
		ReleaseGrabbedActor();
	}

	// End grabbing.
	grabbing = false;
}

void AVRHand::Interact(bool pressed)
{
	if (objectInHand)
	{
		IInteractionInterface::Execute_Interact(objectInHand, pressed);
	}
}

void AVRHand::ReleaseGrabbedActor()
{
	// If an object is in the hand.
 	if (objectInHand)
	{	
		// Execute release interactable.
		IInteractionInterface::Execute_Released(objectInHand, this);

		// Nullify grabbed objects variables.
		objectInHand = nullptr;
		objectToGrab = nullptr;

		// Show the hands if they are hidden and start checking the hands collision in 0.4f seconds to be re-enabled.
		if (hideOnGrab) handSkel->SetVisibility(true);	
		ActivateCollision(true, 0.6f);
	}
}

void AVRHand::Squeeze(float howHard)
{
	// Execute the interactables grip pressed and released functions.
 	if (objectInHand)
 	{
 		IInteractionInterface::Execute_Squeezing(objectInHand, this, howHard);
 	}
}

void AVRHand::TeleportHand()
{
	// Move hand to teleported location, also disable collision until new overlaps are ended.
	handHandle->TeleportGrabbedComp();
	if (objectInHand) grabHandle->TeleportGrabbedComp();
	ResetCollision();

	// Used on components that need re-positioning after a teleportation.
	if (objectInHand) IInteractionInterface::Execute_Teleported(objectInHand);
}

void AVRHand::UpdateControllerTrackedState()
{
	// If dev mode is enabled.
#if WITH_EDITOR
	if (devModeEnabled)
	{
		foundController = true;
		return;
	}
#endif

	// Only allow the collision on this hand to be enabled if the controller is being tracked.
	bool trackingController = controller->IsTracked();
	if (trackingController)
	{
		if (!foundController)
		{
			ActivateCollision(true);
			foundController = true;
#if WITH_EDITOR
			if (debug) UE_LOG(LogHand, Warning, TEXT("Found and tracking the controller owned by %s"), *GetName());
#endif
		}
	}
	else if (foundController)
	{
		ActivateCollision(false);
		foundController = false;
#if WITH_EDITOR
		if (debug) UE_LOG(LogHand, Warning, TEXT("Lost the controller tracking owned by %s"), *GetName());
#endif
	}
}

void AVRHand::CheckForOverlappingActors()
{
	// Get grabColliders current overlapping components.
	TArray<UPrimitiveComponent*> overlapping;
	grabCollider->GetOverlappingComponents(overlapping);
	UObject* toGrab = nullptr;
	float smallestDistance = 100000.0f;

	// If components are found.
	if (overlapping.Num() > 0)
	{
		// Loop through each overlapping component and find the closest one with an interface.
		for (UPrimitiveComponent* comp : overlapping)
		{
			// If a object with an interface has been found.
			UObject* objectWithInterface = LookForInterface(comp);
			if (objectWithInterface)
			{
				// Make sure this interface is currently allowing interaction.
				FInterfaceSettings objectsInterfaceSettings = IInteractionInterface::Execute_GetInterfaceSettings(objectWithInterface);
				if (!objectsInterfaceSettings.active)
				{
					// End overlapping before exiting this function.
					if (objectToGrab)
					{
						IInteractionInterface::Execute_EndOverlapping(objectToGrab, this);
						objectToGrab = nullptr;
					}
					// Go to next item in the overlapping array.
					continue;
				}

				// Update the closest component and smallest distance value to compare to the next component in the array.
				float currentDistance = (comp->GetComponentLocation() - grabCollider->GetComponentLocation()).Size();
				if (currentDistance < smallestDistance)
				{
					smallestDistance = currentDistance;
					toGrab = objectWithInterface;
				}
			}
		}
	}

	// If the object to grab has changed from null or the last object continue to check whats been changed.
	if (objectToGrab != toGrab)
	{
		// If there was an object To Grab end overlapping. (Un-Highlight)
		if (objectToGrab)
		{
			IInteractionInterface::Execute_EndOverlapping(objectToGrab, this);
			objectToGrab = nullptr;
		}

		// If there was an object to grab overlap this object and save its interface for checking variables... (Highlight)
		if (toGrab)
		{
			objectToGrab = toGrab;
			IInteractionInterface::Execute_Overlapping(objectToGrab, this);
		}
	}
}

UObject* AVRHand::LookForInterface(USceneComponent* comp)
{
	// Check the component for the interface then work way up each parent until one is found. If not return null.
	bool componentHasTag = comp->ComponentHasTag("Grabbable");
	bool componentHasInterface = comp->GetClass()->ImplementsInterface(UInteractionInterface::StaticClass());
	if (componentHasInterface) return comp;
	else
	{
		// Check components actor for the interface before searching through its parents.
		bool actorHasTag = comp->GetOwner()->ActorHasTag(FName("Grabbable"));
		bool actorHasInterface = comp->GetOwner()->GetClass()->ImplementsInterface(UInteractionInterface::StaticClass());
		if (actorHasInterface && (actorHasTag || componentHasTag)) return comp->GetOwner();
		// If the components actor doest have the interface and has a parent component look for interface here.
		else if (comp->GetAttachParent())
		{
			// Look through each parent in order from bottom to top, then if nothing is found check the actor itself.
			bool searching = true;
			USceneComponent* parentComponent = comp->GetAttachParent();
			while (searching)
			{
				bool parentHasInterface = parentComponent->GetClass()->ImplementsInterface(UInteractionInterface::StaticClass());
				// If the parent has the interface and is grabbable use this component.
				if (parentHasInterface)
				{
					return parentComponent;
					searching = false;
				}
				else if (parentComponent->GetAttachParent()) parentComponent = parentComponent->GetAttachParent();
				// If there are no more attach parents exit the while loop.
				else searching = false;
			}
		}
	}
	return nullptr;
}

void AVRHand::CheckInteractablesDistance()
{
	if (objectInHand)
	{
		// Get the grabbed objects interface settings.
		FInterfaceSettings grabbedObjectSettings = IInteractionInterface::Execute_GetInterfaceSettings(objectInHand);

		// Get required variables from the current grabbed objects interface.
		float currentHandGrabDistance = grabbedObjectSettings.handDistance;
		float currentHandMinRumbleDist = grabbedObjectSettings.rumbleDistance;

		// Release the intractable if the relative distance becomes greater than the set amount in the interactables interface.
		if (grabbedObjectSettings.releaseDistance < currentHandGrabDistance)
		{
			// Ensure the distance is the same and not a one of frame.
			if (distanceFrameCount > 1)
			{
				ReleaseGrabbedActor();
				distanceFrameCount = 0;
				return;
			}

			distanceFrameCount++;
			return;
		}
		// Otherwise Rumble the hand with the intensity relative to the distance between the hand an grabbed actor. Use default feedback effect.
		else if (currentHandGrabDistance > currentHandMinRumbleDist) PlayFeedback(nullptr, (currentHandGrabDistance - currentHandMinRumbleDist) / 20, true);
		else return;
	}
}

void AVRHand::UpdateFingerTracking()
{
	// Update finger tracking in animation instance.
	UpdateAnimationInstance();

	// Check if grab should be pressed/released.
	// If more than three fingers are down then grab, otherwise if grabbed release.
	fingersClosedAlpha = (currentCurls.Index + currentCurls.Middle + currentCurls.Pinky + currentCurls.Ring + currentCurls.Thumb) / 5.0f;
	if (fingersClosedAlpha > 0.6f)
	{
		if (openedSinceGrabbed)
		{
			Grab();
			openedSinceGrabbed = false;
		}
	}
	else
	{
		Drop();
		openedSinceGrabbed = true;
	}

	// Update the boxes extent and positioning for when the hand is closed or opened. 
	// NOTE: This is overcomplicated because of the way the handskel is attached to the physics box.
	float grabExtentX = FMath::Lerp(9.5f, 4.0f, fingersClosedAlpha);
	handPhysics->SetBoxExtent(FVector(grabExtentX, 2.9f, 5.6f));
	FVector newOffset = handPhysics->GetForwardVector() * (fingersClosedAlpha * -4.0f);
	handHandle->SetLocationOffset(newOffset);
	handSkel->SetRelativeLocation(originalSkelOffset + FVector(fingersClosedAlpha * 4.0f, 0.0f, 0.0f));
}

void AVRHand::UpdateAnimationInstance()
{
	// Get the hand animation class and update animation variables.
	UHandsAnimInstance* handAnim = Cast<UHandsAnimInstance>(handSkel->GetAnimInstance());	
	if (handAnim)
	{

// Handle dev mode curl updating...
#if WITH_EDITOR
		if (devModeEnabled)
		{
			FSteamVRFingerCurls newCurls;
			newCurls.Index = devModeCurlAlpha;
			newCurls.Middle = devModeCurlAlpha;
			newCurls.Ring = devModeCurlAlpha;
			newCurls.Thumb = devModeCurlAlpha;
			newCurls.Pinky = devModeCurlAlpha;
			handAnim->fingerClosingAmount = newCurls.Index;
			handAnim->middleClosingAmount = newCurls.Middle;
			handAnim->ringClosingAmount = newCurls.Ring;
			handAnim->thumbClosingAmount = newCurls.Thumb;
			handAnim->pinkyClosingAmount = newCurls.Pinky;
			currentCurls = newCurls;
			return;
		}
#endif

		// Update anim instances finger curls.
		EHand hand = handEnum == EControllerHand::Left ? EHand::VR_LeftHand : EHand::VR_RightHand;
		FSteamVRFingerCurls curls;
		FSteamVRFingerSplays splays;
		USteamVRInputDeviceFunctionLibrary::GetFingerCurlsAndSplays(hand, curls, splays, ESkeletalSummaryDataType::VR_SummaryType_FromDevice);

		// Update animation variables.
		handAnim->fingerClosingAmount = curls.Index;
		handAnim->middleClosingAmount = curls.Middle;
		handAnim->ringClosingAmount = curls.Ring;
		handAnim->thumbClosingAmount = curls.Thumb;
		handAnim->pinkyClosingAmount = curls.Pinky;
		currentCurls = curls;
	}
}

void AVRHand::UpdateTelekineticGrab()
{
	// Look for lerping grabbable.
	if (!lerpingGrabbable)
	{
		FHitResult sweepResult;
		FVector startLoc = handPhysics->GetComponentLocation();
		FVector endLoc = startLoc + (handPhysics->GetForwardVector() * 1000.0f);
		TArray<TEnumAsByte<EObjectTypeQuery>> grabbableTypes;
		grabbableTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Interactable));
		GetWorld()->SweepSingleByObjectType(sweepResult, startLoc, endLoc, FQuat::Identity, grabbableTypes, FCollisionShape::MakeSphere(15.0f));
		if (sweepResult.bBlockingHit)
		{
			if (AGrabbableActor* isGrabbable = Cast<AGrabbableActor>(sweepResult.Actor))
			{
				lerpingGrabbable = isGrabbable;
				lerpingGrabbable->grabbableMesh->SetCollisionProfileName("PhysicsActorOverlap");
				FVector foundLocation = lerpingGrabbable->grabbableMesh->GetComponentLocation();
				telekineticStartLoc = foundLocation;
				telekineticStartTime = GetWorld()->GetTimeSeconds();
				telekineticLerpSpeed = (FMath::Clamp((foundLocation - controller->GetComponentLocation()).Size(), 0.0f, 1000.0f) / 1000.0f) * 1.5f;
				return;
			}
		}
	}
	// Otherwise lerp the grabbable to the hand.
	else
	{
		float alpha = (GetWorld()->GetTimeSeconds() - telekineticStartTime) / telekineticLerpSpeed;
		FVector endLocation = grabCollider->GetComponentLocation() + (grabCollider->GetRightVector() * (handEnum == EControllerHand::Left ? 8.0f : -8.0f));
		FVector lerpingLocation = FMath::Lerp(telekineticStartLoc, endLocation, alpha);
		lerpingGrabbable->grabbableMesh->SetWorldLocation(lerpingLocation, false, nullptr, ETeleportType::ResetPhysics);
		lerpingGrabbable->grabbableMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
		if (alpha >= 1.0f)
		{
			// Reset interactable settings before grabbing.
			lerpingGrabbable->grabbableMesh->SetCollisionProfileName("Interactable");
			lerpingGrabbable->grabbableMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);

			// Grab the grabbable.
			ForceGrab(lerpingGrabbable);

			// Finished lerping.
			lerpingGrabbable = nullptr;
		}
	}
}

void AVRHand::ResetHandle(UVRPhysicsHandleComponent* handleToReset)
{
	CHECK_RETURN(LogHand, !handleToReset, "The hand class %s, cannot reset a null handle in the ResetPhysicsHandle function.");
	// Reset the joint to its original values.
	handleToReset->ResetJoint();
}

void AVRHand::ResetCollision()
{
	ActivateCollision(false);
	ActivateCollision(true);
}

void AVRHand::ActivateCollision(bool enable, float enableDelay)
{
	if (handSkel)
	{
		// When the hand is open allow all collision to be enabled after a delay while interactables fall out of the way.
		if (enable)
		{
			FTimerDelegate timerDel;
			timerDel.BindUFunction(this, FName("CollisionDelay"));
			GetWorldTimerManager().SetTimer(colTimerHandle, timerDel, 0.1f, true, enableDelay);
			collisionEnabled = true;
		}
		// Disable collision while the hand is closed to prevent accidental interactions.
		else
		{
			handSkel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			handPhysics->SetCollisionProfileName("PhysicsActorOverlap");
			collisionEnabled = false;
			GetWorldTimerManager().ClearTimer(colTimerHandle);
		}

#if WITH_EDITOR
		// Print debug message on activation/deactivation.
		if (debug) UE_LOG(LogHand, Warning, TEXT("Collision in the hand %s, is %s"), *GetName(), collisionEnabled ? TEXT("enabled") : TEXT("disabled"));
#endif
	}
}

void AVRHand::CollisionDelay()
{
	// Only re-enable the collision if the handSkel is no longer overlapping anything.
	TArray<UPrimitiveComponent*> overlappingComps;
	bool overlapping = UVRFunctionLibrary::ComponentOverlapComponentsByChannel(handPhysics, handPhysics->GetComponentTransform(), ECC_PhysicsBody, player->actorsToIgnore, overlappingComps, true);

	// If no longer overlapping any blocking collision to the hand.
	if (!overlapping)
	{
		// Re-enable collision in this classes colliding components.
		handSkel->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		handPhysics->SetCollisionProfileName("PhysicsActor");

		// End this function loop.
		GetWorld()->GetTimerManager().ClearTimer(colTimerHandle);
	}
}

bool AVRHand::PlayFeedback(UHapticFeedbackEffect_Base* feedback, float intensity, bool replace)
{
	if (owningController)
	{
		// Check if should replace feedback effect. If don't replace, still playing and the new intensity is weaker don't play haptic effect.
		bool shouldPlay = replace ? true : (IsPlayingFeedback() ? GetCurrentFeedbackIntensity() < intensity : true);

		// If should play then play the given feedback haptic effect on this hand classes controller.
		if (shouldPlay)
		{
			// If feedback is null use default haptic feedback otherwise use the feedback pointer passed into this function.
			UHapticFeedbackEffect_Base* feedbackToUse = feedback;
			if (!feedbackToUse) feedbackToUse = GetEffects()->GetFeedback("Default");

			// Play the given haptic effect if not nullptr.
			if (feedbackToUse)
			{
				currentHapticIntesity = intensity;
				owningController->PlayHapticEffect(feedbackToUse, handEnum, intensity, false);
				return true;
			}
			else return false;
		}
		else return false;
	}	
	else
	{
	    UE_LOG(LogHand, Log, TEXT("PlayFeedback: The feedback could not be played as the refference to the owning controller has been lost in the hand class %s."), *GetName());
		return false;
	}
}

UEffectsContainer* AVRHand::GetEffects()
{
	if (player && player->IsValidLowLevel()) return player->GetPawnEffects();
	else return nullptr;
}

float AVRHand::GetCurrentFeedbackIntensity()
{
	if (IsPlayingFeedback()) return currentHapticIntesity;
	else return 0.0f;
}

bool AVRHand::IsPlayingFeedback()
{
	bool isPlayingHapticEffect = false;
	if (owningController)
	{
		if (handEnum == EControllerHand::Left && owningController->ActiveHapticEffect_Left.IsValid()) isPlayingHapticEffect = true;
		else if (owningController->ActiveHapticEffect_Right.IsValid()) isPlayingHapticEffect = true;
	}

	// Return if this hand classes controller is playing a haptic effect.
	return isPlayingHapticEffect;
}

void AVRHand::Disable(bool disable)
{
	bool toggle = !disable;

	// Deactivate hand components.
	handSkel->SetActive(toggle);
	grabCollider->SetActive(toggle);

	// Hide hand in game.
	handSkel->SetVisibility(toggle);
	grabCollider->SetVisibility(toggle);

	// Deactivate hand colliders.
	if (toggle)
	{
		handSkel->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		grabCollider->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		handPhysics->SetCollisionProfileName("PhysicsActor");
	}
	else
	{
		handSkel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		grabCollider->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		handPhysics->SetCollisionProfileName("PhysicsActorOverlap");
	}

	// Disable this classes tick.
	this->SetActorTickEnabled(toggle);
	active = toggle;
}
