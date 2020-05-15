// Fill out your copyright notice in the Description page of Project Settings.

#include "Interactables/RotatableStaticMesh.h"
#include "Project/SimpleTimeline.h"
#include "Player/VRHand.h"
#include "DrawDebugHelpers.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/ArrowComponent.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY(LogRotatableMesh);

URotatableStaticMesh::URotatableStaticMesh()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Set collision profile (IMPORTANT)
	SetCollisionProfileName("Interactable");
	ComponentTags.Add("Grabbable");

	// Initialise variables.
	handRef = nullptr;
	grabScene = nullptr;
	rotateMode = ERotationMode::Default;
	grabMode = EGrabMode::Static;
	rotateAxis = ERotateAxis::Yaw;
	fakePhysics = true;
	lockOnlyUpdate = false;
	flipped = false;
	isLimited = false;
	restitution = 0.2f;
	friction = 0.02f;
	rotationLimit = 0.0f;
	startRotation = 0.0f;
	centerRotationLimit = false;
	revolutionCount = 0;
	cumulativeAngle = 0.0f;
	lastAngle = 0.0f;
	maxOverRotation = 50.0f;
	rotationAlpha = 0.0f;
	firstRun = true;
	releaseOnOverRotation = true;
	lockHapticEffect = nullptr;
	lockSound = nullptr;
	lockable = false;
	locked = false;
	lockWhileGrabbed = true;
	grabWhileLocked = true;
	lockingDistance = 2.0f;
	unlockingDistance = 1.0f;
	cannotLock = false;
	releaseWhenLocked = true;
	debugLocking = false;
	interpolating = true;
	lockOnTimelineEnd = false;

#if DEVELOPMENT
	debug = false;
#endif

	// Initialise interface variables.
	interactableSettings.releaseDistance = 30.0f;
	interactableSettings.rumbleDistance = 5.0f;
}

void URotatableStaticMesh::BeginPlay()
{
	Super::BeginPlay();

	// Save original relative transform to compare rotational different when setting new relative rotation in UpdateRotation().
	originalRelativeRotation = GetRelativeTransform().Rotator();

	// Ensure all default variables are applied to private variables.
	cumulativeAngle = startRotation;
	actualCumulativeAngle = startRotation;

	// Calculate current limit at the start of the game.
	if (rotationLimit != 0)
	{
		isLimited = true;
		if (rotationLimit < 0)
		{
			flipped = true;
			currentRotationLimit = FMath::Abs(rotationLimit);
		}
		else
		{
			flipped = false;
			currentRotationLimit = rotationLimit;
		}
	}

	// Setup default cumulative rotation. Enables user to set a default position within the constraint.
	float currentOriginal = GetOriginalRelativeAngle();
	if (currentOriginal != 0.0f)
	{
		// Clamp value within the constraint.
		if (centerRotationLimit)
		{
			float halfLimit = currentRotationLimit / 2;
			cumulativeAngle = FMath::Clamp(currentOriginal, -halfLimit, halfLimit);
		}
		else if (flipped)
		{
			if (currentOriginal <= 0) cumulativeAngle = currentOriginal;
			else cumulativeAngle = (180.0f - currentOriginal) + 180.0f;
			cumulativeAngle = FMath::Clamp(cumulativeAngle, -currentRotationLimit, 0.0f);
		}
		else
		{
			if (currentOriginal <= 0) cumulativeAngle = 180.0f + (180.0f + currentOriginal);
			else cumulativeAngle = currentOriginal;
			cumulativeAngle = FMath::Clamp(cumulativeAngle, 0.0f, currentRotationLimit);
		}
	}

	// If there is a curve set create the simple timer.
	if (rotationUpdateCurve)
	{
		rotationTimeline = USimpleTimeline::CreateSimpleTimeline(rotationUpdateCurve, "RotatableTimeline", this, "UpdateRotatableRotation", "EndRotatableRotation", this->GetOwner());
	}
}

float URotatableStaticMesh::GetOriginalRelativeAngle()
{
	switch (rotateAxis)
	{
	case ERotateAxis::Pitch:
		return originalRelativeRotation.Pitch;
	case ERotateAxis::Yaw:
		return originalRelativeRotation.Yaw;
	case ERotateAxis::Roll:
		return originalRelativeRotation.Roll;
	}
	return 0.0f;
}

