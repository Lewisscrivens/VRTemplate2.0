// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "Globals.h"
#include "GameFramework/Actor.h"
#include "Player/InteractionInterface.h"
#include "GrabbableActor.generated.h"

/** Define this actors log category. */
DECLARE_LOG_CATEGORY_EXTERN(LogGrabbable, Log, All);

/** Declare classes used. */
class UStaticMeshComponent;
class UVRPhysicsHandleComponent;
class AVRHand;
class USoundBase;
class UAudioComponent;
class UHapticFeedbackEffect_Base;

/** Struct to hold a hands initial and current grabbing information/variables. */
USTRUCT(BlueprintType)
struct FGrabInformation
{
	GENERATED_BODY()

public:

	/** Storage variable for the hand when grabbing a grabbableActor. */
	UPROPERTY(BlueprintReadOnly, Category = "Grabbable")
	AVRHand* handRef;

	/** Component used to target location/rotation while grabbed. */
	UPROPERTY(BlueprintReadOnly, Category = "Grabbable")
	UPrimitiveComponent* targetComponent; 

	/** Default constructor. */
	FGrabInformation()
	{
		Reset();
	}

	/** Reset this structures variables. */
	void Reset()
	{
		handRef = nullptr;
		targetComponent = nullptr;
	}
};

/** Make a static mesh actor grabbable using this class.
 *  NOTE: IMPORTANT to make sure that physics material override is left to empty, if this is needed use the physics material in the material of the grabbable mesh!!! */
UCLASS()
class VRPROJECT_API AGrabbableActor : public AActor, public IInteractionInterface
{
	GENERATED_BODY()

public:

	/** Grabbable mesh root component. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite)
	UStaticMeshComponent* grabbableMesh;

	/** Component to play audio when this grabbable impacts other objects on hit. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite)
	UAudioComponent* grabbableAudio;

	/** Struct to hold information on the hand grabbing this component like grab offsets, handRefference, target locations etc. */
	UPROPERTY(BlueprintReadOnly, Category = "Grabbable")
	FGrabInformation grabInfo;

	/** Struct to hold a second hands information if two handed grabbing is enabled on this actor. */
	UPROPERTY(BlueprintReadOnly, Category = "Grabbable")
	FGrabInformation otherGrabInfo;

	/** Physics material while grabbed enabled or disabled. NOTE: Replaces physics material override back to null after release. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable|Physics")
	bool grabbedPhysicsMaterial;

	/** Friction material to use while grabbed to prevent grabbable edge colliders catching on flat surfaces. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable|Physics", meta = (EditCondition = "grabbedPhysicsMaterial"))
	UPhysicalMaterial* physicsMaterialWhileGrabbed;

	/** The haptic feedback intensity multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable|Haptic Effects")
	float hapticIntensityMultiplier;

	/** The haptic feedback collision effect to play override. If null it will use the default collision feedback in the AVRHands grabbing this grabbable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable|Haptic Effects")
	UHapticFeedbackEffect_Base* collisionFeedbackOverride;

	/** Sound to play on collision if null it will use the AVRHands default collision sound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable|Haptic Effects")
	USoundBase* impactSoundOverride;

	/** Snap the grabbed object to the current hand location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
	bool snapToHand;

	/** Snap the grabbed object to the current second hand location. NOTE: Only used if two handed grabbing is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
	bool snapToSecondHand;

	/** Used for snap to hand so the rotation for each object can be adjusted to an offset.
	  * NOTE: This is the offset for the first hand. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable", meta = (EditCondition = "snapToHand"))
	FRotator snapToHandRotationOffset;

	/** Used for snap to hand so the location for each object can be adjusted to an offset. 
	  * NOTE: This is the offset for the first hand. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable", meta = (EditCondition = "snapToHand"))
	FVector snapToHandLocationOffset;

	/** Consider the weight of the object when throwing it by decreasing velocity based on mass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable|Physics")
	bool considerMassWhenThrown;

	/** Can be used to change the mass on grab to prevent it effecting the physics handles functionality.
	 * NOTE: Changes mass of this grabbable when grabbed to massWhenGrabbed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable|Physics")
	bool changeMassOnGrab;

	/** Mass to use on the grabbableMesh while grabbed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable|Physics", meta = (EditCondition = "changeMassOnGrab"))
	float massWhenGrabbed;

	/** The current frames velocity of  the grabbable. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grabbable|Physics")
	float currentFrameVelocity;

	/** The current frames velocity change when compared to the last frame. NOTE: Useful for calculating impact force etc. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grabbable|Physics")
	float currentVelocityChange;

	/** Show debug information for grabbing this grabbable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
	bool debug;

	/** Cancel the grabbing of this component. */
	UPROPERTY(BlueprintReadWrite, Category = "Grabbable")
	bool cancelGrab;

