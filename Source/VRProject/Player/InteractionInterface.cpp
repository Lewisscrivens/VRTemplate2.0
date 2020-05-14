// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/InteractionInterface.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ShapeComponent.h"
#include "VRHand.h"

DEFINE_LOG_CATEGORY(LogInteractionInterface);

void IInteractionInterface::Grabbed_Implementation(AVRHand* hand)
{

}

void IInteractionInterface::Released_Implementation(AVRHand* hand)
{

}

void IInteractionInterface::Dragging_Implementation(float deltaTime)
{

}

void IInteractionInterface::Interact_Implementation(bool pressed)
{

}

void IInteractionInterface::Overlapping_Implementation(AVRHand* hand)
{
	// Get the interfaces settings.
	UObject* objectClass = _getUObject();
	FInterfaceSettings currentSettings = IInteractionInterface::Execute_GetInterfaceSettings(objectClass);

	// Add the overlapped hand.
	overlappingHands.Add(hand);

	// Continue if not already overlapping and the class Pointer is set.
	if (objectClass)
	{
		if (!overlapping && currentSettings.hightlightInteractable)
		{
			bool loopFoundComponents = false;
			// If the class extending this interface is an actor loop through its components to find which elements to highlight. (Anything with the tag "Grabbable")
			AActor* isClassActor = Cast<AActor>(objectClass);
			if (isClassActor)
			{
				// If the actor is grabbable then highlight everything...
				if (isClassActor->ActorHasTag(FName("Grabbable"))) foundChildren = isClassActor->GetComponents().Array();
				// Otherwise find and highlight only the components with the tag grabbable...
				else foundChildren = isClassActor->GetComponentsByTag(UPrimitiveComponent::StaticClass(), FName("Grabbable"));
				// Loop through the components.
				loopFoundComponents = true;
			}
			// Otherwise check if the class is a grabbable component as there will be no need to check children etc.
			else
			{
				UPrimitiveComponent* isClassComponent = Cast<UPrimitiveComponent>(objectClass);
				if (isClassComponent)
				{
					foundChildren.Add(isClassComponent);
					loopFoundComponents = true;
				}
			}
			// Loop through the found children.
			if (loopFoundComponents && foundChildren.Num() > 0)
			{
				for (UActorComponent* comp : foundChildren)
				{
					UPrimitiveComponent* primComp = Cast<UPrimitiveComponent>(comp);
					if (primComp)
					{
						// Highlight the objects that are grabbable.
						primComp->SetCustomDepthStencilValue(2);
						primComp->SetRenderCustomDepth(true);
						foundComponents.Add(primComp);
					}
				}
				foundChildren.Empty();
				overlapping = true;
			}
		}		
	}
	else UE_LOG(LogInteractionInterface, Warning, TEXT("A value must be set for the class pointer variable for overlapping to work. (HandsInterface)"));
}

void IInteractionInterface::EndOverlapping_Implementation(AVRHand* hand)
{
	// Get the interfaces settings.
	UObject* objectClass = _getUObject();
	FInterfaceSettings currentSettings = IInteractionInterface::Execute_GetInterfaceSettings(objectClass);

	// Remove the overlapped hand.
	overlappingHands.Remove(hand);

	// Only end overlap if there are no hands still overlapping, their are some found components and highlight intractable is true.
	if (objectClass)
	{
		if (overlappingHands.Num() == 0 && currentSettings.hightlightInteractable)
		{
			// If there are any found components that are highlighted in this interfaces child, end the highlight material on each component.
			if (foundComponents.Num() > 0)
			{
				// Loop through previously found components from the overlapping function and reset their custom depth values.
				for (UPrimitiveComponent* comp : foundComponents)
				{
					if (comp->bRenderCustomDepth)
					{
						// Reset stencil value to 0 which has no post process material.
						comp->SetCustomDepthStencilValue(0);
						comp->SetRenderCustomDepth(false);
					}
				}
				// Empty the previously found components.
				foundComponents.Empty();
				// End overlapping.
				overlapping = false;
			}
		}
	
	}
	else UE_LOG(LogInteractionInterface, Warning, TEXT("A value must be set for the rootComponentPointer variable for end overlapping to work. (HandsInterface)"));
}

void IInteractionInterface::Squeezing_Implementation(AVRHand* hand, float howHard)
{

}

void IInteractionInterface::Teleported_Implementation()
{

}

FInterfaceSettings IInteractionInterface::GetInterfaceSettings_Implementation()
{
	FInterfaceSettings dummySettings = FInterfaceSettings();
	UE_LOG(LogInteractionInterface, Warning, TEXT("Override get interface settings!! Otherwise all values will be default from code."));
	return dummySettings;
}

void IInteractionInterface::SetInterfaceSettings_Implementation(FInterfaceSettings newInterfaceSettings)
{
	UE_LOG(LogInteractionInterface, Warning, TEXT("Setting interface settings did not work as SetInterfaceSettings has no override."));
}
