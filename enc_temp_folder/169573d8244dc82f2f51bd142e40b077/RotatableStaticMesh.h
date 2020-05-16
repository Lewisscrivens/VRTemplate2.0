// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "Player/InteractionInterface.h"
#include "Project/VRFunctionLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Globals.h"
#include "RotatableStaticMesh.generated.h"

/** Define this components log category. */
DECLARE_LOG_CATEGORY_EXTERN(LogRotatableMesh, Log, All);

/** Locking delegate. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRotatableLocked, float, angle);

/** Declare classes used. */
class AVRHand;
class USceneComponent;
class UCurveFloat;
class USimpleTimeline;

/** The rotation mode of how the rotatable will be tracked. */
UENUM(BlueprintType)
enum class ERotationMode : uint8
{
	Default UMETA(DisplayName = "Default", ToolTip = "Rotation follows original grabbed position."),
	Twist UMETA(DisplayName = "TwistRotation", ToolTip = "Rotation follows twisting mostion of original grabbed position."),
};

/** The grabbing method that the rotatable will be grabbed. */
UENUM(BlueprintType)
enum class EGrabMode : uint8
{
	Static UMETA(DisplayName = "GrabStatic", ToolTip = "Rotation will be updated via the controllers offset from original grabbed rotation."),
	Physics UMETA(DisplayName = "GrabPhysics", ToolTip = "Rotation will be updated via the hands grab vr physics handle. NOTE: Collisions with physics objects will be taken into account this way."),
};

/** Axis to rotate around. */
UENUM(BlueprintType)
enum class ERotateAxis : uint8
{
	Pitch UMETA(DisplayName = "Pitch", ToolTip = "Rotate around the relative Pitch axis."),
	Yaw UMETA(DisplayName = "Yaw", ToolTip = "Rotate around the relative Yaw axis."),
	Roll UMETA(DisplayName = "Roll", ToolTip = "Rotate around the relative Roll axis.")
};

/** Different constrained positions, used to visualize what state the constraint is in. */
UENUM(BlueprintType)
enum class EConstraintState : uint8
{
	Bellow180,
	Start,
	Middle,
	End
};

/** A static mesh that can be grabbed and rotated statically based of controller position or via the hands grab handle. 
  * NOTE: Two handed grabbing is only implemented for the physics based grabbing mode for now... */
UCLASS(ClassGroup = (Custom), Blueprintable, BlueprintType, PerObjectConfig, EditInlineNew)
class VRPROJECT_API URotatableStaticMesh : public UStaticMeshComponent, public IInteractionInterface
{
	GENERATED_BODY()

public:

	/** Reference to the hand currently grabbing this component. Also can be used as a bool to check if this is in a hand. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Rotatable")
		AVRHand* handRef;

	/** What rotation mode is this rotatable static mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Rotation")
		ERotationMode rotateMode;

	/** What grab mode will be used to interact with this rotatable mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Rotation")
		EGrabMode grabMode;

	/** Rotate around the selected axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Rotation")
		ERotateAxis rotateAxis;

	/** The constraint will attempt to fake physics using hand release velocity and faked restitution/friction values. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Rotation")
		bool fakePhysics;

	/** Center the rotational limit to plus and minus "rotationLimit / 2" in either direction. Default is false. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Rotation")
		bool centerRotationLimit;

	/** Use the maxOverRotation float to determine when to release from the hand... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rotatable|Rotation")
		bool releaseOnOverRotation;

	/** Print any relevant debugging messages for this class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Rotation")
		bool debug;

	/** The fakedPhysics restitution when hitting objects. How much velocity to keep when bouncing off the walls of the constraint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Rotation", meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"))
		float restitution;

	/** The fakedPhysics damping variable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Rotation", meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "0.2", UIMax = "0.2"))
		float friction;

	/** The max rotation limit. NOTE: 0 means its free to rotate to any given limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Rotation")
		float rotationLimit;

	/** The start rotation of the rotatable. Updates in real time within editor when changed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Rotation")
		float startRotation;

	/** Max angle you can go past the constraint before it is released from the hand. Used to prevent unusual functionality. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rotatable|Rotation")
		float maxOverRotation;

	/** Curve to drive the timeline interpolation when SetRotatableRotation is ran with the interpolation flag set to true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Rotation")
		UCurveFloat* rotationUpdateCurve;

	/** Current amount of rotation done by this rotatable static mesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rotatable|CurrentValues")
		float cumulativeAngle;

	/** Alpha to present how much the rotatable has been rotated. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rotatable|CurrentValues")
		float rotationAlpha;

	/** Current amount of revolutions calculated from the cumulative angle. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rotatable|CurrentValues")
		int revolutionCount;

	/** Play rotating haptic effect if not null every hapticRotationDelay degrees. NOTE: If set to 0.0f it will play the haptic effect constantly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Effects", meta = (UIMin = "0.1", ClampMin = "0.1"))
		float hapticRotationDelay;

	/** Haptic effect to play on hand when grabbed and rotating this component every hapticRotationDelay. NOTE: Will not play haptic effect if this in null. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Effects")
		UHapticFeedbackEffect_Base* rotatingHapticEffect;

	/** Haptic effect to play on hand when locking while grabbed. NOTE: Will not play haptic effect if this in null. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Effects", meta = (EditCondition = "lockable"))
		UHapticFeedbackEffect_Base* lockHapticEffect;

	/** Sound to play when rotatable hits its constraint limits. NOTE: Default collision haptic feedback is used from the AVRHand grabbing this component. Sound will not play if null. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Effects")
		USoundBase* impactSound;

	/** Sound to play when locking this rotatable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Effects", meta = (EditCondition = "lockable"))
		USoundBase* lockSound;

	/** Enable the rotatable to lock into certain positions. NOTE: On lock is a delegate that indicates which point has been locked to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Locking")
		bool lockable;

	/** The rotation will only update on locking angle. NOTE: Use-full for knobs for selection etc. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Locking", meta = (EditCondition = "lockable"))
		bool lockOnlyUpdate;

	/** Is currently locked. If enabled on begin play the rotatable will be locked in its start rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Locking", meta = (EditCondition = "lockable"))
		bool locked;

	/** Can lock while grabbed? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Locking", meta = (EditCondition = "lockable"))
		bool lockWhileGrabbed;

	/** Grab while locked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Locking", meta = (EditCondition = "lockable"))
		bool grabWhileLocked;

	/** Print debug messages about locking to the log. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Locking", meta = (EditCondition = "lockable"))
		bool debugLocking;

	/** Release from the hand when locked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Locking", meta = (EditCondition = "lockable"))
		bool releaseWhenLocked;

	/** How close to a locking point before locking into rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Locking", meta = (EditCondition = "lockable", UIMin = "0.0", ClampMin = "0.0"))
		float lockingDistance;

	/** How far in the rotateAxis after an unlock before this rotatable can be locked again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Locking", meta = (EditCondition = "lockable", UIMin = "0.0", ClampMin = "0.0"))
		float unlockingDistance;

	/** Rotatable locking points. NOTE: If you want the constraint to lock at (x) degrees add a float to the array set to (x)f. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Locking", meta = (EditCondition = "lockable"))
		TArray<float> lockingPoints;

	/** The interface settings for hand interaction with this interactable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Rotation")
		FInterfaceSettings interactableSettings;

	/** Scene component reference. NOTE: Spawned in when grabbed to keep track of grabbed position/rotation. */
	UPROPERTY(BlueprintReadOnly, Category = "Rotatable")
		USceneComponent* grabScene; 