FRotator URotatableStaticMesh::GetNewRelativeAngle(float newAngle)
{
	FRotator current = GetRelativeTransform().Rotator();
	switch (rotateAxis)
	{
	case ERotateAxis::Pitch:
		return FRotator(newAngle, current.Yaw, current.Roll);
	case ERotateAxis::Yaw:
		return FRotator(current.Pitch, newAngle, current.Roll);
	case ERotateAxis::Roll:
		return FRotator(current.Pitch, current.Yaw, newAngle);
	}
	return FRotator::ZeroRotator;
}

#if WITH_EDITOR
void URotatableStaticMesh::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	//Get the name of the property that was changed.
	FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	// If the property was the start rotation update it.
	if (PropertyName == GET_MEMBER_NAME_CHECKED(URotatableStaticMesh, startRotation))
	{
		// If the start rotation is changed update the rotation of this rotatable actor if its within the specified rotation limit.
		if (rotationLimit < 0 ? startRotation < 0 && startRotation >= rotationLimit : startRotation >= 0 && startRotation <= rotationLimit)
		{		
			this->SetRelativeRotation(GetNewRelativeAngle(startRotation));

			// Setup default cumulative rotation from current rotation.
			cumulativeAngle = startRotation;
			actualCumulativeAngle = cumulativeAngle;
		}
		// Clamp start rotation within its limits.
		else startRotation = rotationLimit < 0 ? FMath::Clamp(startRotation, rotationLimit, 0.0f) : FMath::Clamp(startRotation, 0.0f, rotationLimit);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void URotatableStaticMesh::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// If grabbed by the hand update the rotation.
	if (handRef)
	{
		UpdateRotatable(DeltaTime);
		UpdateRotation(DeltaTime);
	}
	// Otherwise If the hands release velocity has been set slow down the rotatable using the friction and restitution values to fake physics...	
	else if (angleChangeOnRelease != 0.0f && !interpolating)
	{
		UpdatePhysicalRotation(DeltaTime);
	}
}

void URotatableStaticMesh::UpdateGrabbedRotation()
{
	// Update distance between hand and interactables. Do this by finding the distance between where the hand should be and where it currently is.
	UpdateHandGrabDistance();

	// Get the correct world offset depending on current rotate mode.
	FVector handOffset;
	if (rotateMode == ERotationMode::Twist) handOffset = grabScene->GetComponentLocation();
	else handOffset = handRef->grabCollider->GetComponentLocation();

	// Create a transform from the parent where the origin point is in the correct place, at the meshes origin point.
	FTransform compTransform = GetParentTransform();
	compTransform.SetLocation(GetComponentLocation());
	FVector currentWorldOffset = compTransform.InverseTransformPositionNoScale(handOffset);
	float currentAngleOfHand = 0.0f;
	float originalAngleOfHand = 0.0f;

	// Get the delta rotator normalized of the two angles to get the rotation between the two.
	FRotator currentHandRot, originalHandRot;	
	switch (rotateAxis)
	{
	case ERotateAxis::Pitch:
		currentAngleOfHand = UVRFunctionLibrary::GetPitchAngle(currentWorldOffset);
		originalAngleOfHand = UVRFunctionLibrary::GetPitchAngle(handStartLocation);
		currentHandRot = FRotator(currentAngleOfHand, 0.0f, 0.0f);
		originalHandRot = FRotator(originalAngleOfHand, 0.0f, 0.0f);
		break;
	case ERotateAxis::Yaw:
		currentAngleOfHand = UVRFunctionLibrary::GetYawAngle(currentWorldOffset);
		originalAngleOfHand = UVRFunctionLibrary::GetYawAngle(handStartLocation);
		currentHandRot = FRotator(0.0f, currentAngleOfHand, 0.0f);
		originalHandRot = FRotator(0.0f, originalAngleOfHand, 0.0f);
		break;
	case ERotateAxis::Roll:
		currentAngleOfHand = UVRFunctionLibrary::GetRollAngle(currentWorldOffset);
		originalAngleOfHand = UVRFunctionLibrary::GetRollAngle(handStartLocation);
		currentHandRot = FRotator(0.0f, 0.0f, currentAngleOfHand);
		originalHandRot = FRotator(0.0f, 0.0f, originalAngleOfHand);
		break;
	}
	FRotator rotationOffset = (currentHandRot - originalHandRot).GetNormalized();

	// Get the local rotation from the meshes starting world position as the component disables/re-enables physics so relative rotations are broken/disconnected.
	FRotator finalRotation = meshStartRelative + rotationOffset;

	// Update the current angle.
	switch (rotateAxis)
	{
	case ERotateAxis::Pitch:
		currentAngle = finalRotation.Pitch;
		break;
	case ERotateAxis::Yaw:
		currentAngle = finalRotation.Yaw;
		break;
	case ERotateAxis::Roll:
		currentAngle = finalRotation.Roll;
		break;
	}
}

