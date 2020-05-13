// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "Player/InteractionInterface.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "IIdentifiableXRDevice.h"
#include "Globals.h"
#include "VRPlayer.generated.h"

/** Declare log type for the player pawn class. */
DECLARE_LOG_CATEGORY_EXTERN(LogVRPlayer, Log, All);

/** Declare classes used. */
class UFloatingPawnMovement;
class UCapsuleComponent;
class USceneComponent;
class UCameraComponent;
class USphereComponent;
class AVRMovement;
class UStaticMeshComponent;
class AVRHand;
class UInputComponent;
class UMaterial;
class UEffectsContainer;

/** Post update ticking function integration. 
 *  NOTE: Important for checking the tracking state of the HMD and hands. */
USTRUCT()
struct FPostUpdateTick : public FActorTickFunction
{
	GENERATED_BODY()

	/** Target actor. */
	class AVRPlayer* Target;

	/** Declaration of the new ticking function for this class. */
	virtual void ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
};
template <>
struct TStructOpsTypeTraits<FPostUpdateTick> : public TStructOpsTypeTraitsBase2<FPostUpdateTick>
{
	enum { WithCopy = false };
};

/** VR Pawn system that connected the VRMovement component and VRHands, manages input across the hands and the movement class. */
UCLASS()
class VRPROJECT_API AVRPlayer : public APawn
{
	GENERATED_BODY()

public:

	/** Movement component for the developer mode and certain types of vr movement. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category = "Pawn")
	UFloatingPawnMovement* floatingMovement;

	/** Movement component for the developer mode and certain types of vr movement. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category = "Pawn")
	UCapsuleComponent* movementCapsule;

	/** Location of the floor relative to the headset. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category = "Pawn")
	USceneComponent* scene;

	/** Player camera to map HMD location, rotation etc. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category = "Pawn")
	UCameraComponent* camera;

	/** Head colliders. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Pawn")
	USphereComponent* headCollider;
	
	/** Vignette for movement peripheral vision damping. Helps with motion sickness, enable this in the movement component VR. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category = "Pawn")
	UStaticMeshComponent* vignette;

	/** Left hand class pointer. */
	UPROPERTY(BlueprintReadOnly, Category = "Pawn")
	AVRHand* leftHand;

	/** Right hand class pointer. */
	UPROPERTY(BlueprintReadOnly, Category = "Pawn")
	AVRHand* rightHand;

	/** Blueprint template class to spawn the movement component from. */
	UPROPERTY(EditDefaultsOnly, Category = "Pawn")
	TSubclassOf<AVRMovement> movementClass;

	/** The template class/BP for the left hand. */
	UPROPERTY(EditDefaultsOnly, Category = "Pawn")
	TSubclassOf<AVRHand> leftHandClass;

	/** The template class/BP for the right hand. */
	UPROPERTY(EditDefaultsOnly, Category = "Pawn")
	TSubclassOf<AVRHand> rightHandClass;

	/** Container component to add feedback and audio references that are obtainable by name in code. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pawn")
	UEffectsContainer* pawnEffects;

	/** VR Physics handle component to handle the head collider. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite)
	UVRPhysicsHandleComponent* headHandle;

	/** Enable any debug messages for this class. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pawn")
	bool debug;

	/** Component that holds all the movement functionality. Created and spawned from template (vrMovementTemplate) in begin play. */
	UPROPERTY(BlueprintReadWrite, Category = "Pawn")
	AVRMovement* movement;

	FPostUpdateTick postTick; /** Post ticking declaration. */
	TArray<AActor*> actorsToIgnore; /** Ignored actors for this class. */

	bool foundHMD; /** Found and tracking the HMD. */
	bool tracked; /** Has the headset ever been tracked. Used to move the player to the start location on begin play and first tracked event for the HMD. */
	bool devModeActive; /** Local bool to check if dev mode is enabled. */
	bool movementLocked; /** Can the player initiate movement. */
	bool thumbL, thumbR; /** Functions to prevent teleporting being disabled by trigger val == 0. */

private:

	bool collisionEnabled; /** This classes components are blocking physics simulated components. */
	FTimerHandle headColDelay; /** Timer handle for the collision delay when the collision will be re-enabled on the head Collider. Also some timer handles for the hands. */
	FXRDeviceId hmdDevice; /** Device ID for the current HMD device that is being used. */
	AVRHand* movingHand; /** The hand that is currently initiating movement for the VRPawn. */

protected:

