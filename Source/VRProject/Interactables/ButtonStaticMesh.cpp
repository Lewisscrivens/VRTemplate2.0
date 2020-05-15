// Fill out your copyright notice in the Description page of Project Settings.

#include "ButtonStaticMesh.h"
#include "BoxSphereBounds.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Project/VRFunctionLibrary.h"
#include "Player/VRHand.h"
#include "GrabbableActor.h"

DEFINE_LOG_CATEGORY(LogButtonMesh);

// Sets default values for this component's properties
UButtonStaticMesh::UButtonStaticMesh()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Initialize the default button. Only enable collision for button when fully pressed down.
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionProfileName(FName("BlockAll"));
	SetGenerateOverlapEvents(true);

	// Initialise default variables.
	buttonMode = EButtonMode::Default;
	shapeTraceType = EButtonTraceCollision::Sphere;
	buttonOffset = FVector(0.0f, 0.0f, 1.0f);
	sphereSize = 0.0f;
	travelDistance = 4.0f;
	buttonIsUpdating = true;
	interpToPosition = false;
	on = false;
	keepingPos = false;
	alreadyToggled = false;
	hapticFeedbackEnabled = true;
	onPercentage = 0.8f;
	interpolationSpeed = 10.0f;
	pressSpeed = 18.0f;
	soundIntensity = soundPitch = 1.0f;

#if DEVELOPMENT
	debug = false;
#endif
}

void UButtonStaticMesh::BeginPlay()
{
	Super::BeginPlay();

	// Save the starting relative location of this pushable mesh.
	startTransform = GetComponentTransform();
	startRelativeTransform = GetRelativeTransform();

	// Ignore self in update button function when sweep tracing.
	ignoredActors.Add(GetOwner());

	// Get the local bounds for this component that are not effected by the current transform.
	if (this->GetStaticMesh())
	{
		FBoxSphereBounds bounds = GetStaticMesh()->GetBounds();
		buttonExtent = bounds.BoxExtent * GetComponentScale();
		sphereSize = bounds.SphereRadius * GetComponentScale().GetMax();

		// Reduce the x and y extent as it gave better results visually.
		buttonExtent.X *= 0.9f;
		buttonExtent.Y *= 0.9f;
	}

	// Ensure travel distance is positive.
	onDistance = travelDistance * onPercentage;

	// Get the world positions of each state of this button with its offset.
	FVector startWorldPosition = startTransform.TransformPositionNoScale(buttonOffset);
	FVector endWorldPosition = startWorldPosition - (GetUpVector() * travelDistance);
	FVector onPosition = startWorldPosition - (GetUpVector() * onDistance);

	// Relative button positions including button offsets.
	endPositionRel = GetParentTransform().InverseTransformPositionNoScale(endWorldPosition);
	startPositionRel = GetParentTransform().InverseTransformPositionNoScale(startWorldPosition);
	onPositionRel = GetParentTransform().InverseTransformPositionNoScale(onPosition);

	// Location for lerping and shape traces.
	lerpRelativeLocation = startRelativeTransform.GetLocation();
	endTraceToUse = startPositionRel;
}

void UButtonStaticMesh::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// If update button is enabled update the button.
	if (buttonIsUpdating && !forcePressed) UpdateButtonPosition();
	if (interpToPosition) InterpButtonPosition(DeltaTime);
}