void URotatableStaticMesh::UpdateRotatable(float DeltaTime)
{
	UpdateGrabbedRotation();

	// Get the current angle change to add/remove from the currentCumulativeAngle.
	if (!firstRun) currentAngleChange = currentAngle - lastAngle;
	else firstRun = false;
	lastAngle = currentAngle;

	// Update the rotation alpha for driving other game mechanics from this rotatable.
	rotationAlpha = centerRotationLimit ? cumulativeAngle / (rotationLimit / 2) : FMath::Abs(cumulativeAngle / rotationLimit);

	// If the angle change is too big/small remove the error.
	if (currentAngleChange < -100.0f) currentAngleChange += 360.0f;
	else if (currentAngleChange > 100.0f) currentAngleChange -= 360.0f;

	// Update the current cumulative angle and clamp it to its max and min rotation in the given axis.
	IncreaseCumulativeAngle(currentAngleChange);

#if DEVELOPMENT
	// Print debugging information...
	if (debug)
	{
		FString className = GetName();
		UE_LOG(LogRotatableMesh, Log, TEXT("The rotatable mesh, %s has a cumulative rotation of:  %s"), *className, *FString::SanitizeFloat(cumulativeAngle));
		UE_LOG(LogRotatableMesh, Log, TEXT("The rotatable mesh, %s has a revolution count of:     %s"), *className, *FString::SanitizeFloat(revolutionCount));
	}
#endif
}

void URotatableStaticMesh::IncreaseCumulativeAngle(float increaseAmount)
{
	// Use actual cumulative angle to keep track of where the hand is relative to the original grabbed position.
	actualCumulativeAngle += increaseAmount;
	cumulativeAngle = actualCumulativeAngle;

	// Only apply clamp if there is a given range.
	if (rotationLimit != 0) 
	{
		// Center the constraint.
		if (centerRotationLimit) 
		{
			float halfLimit = currentRotationLimit / 2;
			cumulativeAngle = FMath::Clamp(cumulativeAngle, -halfLimit, halfLimit);
		}
		else if (flipped) cumulativeAngle = FMath::Clamp(cumulativeAngle, -currentRotationLimit, 0.0f);
		else cumulativeAngle = FMath::Clamp(cumulativeAngle, 0.0f, currentRotationLimit);
	}

	// Update revolution count after it has been clamped.
	revolutionCount = cumulativeAngle / 360.0f;

	// Handle locking functionality if it is enabled.
	// NOTE: Only check if there are locking points in the array.
	// NOTe: Also take care of haptic's and sounds for the locking.
	if (lockable && lockingPoints.Num() > 0) UpdateRotatableLock();
}

bool URotatableStaticMesh::InRange(float Value, float Min, float Max, bool InclusiveMin, bool InclusiveMax)
{
	return ((InclusiveMin ? (Value >= Min) : (Value > Min)) && (InclusiveMax ? (Value <= Max) : (Value < Max)));
}

void URotatableStaticMesh::UpdateRotatableLock()
{
	// If grabbed only lock if lockedWhileGrabbed is enabled.
	if (handRef && !lockWhileGrabbed) return;

	// If currently cannot lock check if we can disable it from current distance from lastUnlockAngle.
	if (cannotLock)
	{
		// Set can lock to true if hand is no longer grabbing.
		if (!FMath::IsNearlyEqual(cumulativeAngle, lastUnlockAngle, unlockingDistance) || !handRef)
		{
			cannotLock = false;
			lastCheckedRotation = cumulativeAngle;
		}
	}
	// Otherwise look for closest locking angle and lock at that angle.
	else
	{
		// For each locking point check 
		float closestRotationFound = BIG_NUMBER;
		bool pointFound = false;
		for (float point : lockingPoints)
		{
			// If the last checked rotation to the current rotation has passed the current point and is smaller relative to the last checked rotation, lock at said point.
			bool hasPassedLock = lastCheckedRotation < cumulativeAngle ? InRange(point, lastCheckedRotation, cumulativeAngle) : // Last is smaller
																		 InRange(point, cumulativeAngle, lastCheckedRotation);  // Last is bigger
			if (hasPassedLock && point < closestRotationFound)
			{
				closestRotationFound = point;
				pointFound = true;
			}
		}

		// Lock to point if one was found. 
		if (pointFound)
		{
			// Lock the rotatable at the found angle.
			OnRotatableLock.Broadcast(closestRotationFound);
			Lock(closestRotationFound);		
			currentLockedRotation = closestRotationFound;
		}

		// Save last rotation checked.
		lastCheckedRotation = cumulativeAngle;
	}
}