	/** Simple time line class to be used when SetRotatableRotation is ran with the interpolation flag if there is a curve set. */
	UPROPERTY(BlueprintReadOnly, Category = "Rotatable")
		USimpleTimeline* rotationTimeline;

	/** Physics constraint for physics mode of this rotatable static mesh spawned in on begin play.  */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
		UPhysicsConstraintComponent* pivot;

	//////////////////////////
	//	  Grab delegates    //
	//////////////////////////

	/** Mesh grabbed by hand. */
	UPROPERTY(BlueprintAssignable)
	FInteraction OnRotatableGrabbed;

	/** Mesh released from hand. */
	UPROPERTY(BlueprintAssignable)
	FInteraction OnRotatableReleased;

	/** Locking delegate. Called when locked function is ran. */
	UPROPERTY(BlueprintAssignable)
	FRotatableLocked OnRotatableLock;

private:

	bool flipped;/** Is the rotation range negative, changes the maths used in certain areas of this class. */
	bool firstRun;/** Boolean to change last angle on angle change to current angle. (So there is no angle change calculated on grabbing...) */
	bool cannotLock; /** Disables locking after grabbed when locked in position so it doesn't instantly release from the hand and lock again. */
	bool isLimited; /** Is limited to a range. */
	bool interpolating; /** Is the timer currently running for setting this rotatableMeshes rotation? */
	bool lockOnTimelineEnd; /** Should interpolating timeline lock when its finished? */
	bool impactSoundEnabled; /** Is the impact sound currently allowed to be played in UpdateAudioAndHaptics? */

	float lastAngle;/** last frames angle. */
	float actualCumulativeAngle;/** un-clamped cumulative angle. (The amount the hand has cumulatively rotated) */
	float currentAngle;/** Current frames angle updated from the UpdateGrabbedRotation function. */
	float currentAngleChange, angleChangeOnRelease; /** Difference in rotation between frames in the axis. */
	float angularVelocity; /** Current angular velocity of this rotatable. */
	float lastUnlockAngle, lastCheckedRotation; /** The last unlocked angle to check against when grabbed. */
	float currentRotationLimit;/** Absolute rotationLimit. (Positive rotationLimit) */
	float currentLockedRotation;/** The current locked rotation of the rotatable if it is locked. */
	float timelineStartRotation, timelineEndRotation; /** Interpolation/Lerp variables for updating rotation over time after SetRotatableRotation is ran with interpolation set to true. */
	float lastHapticFeedbackRotation; /** Last haptic feedback rotation so it's played every hapticRotationDelay correctly... */