void UButtonStaticMesh::UpdateButtonPosition()
{
	// Get owners transform.
	FTransform parentTransform = GetParentTransform();

	// Get the start and end trace locations. These locations will have to be calculated from relative positions found in the begin play function.
	FVector startCurrentWorldLocation = parentTransform.TransformPositionNoScale(endPositionRel);
	FVector endCurrentWorldLocation = parentTransform.TransformPositionNoScale(endTraceToUse);

	// Setup debugging variable.
	EDrawDebugTrace::Type drawDebug = EDrawDebugTrace::None;
#if DEVELOPMENT	
	if (debug)
	{
		drawDebug = EDrawDebugTrace::ForOneFrame;
		DrawDebugPoint(GetWorld(), parentTransform.TransformPositionNoScale(onPositionRel), 10.0f, FColor::Red, false, 0.1f, 0.0f);// OnPos
		DrawDebugPoint(GetWorld(), parentTransform.TransformPositionNoScale(endPositionRel), 10.0f, FColor::Green, false, 0.1f, 0.0f);// EndPos
		DrawDebugPoint(GetWorld(), GetComponentTransform().TransformPositionNoScale(buttonOffset), 10.0f, FColor::Blue, false, 0.1f, 0.0f);// CurrentPos
		DrawDebugPoint(GetWorld(), parentTransform.TransformPositionNoScale(endTraceToUse), 10.0f, FColor::Purple, false, 0.1f, 0.0f);// StartPos
	}
#endif

	// Use kismet system library for handling the debugging for shape traces.
	if (shapeTraceType != EButtonTraceCollision::Box) UKismetSystemLibrary::SphereTraceSingleByProfile(GetWorld(), startCurrentWorldLocation, endCurrentWorldLocation, sphereSize, "Interactable", false, ignoredActors, drawDebug, buttonHit, true);
	else UKismetSystemLibrary::BoxTraceSingleByProfile(GetWorld(), startCurrentWorldLocation, endCurrentWorldLocation, buttonExtent, GetComponentTransform().Rotator(), "Interactable", false, ignoredActors, drawDebug, buttonHit, true);

	// If the button trace has hit anything.
	if (buttonHit.bBlockingHit && !cannotPress)
	{
		// Prevent button from being pressed from behind/underneath.
		FVector impactOffset = GetComponentTransform().InverseTransformPositionNoScale(buttonHit.ImpactPoint);
		if (impactOffset.Z > 0.0f)
		{
			// Get the current relative transform for the impact offset. Done this way in-case button is on moving object.
			FVector offset = buttonHit.Location - parentTransform.TransformPositionNoScale(startPositionRel);
			FVector relativeButtonPosition = startRelativeTransform.TransformPositionNoScale(-startPositionRel.UpVector * offset.Size());
			SetRelativeLocation(relativeButtonPosition);

			// Stop lerping into position if something is hitting the button.
			if (interpToPosition) interpToPosition = false;

			// Update the buttons values if the has gone past the current on distance. result = button on.
			FVector offsetRel = startRelativeTransform.InverseTransformPositionNoScale(relativeButtonPosition);
			bool result = offsetRel.Z <= -onDistance;
			switch (buttonMode)
			{
				case EButtonMode::Default:

					// By default just set the button to on or off Dependant on its current position. (Has to be held at positions on returns to default position).
					if (on != result) UpdateButton(result);

				break;
				case EButtonMode::Toggle:

					// Toggle between on and off.
					if (!alreadyToggled && result) UpdateButton(!on);

				break;
				case EButtonMode::KeepPosition:

					// Switch between different modes but only one switch per continuous collision.
					if (result)
					{
						if (!keepingPos)
						{
							if (!on)
							{
								UpdateButton(true);
								lerpRelativeLocation = RemoveRelativeOffset(onPositionRel);
								endTraceToUse = onPositionRel;
							}
							else
							{
								UpdateButton(false);
								lerpRelativeLocation = startRelativeTransform.GetLocation();
								endTraceToUse = startPositionRel;
							}
						}
					}
					// Otherwise if went over on position disable keepingPos.
					else if (keepingPos) keepingPos = false;

				break;
				case EButtonMode::SingleUse:

					// If the button is single use, lock it after it has fully been pressed down.
					if (result == true && !on)
					{
						UpdateButton(true);
						locked = true;
						lerpRelativeLocation = startRelativeTransform.GetLocation();
						endTraceToUse = onPositionRel;
					}

				break;
			}

			// Only enable this static meshes collision when at its end state. (For physics handle grabbables in the hand fix.)
			bool fullyDown = offsetRel.Z <= -travelDistance;
			if (fullyDown && GetCollisionEnabled() == ECollisionEnabled::QueryOnly) SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			else if (GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics) SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		}		
	}
	else// If nothing is hit interp back to the current interpToPosition position.
	{
		// If the button is still on but nothing is hitting set the button to off.
		if (buttonMode == EButtonMode::Default && on) UpdateButton(false);
		interpToPosition = true;
		keepingPos = false;
		alreadyToggled = false;
	}
}

FVector UButtonStaticMesh::RemoveRelativeOffset(FVector relativeVector)
{
 	// Remove the button offset from the relative location passed into this function.
 	FTransform currentRelativeTransform = startRelativeTransform;
 	currentRelativeTransform.SetLocation(relativeVector);
	return currentRelativeTransform.TransformPositionNoScale(-buttonOffset);
}