void URotatableStaticMesh::Lock(float lockingAngle)
{
	if (lockable)
	{
		// If lock haptic effect is enable and not null play on hand then release. Also play sound.
		if (handRef)
		{
			if (lockHapticEffect) handRef->PlayFeedback(lockHapticEffect, 1.0f);
			if (releaseWhenLocked) handRef->ReleaseGrabbedActor();
		}

		// Lock.
		cumulativeAngle = lockingAngle;
		actualCumulativeAngle = cumulativeAngle;
		SetRelativeRotation(GetNewRelativeAngle(cumulativeAngle));
		angleChangeOnRelease = 0.0f; // Ensure theres no faking of physics once locked.

		// If this lock cannot be grabbed while locked prevent this interactable from being grabbed.
		if (!grabWhileLocked) interactableSettings.active = false;

		// Play locking sound. Only if there is a locking sound.
		if (lockSound) UGameplayStatics::PlaySoundAtLocation(GetWorld(), lockSound, GetComponentLocation());

		// Log locking if debug is enabled.
		if (debugLocking) UE_LOG(LogRotatableMesh, Warning, TEXT("The Rotatable %s was locked at rotation %f."), *GetName(), lockingAngle);

		// Now locked.
		locked = true;
		firstRun = true;// Bug Fix. Last yaw angle problem.
		cannotLock = true;// Bug Fix. Keeps running lock while grabbed after it locks into rot.
	}
}

void URotatableStaticMesh::Unlock()
{
	// Unlock this rotatable.
	if (lockable && locked)
	{
		if (!grabWhileLocked) interactableSettings.active = true;
		lastUnlockAngle = cumulativeAngle;
		locked = false;

		// Log unlocking action if debug is enabled.
		if (debugLocking) UE_LOG(LogRotatableMesh, Warning, TEXT("The Rotatable %s was unlocked."), *GetName());
	}
}

void URotatableStaticMesh::UpdateRotation(float DeltaTime)
{
	if (!lockOnlyUpdate)
	{
		// Convert the cumulative angle back into word rotation format.
		float actualAngle = UVRFunctionLibrary::GetAngleFromCumulativeAngle(cumulativeAngle);

		// Get the final relative rotation.
		FRotator updatedRotation = GetNewRelativeAngle(actualAngle);

		// Use correct update method for current grabbing mode.
		switch (grabMode)
		{
		case EGrabMode::Static:
			SetRelativeRotation(updatedRotation);
		break;
		case EGrabMode::Physics:
			// TODO
		break;
		}
	}
}

void URotatableStaticMesh::UpdatePhysicalRotation(float DeltaTime)
{
	// Check for the constraint being clamped only when there is a rotational limit.
	if (rotationLimit != 0.0f)
	{
		// Check if the constraint is going to be clamped, if so it means it has hit a constraint wall and should bounce off using the restitution value.
		bool shouldFlip = false;
		if (centerRotationLimit)
		{
			if (cumulativeAngle <= -currentRotationLimit / 2 || cumulativeAngle >= currentRotationLimit / 2) shouldFlip = true;
		}
		else if (flipped)
		{
			if (cumulativeAngle <= -currentRotationLimit || cumulativeAngle >= 0.0f) shouldFlip = true;
		}
		else
		{
			if (cumulativeAngle <= 0.0f || cumulativeAngle >= currentRotationLimit) shouldFlip = true;
		}

		// Flip the angle change direction and apply restitution damping...
		if (shouldFlip) angleChangeOnRelease = (angleChangeOnRelease * restitution) * -1;
	}
		
	// Update the new cumulative rotation from the angle change over time using the friction values...
	IncreaseCumulativeAngle(angleChangeOnRelease);
	angleChangeOnRelease = angleChangeOnRelease - (angleChangeOnRelease * FMath::Clamp(friction, 0.0f, 0.2f));
	if (FMath::IsNearlyEqual(angleChangeOnRelease, 0.0f, 0.01f)) angleChangeOnRelease = 0.0f;

	// Update the rotation.
	UpdateRotation(DeltaTime);
}

