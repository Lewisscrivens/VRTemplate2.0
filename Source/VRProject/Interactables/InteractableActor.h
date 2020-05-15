// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/InteractionInterface.h"
#include "Globals.h"
#include "InteractableActor.generated.h"

/** Define this actors log category. */
DECLARE_LOG_CATEGORY_EXTERN(LogInteractable, Log, All);

/** Declare classes used. */
class AVRHand;

/** Class with implemented interface ready for blueprint use, Needed due to the IHandsInterface class being null when called
 *  from a BlueprintActor with said interface implemented. */
UCLASS()
class VRPROJECT_API AInteractableActor : public AActor, public IInteractionInterface
{
	GENERATED_BODY()

public:

	/** Enable the debugging message for printing this interactables current settings every frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interatable")
	bool debugSettings;

	/** The interfaces interactable settings for how to interact with VR controllers/hands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interatable")
	FInterfaceSettings interactableSettings;

protected:

	/** Level Start. */
	virtual void BeginPlay() override;

public:

	/** Constructor. */
	AInteractableActor();

	/** Frame. */
	virtual void Tick(float DeltaTime) override;

	/** Blueprint interface functions to be override in the BP....
	 * NOTE: This is needed due to the interface being C++ it doesn't work correctly in BP... */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = Blueprint)
	void GrabbedBP(AVRHand* hand);
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = Blueprint)
	void ReleasedBP(AVRHand* hand);
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = Blueprint)
	void DraggingBP(float deltaTime);
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = Blueprint)
	void SqueezingBP(AVRHand* hand, float howHard);
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = Blueprint)
	void InteractBP(bool pressed);
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = Blueprint)
	void OverlappingBP(AVRHand* hand);
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = Blueprint)
	void EndOverlappingBP(AVRHand* hand);
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = Blueprint)
	void TeleportedBP();

	/** Implementation of the interfaces functions... */
	virtual void Grabbed_Implementation(AVRHand* hand) override;
	virtual void Released_Implementation(AVRHand* hand) override;
	virtual void Squeezing_Implementation(AVRHand* hand, float howHard) override;
	virtual void Dragging_Implementation(float deltaTime) override;
	virtual void Interact_Implementation(bool pressed) override;
	virtual void Overlapping_Implementation(AVRHand* hand) override;
	virtual void EndOverlapping_Implementation(AVRHand* hand) override;
	virtual void Teleported_Implementation() override;

	/**  Get and set functions to allow changes from blueprint. */
	virtual FInterfaceSettings GetInterfaceSettings_Implementation() override;
	virtual void SetInterfaceSettings_Implementation(FInterfaceSettings newInterfaceSettings) override;
};