void UButtonStaticMesh::UpdateButton(bool isOn)
{
	class USoundBase* soundToUse = nullptr;

	// Set 'on' value and changed the soundToUse respectively.
	if (isOn)
	{
		soundToUse = buttonPressed;
		on = true;
	}
	else
	{
		soundToUse = buttonReleased;
		on = false;
	}

	// Play haptic feedback on the hand if overlapping this component and button state has changed.
	if (hapticFeedbackEnabled)
	{
		if (buttonHit.GetActor())
		{
			if (AVRHand* foundHand = Cast<AVRHand>(buttonHit.GetActor()))
			{
				// If there is a haptic effect use it, otherwise use the default haptic effect (Handled in rumble controller function).
				foundHand->PlayFeedback(hapticEffect);
			}
			else if (AGrabbableActor* foundGrabbable = Cast<AGrabbableActor>(buttonHit.GetActor()))
			{
				// Otherwise if there is a grabbable find the hand holding the grabbable and play haptic effect.
				if (AVRHand* hand = foundGrabbable->otherGrabInfo.handRef) hand->PlayFeedback(hapticEffect);
			}
		}
	}

	// Broadcast to delegate events.
	Pressed.Broadcast(on);

	// Play on/off audio.
	UGameplayStatics::PlaySoundAtLocation(GetWorld(), soundToUse, GetComponentLocation(), soundIntensity, soundPitch, 0.0f, soundAttenuation);
	keepingPos = true;
	alreadyToggled = true;

#if DEVELOPMENT
	// If debug print the resulting changed on value.
	if (debug) UE_LOG(LogButtonMesh, Warning, TEXT("The pressable mesh, %s has a new on value of: %s"), *GetName(), SBOOL(on));
#endif
}

void UButtonStaticMesh::InterpButtonPosition(float DeltaTime)
{
	// Interpolate the buttons relative z location down to a pressed state or back up to the unpressed state.
	FVector lerpingLocation = UKismetMathLibrary::VInterpTo(GetRelativeLocation(), lerpRelativeLocation, DeltaTime, interpolationSpeed);
	SetRelativeLocation(lerpingLocation);

	// Reset the buttons interpolation values after the interpolation is complete.
	if (lerpingLocation == lerpRelativeLocation)
	{
		interpToPosition = false;
		
		// Reset interpolation values if need be.
		if (resetInterpolationValues)
		{		
			resetInterpolationValues = false;
			Pressed.Broadcast(turnOn);
			if (!turnOn) forcePressed = false;
		}
	}
}

void UButtonStaticMesh::PressButton()
{
	// Interpolate the button down.
	interpToPosition = true;
	lerpRelativeLocation = RemoveRelativeOffset(endPositionRel);
	oldInteractionSpeed = interpolationSpeed;
	interpolationSpeed = pressSpeed;
	resetInterpolationValues = true;
	forcePressed = true;
	turnOn = true;
}

void UButtonStaticMesh::ReleaseButton()
{
	// Interpolate button back to start.
	interpToPosition = true;
	lerpRelativeLocation = startRelativeTransform.GetLocation();
	interpolationSpeed = oldInteractionSpeed;
	resetInterpolationValues = true;
	turnOn = false;
}

void UButtonStaticMesh::ResetButton()
{
	// Reset the button variables to default.
	on = false;
	locked = false;
	interpToPosition = true;
	keepingPos = false;
	lerpRelativeLocation = startRelativeTransform.GetLocation();
	endTraceToUse = startPositionRel;
}

FTransform UButtonStaticMesh::GetParentTransform()
{
	FTransform parentTransform;
	if (GetAttachParent()) parentTransform = GetAttachParent()->GetComponentTransform();
	else if (GetOwner()) parentTransform = GetOwner()->GetActorTransform();
	return parentTransform;
}

void UButtonStaticMesh::UpdateAudio(USoundBase* downSound, USoundBase* upSound, float intesity, float pitch, USoundAttenuation* attenuation)
{
	// Update audio variables.
	buttonPressed = downSound;
	buttonReleased = upSound;
	soundIntensity = intesity;
	soundPitch = pitch;
	soundAttenuation = attenuation;
}