void URotatableStaticMesh::CreateSceneComp(USceneComponent* connection, FVector worldLocation)
{
	grabScene = NewObject<USceneComponent>(this, FName("grabScene"));
	grabScene->SetMobility(EComponentMobility::Movable);
	grabScene->RegisterComponent();
	grabScene->SetWorldLocation(worldLocation);
	grabScene->AttachToComponent(connection, FAttachmentTransformRules::KeepWorldTransform);
}

FTransform URotatableStaticMesh::GetParentTransform()
{
	FTransform parentTransform;
	if (GetAttachParent()) parentTransform = GetAttachParent()->GetComponentTransform();
	else if (GetOwner()) parentTransform = GetOwner()->GetActorTransform();
	return parentTransform;
}

void URotatableStaticMesh::UpdateHandGrabDistance()
{
	// Release from hand if the actualCumulativeAngle becomes too much greater than the cumulative angle.
	if (releaseOnOverRotation && FMath::Abs(actualCumulativeAngle - cumulativeAngle) >= maxOverRotation)
	{
		interactableSettings.handDistance = interactableSettings.releaseDistance + 1;
	}
	else 
	{
		// Check the distance from the hand grabbed position to the current hand position.
		if (rotateMode == ERotationMode::Twist)
		{
			FVector currentHandExpectedOffset = GetParentTransform().TransformPositionNoScale(twistingHandOffset);
			interactableSettings.handDistance = FMath::Abs((currentHandExpectedOffset - handRef->grabCollider->GetComponentLocation()).Size());
#if DEVELOPMENT
			if (debug) 
			{
				// Draw debugging information.
				DrawDebugPoint(GetWorld(), currentHandExpectedOffset, 5.0f, FColor::Blue, true, 0.0f, 0.0f); // Draw expected hand position.
				DrawDebugPoint(GetWorld(), grabScene->GetComponentLocation(), 5.0f, FColor::Red, true, 0.0f, 0.0f); // Draw direction position.
			}
#endif
		}
		else
		{
			interactableSettings.handDistance = FMath::Abs((grabScene->GetComponentLocation() - handRef->grabCollider->GetComponentLocation()).Size());
#if DEVELOPMENT
			if (debug) DrawDebugPoint(GetWorld(), grabScene->GetComponentLocation(), 5.0f, FColor::Blue, true, 0.0f, 0.0f); // Draw expected hand position.
#endif
		}
	}	
#if DEVELOPMENT
	// Draw the hands current position.
	if (debug) DrawDebugPoint(GetWorld(), handRef->grabCollider->GetComponentLocation(), 5.0f, FColor::Green, true, 0.0f, 0.0f);
#endif
}

void URotatableStaticMesh::SetRotatableRotation(float angle, bool lockAtAngle, bool interpolate)
{
	// Create angle to set by clamping within limits.
	float absHalfRotationLimit = FMath::Abs(rotationLimit) / 2;
	float clampedAngle = centerRotationLimit ? FMath::Clamp(angle, -absHalfRotationLimit, absHalfRotationLimit) : // Centered.
		rotationLimit < 0 ? FMath::Clamp(angle, rotationLimit, 0.0f) : FMath::Clamp(angle, 0.0f, rotationLimit);  // Not centered.

	// Start timeline to interpolate to the set angle.
	if (interpolate)
	{
		if (rotationUpdateCurve)
		{
			// Setup timeline variables.
			interpolating = true;
			timelineStartRotation = cumulativeAngle;		
			timelineEndRotation = clampedAngle;
			lockOnTimelineEnd = lockAtAngle;

			// Start timeline.
			rotationTimeline->PlayFromStart();
			return;
		}
		
		// Print debug message about no timeline curve being set.
		UE_LOG(LogRotatableMesh, Warning, TEXT("RotatableMesh %s, cannot interpolate as there is no timeline curve set, setting location instantly instead..."), *GetName());
	}

	// Otherwise set angle instantly.
	cumulativeAngle = clampedAngle;
	actualCumulativeAngle = cumulativeAngle;
	rotationAlpha = centerRotationLimit ? cumulativeAngle / (rotationLimit / 2) : FMath::Abs(cumulativeAngle / rotationLimit);
	UpdateRotation();
	angleChangeOnRelease = 0.0f;

	// Lock at angle if set to true.
	if (lockAtAngle)
	{
		lockable = true;
		Lock(cumulativeAngle);
	}
}