	/** Ran before begin play. */
	virtual void PostInitializeComponents() override;

	/** Level start. */
	virtual void BeginPlay() override;

	/** Setup pawn input. */
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** Disable/Enable collisions on the whole pawn including hands individually from each other based on current tracking status....
	 * NOTE: When this is not enabled, when the device loses tracking and repositions itself when found again the sweep will cause physic
	 *		 actors in the scene to be affected by the force of movement... 
	 * NOTE: Could also use SetWorldLocation/Rotation no physics functions but I have no control over the object being spawned and positioned
	 *	     on begin play. */
	void UpdateHardwareTrackingState();

public:

	/** Constructor. */
	AVRPlayer();

	/** Frame. */
	virtual void Tick(float DeltaTime) override;

	/** Late Frame. */
	void PostUpdateTick(float DeltaTime);

	/** Teleported function to handle any events on teleport. */
	void Teleported();

	/** Move/Set new world location and rotation of where the player is stood and facing.
	 * @Param newLocation, The new location in world-space to teleport the player to.
	 * @Param newRotation, The new rotation in world-space to teleport the player facing. */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	void MovePlayerWithRotation(FVector newLocation, FRotator newFacingRotation);

	/** Move/Set new world location of where the player is stood.
	 * @Param newLocation, The new location in world-space to teleport the player to. */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	void MovePlayer(FVector newLocation);

	/** @Return collisionEnabled. */
	UFUNCTION(BlueprintCallable, Category = "Pawn|Collision")
	bool GetCollisionEnabled();

	/** Used to quickly activate/deactivate all collision on the player. */
	UFUNCTION(BlueprintCallable, Category = "Pawn|Collision")
	void ActivateAllCollision(bool enable);

	/** Used to reset player collision after a set world location event to prevent physics objects getting stuck under or inside other game world objects. */
	UFUNCTION(BlueprintCallable, Category = "Pawn|Collision")
	void ResetCollision();

	/** Used to activate/deactivate all collision in the pawn class. */
	UFUNCTION(BlueprintCallable, Category = "Pawn|Collision")
	void ActivateCollision(bool enable);

	/** Looping function every 0.1 seconds checking if the head Collider is currently overlapping physics etc. If not re-enable collision, until then loop and check. */
	UFUNCTION(Category = "Pawn|Collision")
	void CollisionDelay();

	/** Get the effects container from the pawn. So hands and other interactables can obtain default effects for rumbling or audio feedback. */
	UFUNCTION(BlueprintCallable, Category = "Pawn|Collision")
	UEffectsContainer* GetPawnEffects();

	/////////////////////////////////////////////////////
	/**					Input events.                  */
	/////////////////////////////////////////////////////

	UFUNCTION(Category = "Pawn|Input")
	void TriggerLeftPressed();

	UFUNCTION(Category = "Pawn|Input")
	void TriggerLeftReleased();

	UFUNCTION(Category = "Pawn|Input")
	void TriggerRightPressed();

	UFUNCTION(Category = "Pawn|Input")
	void TriggerRightReleased();

	UFUNCTION(Category = "Pawn|Input")
	void ThumbLeftPressed();

	UFUNCTION(Category = "Pawn|Input")
	void ThumbLeftReleased();

	UFUNCTION(Category = "Pawn|Input")
	void ThumbRightPressed();

	UFUNCTION(Category = "Pawn|Input")
	void ThumbRightReleased();

	UFUNCTION(Category = "Pawn|Input")
	void SqueezeL(float val);

	UFUNCTION(Category = "Pawn|Input")
	void SqueezeR(float val);

	UFUNCTION(Category = "Pawn|Input")
	void ThumbstickLeftX(float val);

	UFUNCTION(Category = "Pawn|Input")
	void ThumbstickLeftY(float val);

	UFUNCTION(Category = "Pawn|Input")
	void ThumbstickRightX(float val);

	UFUNCTION(Category = "Pawn|Input")
	void ThumbstickRightY(float val);

	UFUNCTION(Category = "Pawn|Input")
	void TriggerLeftAxis(float val);

	UFUNCTION(Category = "Pawn|Input")
	void TriggerRightAxis(float val);
};