	EConstraintState constrainedState; /** Current state of the constraint, this is used to keep track of how the constrained rotatable should act at different rotations. */
	FRotator originalRelativeRotation;/** Save the original rotation of the rotatable mesh used to add or subtract cumulative rotation from. */
	FVector handStartLocation, twistingHandOffset; /** Save the start locations to help calculate offsets. */
	FRotator meshStartRelative; /** Relative rotation of this component around the constrained axis. */

protected:

	/** Level start. */
	virtual void BeginPlay() override;

	/** Return the original relative rotating angle. */
	float GetOriginalRelativeAngle();

	/** Create and return new relative angle. */
	FRotator GetNewRelativeAngle(float newAngle);

#if WITH_EDITOR
	/** Post edit change. */
	virtual void PostEditChangeProperty(FPropertyChangedEvent & PropertyChangedEvent) override;
#endif

	/** Spawn in and setup a physics constraint for this mesh to its parent with the given physics options for the constraint. */
	void CreatePhysicsConstraint();

	/** Update the constraint mode from the current cumulative angle. */
	void UpdateConstraintMode();

	/** Change the constraints current state, used to allow cumulative rotations while using the physics constraint. (As it is limited to 360 and has many other issues.)
	 * @Param state, state of constraint (ENUM) to swap to.	*/
	void UpdateConstraint(EConstraintState state);

	/** Update the constraints reference position for the selected axis. NOTE: Should be half of the currentLimit.
	 * @Param constraintAngle, the angle in the yaw-axis of the mesh that the constraints reference should be set to. */
	void UpdateConstraintRefference(float constraintAngle);

	/** Update the current angle in this components rotateAxis. */
	void UpdateRotation(float DeltaTime = 0.0f);

	/** If grabbed this function will update the grabbed angle (currentAngle) in the rotateAxis using original grab offsets and trigonometry. */
	void UpdateGrabbedRotation();

	/** Updates the rotational values used in update rotation for keeping rotation offset from hand grabbed location */
	void UpdateRotatable(float DeltaTime);

	/** Updates and clamps both the cumulative angle and revolution count.
	 * @Param increaseAmount, the amount the cumulative angle has increased/decreased. */
	void IncreaseCumulativeAngle(float increaseAmount);

	/** Apply physical rotation from last force of the hand on release, Handled in tick....
	 * Also handles restitution values. */
	void UpdatePhysicalRotation(float DeltaTime);

	/** Check if the current cumulative angle is close enough to a locking position, if so lock the rotatable. */
	void UpdateRotatableLock();

	/** Spawns a scene component to keep track of certain rotations/locations relative to the hand and this component.
	 * @Param connection, the primitive component to connect the newly spawned scene component to.
	 * @Param worldLocation, the world location to spawn and attach the scene component at... */
	void CreateSceneComp(USceneComponent* connection, FVector worldLocation);

	/** Return the correct transform to use as this components parent.... */
	FTransform GetParentTransform();

	/** Update the hands release distance variables. */
	void UpdateHandGrabDistance();

	/** Update the rotatable audio events and haptic feedback events if grabbed while rotating or impacting the constraint bounds. */
	void UpdateAudioAndHaptics();

public:

	/** Constructor. */
	URotatableStaticMesh();

	/** Updates the current rotational values and the physical rotation that uses those values. */
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	/** Is the given value within a range between min and max. */
	bool InRange(float Value, float Min, float Max, bool InclusiveMin = true, bool InclusiveMax = true);

	/** Lock this rotatable static mesh at the specified locking angle. */
	UFUNCTION(BlueprintCallable, Category = "Rotatable")
	void Lock(float lockingAngle);

	/** Unlock this rotatable static mesh. */
	UFUNCTION(BlueprintCallable, Category = "Rotatable")
	void Unlock();

	/** Set the current angle of this rotatable, depending on variables interpolate and lock to the set rotation.
	  * @Param angle, The angle to update this rotatable to. 
	  * @Param lockAtAngle, Lock at the given angle updating to.
	  * @Param interpolate, Should interpolate using the timeline curve. */
	UFUNCTION(BlueprintCallable, Category = "Rotatable")
	void SetRotatableRotation(float angle, bool lockAtAngle = false, bool interpolate = false);

	/** Called from simple timer used in the set rotatable rotation function on update of the timeline.
	  * @Param val, Current timeline progress. */
	UFUNCTION(Category = "Rotatable")
	void UpdateRotatableRotation(float val);

	/** Called from simple timer used in the set rotatable rotation function at the end of the timeline. */
	UFUNCTION(Category = "Rotatable")
	void EndRotatableRotation();

	/** Implementation of the hands interface. */
	virtual void Grabbed_Implementation(AVRHand* hand) override;
	virtual void Released_Implementation(AVRHand* hand) override;
	virtual void Dragging_Implementation(float deltaTime) override;
	virtual void Overlapping_Implementation(AVRHand* hand) override;
	virtual void EndOverlapping_Implementation(AVRHand* hand) override;

	/**  Get and set functions to allow changes from blueprint. */
	virtual FInterfaceSettings GetInterfaceSettings_Implementation() override;
	virtual void SetInterfaceSettings_Implementation(FInterfaceSettings newInterfaceSettings) override;
};