void URotatableStaticMesh::UpdateRotatableRotation(float val)
{
	float newCumulativeAngle = FMath::Lerp(timelineStartRotation, timelineEndRotation, val);
	cumulativeAngle = newCumulativeAngle;
	actualCumulativeAngle = cumulativeAngle;
	rotationAlpha = centerRotationLimit ? cumulativeAngle / (rotationLimit / 2) : FMath::Abs(cumulativeAngle / rotationLimit);
	UpdateRotation();
}

void URotatableStaticMesh::EndRotatableRotation()
{
	// Disable interpolating variable.
	interpolating = false;
	angleChangeOnRelease = 0.0f;

	// If lock is enabled do so at the end of the timeline.
	if (lockOnTimelineEnd)
	{
		lockable = true;
		Lock(cumulativeAngle);
	}
}

void URotatableStaticMesh::Grabbed_Implementation(class AVRHand* hand)
{
	// Broadcast grabbed delegate.
	OnRotatableGrabbed.Broadcast(hand);

	// If interpolating the timeline must be running and created so end it.
	if (interpolating)
	{
		rotationTimeline->Pause();
	}

	// Unlock if currently locked.
	if (locked) Unlock();
	handRef = hand;
	angleChangeOnRelease = 0.0f; 

	// Grab using the correct methods.
	switch (rotateMode)
	{
	case ERotationMode::Default:
		CreateSceneComp(this, handRef->grabCollider->GetComponentLocation());
	break;
	case ERotationMode::Twist:
	{
		FVector twistSceneLoc;
		switch (rotateAxis)
		{
		case ERotateAxis::Pitch:
			twistSceneLoc = GetComponentLocation() + (GetUpVector() * 100.0f);
			break;
		case ERotateAxis::Yaw:
			twistSceneLoc = GetComponentLocation() + (GetRightVector() * 100.0f);
			break;
		case ERotateAxis::Roll:
			twistSceneLoc = GetComponentLocation() + (GetForwardVector() * 100.0f);
			break;
		}
		CreateSceneComp(handRef->controller, twistSceneLoc);
		twistingHandOffset = GetParentTransform().InverseTransformPositionNoScale(handRef->grabCollider->GetComponentLocation());
	}
	break;
	}

	// Save the current rotation so it can be compared later.
	meshStartRelative = GetRelativeTransform().Rotator();

	// Save the hand start location for later calculations.
	handStartLocation = GetParentTransform().InverseTransformPositionNoScale(grabScene->GetComponentLocation());
}

void URotatableStaticMesh::Released_Implementation(AVRHand* hand)
{
	// Ensure these are the same when let go as the hand is no longer interacting with this interactable.
	actualCumulativeAngle = cumulativeAngle;

	// Set angle change on release to run fake physics if enabled.
	if (fakePhysics)
	{
		angleChangeOnRelease = currentAngleChange;
	}

	// Reset class variables.
	AVRHand* oldHand = handRef;
	handRef = nullptr;
	firstRun = true;
	grabScene->DestroyComponent();

	// Run released delegate.
	OnRotatableReleased.Broadcast(oldHand);
}

void URotatableStaticMesh::Dragging_Implementation(float deltaTime)
{
	//...
}

void URotatableStaticMesh::Overlapping_Implementation(AVRHand* hand)
{
	IInteractionInterface::Overlapping_Implementation(hand);

	//...
}

void URotatableStaticMesh::EndOverlapping_Implementation(AVRHand* hand)
{
	IInteractionInterface::EndOverlapping_Implementation(hand);

	//...
}

FInterfaceSettings URotatableStaticMesh::GetInterfaceSettings_Implementation()
{
	return interactableSettings;
}

void URotatableStaticMesh::SetInterfaceSettings_Implementation(FInterfaceSettings newInterfaceSettings)
{
	interactableSettings = newInterfaceSettings;
}