	/** This interactables interface settings for being handled in the VRHand class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
	FInterfaceSettings interactableSettings;

	//////////////////////////
	//	     Pointers       //
	//////////////////////////

	/** Holds ignored actors for when performing traces in the collision check functions. */
	UPROPERTY(BlueprintReadOnly, Category = "Grabbable")
	TArray<AActor*> ignoredActors;

	/** The original physics material of this grabbable before grabbed. */
	UPROPERTY(BlueprintReadOnly, Category = "Grabbable")
	UPhysicalMaterial* originalPhysicalMAT;

	/** Stored impact sound pointer. */
	UPROPERTY(BlueprintReadOnly, Category = "Grabbable")
	USoundBase* impactSound;

	/** Stored collision haptic feedback pointer. */
	UPROPERTY(BlueprintReadOnly, Category = "Grabbable")
	UHapticFeedbackEffect_Base* collisionFeedback;

	//////////////////////////
	//	  Grab delegates    //
	//////////////////////////

	/** Mesh grabbed by hand. */
	UPROPERTY(BlueprintAssignable)
	FInteraction OnMeshGrabbed;

	/** Mesh released from hand. */
	UPROPERTY(BlueprintAssignable)
	FInteraction OnMeshReleased;

private:
	 
	FTimerHandle lastRumbleHandle;/** Timer handle to allow another rumble within this class when over. */
	float lastImpactSoundTime, lastRumbleIntensity;
	float lastFrameVelocity; /** Last frames velocity to help calculate velocity change over time. */
	float lastHandGrabDistance; /** distance the hand was away from this actor last frame.  */
	float lastZ;

protected:

	/** Level Start */
	virtual void BeginPlay() override;

private:

	/** Binded event to this actors hit response delegate. */
	UFUNCTION(Category = "Collision")
	void OnHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit);

	/** Event to reset the rumble intensity last set on hit. */
	UFUNCTION(Category = "Collision")
	void ResetLastRumbleIntensity();	

public:

	/** Constructor. */
	AGrabbableActor();
	
	/** Frame. */
	virtual void Tick(float DeltaTime) override;

	/** Physics handle grabbing/releasing functions. */
	void PickupPhysicsHandle(FGrabInformation info, bool secondHand);
	void DropPhysicsHandle(FGrabInformation info);

	/** Check if the actor is grabbed. */
	UFUNCTION(BlueprintPure, Category = "Grabbable")
	bool IsActorGrabbed();

	/** Check if the actor is grabbed by two hands. */
	UFUNCTION(BlueprintPure, Category = "Grabbable")
	bool IsActorGrabbedWithTwoHands();

	/** Implementation of the required interface functions. */
	virtual void Grabbed_Implementation(AVRHand* hand) override;
	virtual void Released_Implementation(AVRHand* hand) override;
	virtual void Dragging_Implementation(float deltaTime) override;
	virtual void Overlapping_Implementation(AVRHand* hand) override;
	virtual void EndOverlapping_Implementation(AVRHand* hand) override;
	virtual void Teleported_Implementation() override;

	/**  Get and set functions to allow changes from blueprint. */
	virtual FInterfaceSettings GetInterfaceSettings_Implementation() override;
	virtual void SetInterfaceSettings_Implementation(FInterfaceSettings newInterfaceSettings) override;
};
