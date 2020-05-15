// Fill out your copyright notice in the Description page of Project Settings.

#include "Interactables/InteractableActor.h"

DEFINE_LOG_CATEGORY(LogInteractable);

AInteractableActor::AInteractableActor()
{
	PrimaryActorTick.bCanEverTick = true;

	debugSettings = false;

	// Setup default interface variables.
	interactableSettings.releaseDistance = 30.0f;
	interactableSettings.rumbleDistance = 10.0f;
}

void AInteractableActor::BeginPlay()
{
	Super::BeginPlay();

	//...
}

void AInteractableActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

#if DEVELOPMENT
	// Print interface settings.
	if (debugSettings) UE_LOG(LogInteractable, Warning, TEXT("%s"), *interactableSettings.ToString());
#endif
}

void AInteractableActor::Grabbed_Implementation(AVRHand* hand)
{
	GrabbedBP(hand);
}

void AInteractableActor::Released_Implementation(AVRHand* hand)
{
	ReleasedBP(hand);
}

void AInteractableActor::Squeezing_Implementation(AVRHand* hand, float howHard)
{
	SqueezingBP(hand, howHard);
}

void AInteractableActor::Interact_Implementation(bool pressed)
{
	InteractBP(pressed);
}

void AInteractableActor::Dragging_Implementation(float deltaTime)
{
	DraggingBP(deltaTime);
}

void AInteractableActor::Overlapping_Implementation(AVRHand* hand)
{
	IInteractionInterface::Overlapping_Implementation(hand);
	OverlappingBP(hand);
}

void AInteractableActor::EndOverlapping_Implementation(AVRHand* hand)
{
	IInteractionInterface::EndOverlapping_Implementation(hand);
	EndOverlappingBP(hand);
}

void AInteractableActor::Teleported_Implementation()
{
	TeleportedBP();
}

FInterfaceSettings AInteractableActor::GetInterfaceSettings_Implementation()
{
	return interactableSettings;
}

void AInteractableActor::SetInterfaceSettings_Implementation(FInterfaceSettings newInterfaceSettings)
{
	interactableSettings = newInterfaceSettings;
}