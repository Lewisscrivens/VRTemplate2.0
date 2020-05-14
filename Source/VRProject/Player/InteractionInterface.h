// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Player/VRPhysicsHandleComponent.h"
#include "Globals.h"
#include "InteractionInterface.generated.h"

/** Declare interaction delegate for running and passing a hand reference to a given interactable. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInteraction, class AVRHand*, handRef);

/** Define this classes log category. */
DECLARE_LOG_CATEGORY_EXTERN(LogInteractionInterface, Log, All);

/** Declare classes used. */
class AVRHand;
class UActorComponent;
class UPrimitiveComponent;

/** Interface settings structure, used to hold any interface variables that will be changed and used in the hand class.
 * Makes changing this interfaces variables in BP possible... */
USTRUCT(BlueprintType)
struct FInterfaceSettings
{
	GENERATED_BODY()

public:

	/** If the object is physical what are the grab handle settings to use... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandInterface")
		FPhysicsHandleData physicsData;
	/** Distance that the hand can be away from an interacting component. For example the component will be released from the hand if the distance becomes greater than this value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandInterface")
		float releaseDistance;
	/** Distance that the hand must get away from the intractable before the rumble function is called. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandInterface")
		float rumbleDistance;
	/** Current Distance between the hand and intractable when first grabbed used to determine when to rumble the hand on too much distance.  */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "HandInterface")
		float handDistance;
	/** Enable and disable highlight material functionality thats implemented in the hands interface class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandInterface")
		bool hightlightInteractable;
	/** Two handed grab mode, don't release component from current grabbing hand when grabbed by another hand. NOTE: Best for weapons etc. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandInterface")
		bool twoHandedGrabbing;
	/** Used to stop this interface from being interacted with. Useful for disabling things temporarily. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandInterface")
		bool active;	

	/** Constructor for this struct. Defaults to default interface values... */
	FInterfaceSettings(FPhysicsHandleData handleData = FPhysicsHandleData(), float releaseDist = 50.0f, float handMinRumbleDist = 1.0f,
		bool twoHanded = false, bool highlight = true, bool interactEnabled = true, float currentHandDist = 0.0f)
	{
		this->physicsData = handleData;
		this->releaseDistance = releaseDist;
		this->rumbleDistance = handMinRumbleDist;
		this->handDistance = currentHandDist;
		this->hightlightInteractable = highlight;
		this->twoHandedGrabbing = twoHanded;
		this->active = interactEnabled;	
	}

	/** Convert the values of this struct into a printable message/string for debugging. */
	FString ToString()
	{
		FString handleDataString = FString::Printf(TEXT("Handle Data = %s \n Release Distance = %f \n Rumble Distance = %f \n Current Distance = %f \n Should Highlight = %s \n Active? = %s"),
			*physicsData.ToString(),
			releaseDistance,
			rumbleDistance,
			handDistance,
			SBOOL(hightlightInteractable),
			SBOOL(active));
			
		return handleDataString;
	}
};

//=================================
// Interface
//=================================

/** Holder of interface class... */
UINTERFACE(Blueprintable, BlueprintType, MinimalAPI)
class UInteractionInterface : public UInterface
{
	GENERATED_BODY()
};

/** Interface class to hold C++ and Blueprint versions of intractable functions.
 * NOTE: Must implement the getter and setter method along with a local variable for the interactables FHandInterfaceSettings as it cannot be stored in the interface... */
class VRPROJECT_API IInteractionInterface
{
	GENERATED_BODY()

private:

	bool overlapping = false; /** Keeps track of overlapping or not overlapping with any hands. */
	class TArray<AVRHand*> overlappingHands; /** Keep track of what hands are currently overlapping and what hands have ended the overlap. */
	TArray<UActorComponent*> foundChildren; /** Find any added children in BP. */
	TArray<UPrimitiveComponent*> foundComponents; /** Add any changed depth components to this array so they can be reset later. */

public:

	/** Implementable Functions for this interface. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Hands)
	void Grabbed(AVRHand* hand);
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Hands)
	void Released(AVRHand* hand);
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Hands)
	void Squeezing(AVRHand* hand, float howHard);
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Hands)
	void Dragging(float deltaTime);
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Hands)
	void Interact(bool pressed);
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Hands)
	void Overlapping(AVRHand* hand);
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Hands)
	void EndOverlapping(AVRHand* hand);
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Hands)
	void Teleported();

 	/** Get and set functions to allow changes from blueprint. */
 	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Hands)		
	FInterfaceSettings GetInterfaceSettings();
 	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Hands)
 	void SetInterfaceSettings(FInterfaceSettings newInterfaceSettings);

	/** Setup functions for other classes overriding this interfaces functions */

	/** Ran when trigger is pressed all the way down. */
	virtual void Grabbed_Implementation(AVRHand* hand);

	/** Ran when the trigger is released. */
	virtual void Released_Implementation(AVRHand* hand);

	/** Ran when the controller is squeezed. */
	virtual void Squeezing_Implementation(AVRHand* hand, float howHard);

	/** Ticking function that is ran while an interactable is grabbed. */
	virtual void Dragging_Implementation(float deltaTime);

	/** Ran when the thumb button is pressed while something is being held. For example the trigger on the Valve index controller being pressed down. */
	virtual void Interact_Implementation(bool pressed);

	/** Ran on an interactable when the hand has selected it as the overlappingGrabbable to grab when grab is pressed.
	 * NOTE: Handles highlighting of interactables that are grabbable within the world. Be sure to call the super if overridden. */
	virtual void Overlapping_Implementation(AVRHand* hand);

	/** Ran on an interactable when the hand has selected it as the overlappingGrabbable to grab when grab is pressed is removed.
	 * NOTE: Handles un-highlighting of interactables that are grabbable within the world. Be sure to call the super if overridden. */
	virtual void EndOverlapping_Implementation(AVRHand* hand);

	/** Ran on an interactable when the hand is teleported. */
	virtual void Teleported_Implementation();

 	/** Get and set functions to allow changes to an interactable. */
	virtual FInterfaceSettings GetInterfaceSettings_Implementation();
 	virtual void SetInterfaceSettings_Implementation(FInterfaceSettings newInterfaceSettings);
};
