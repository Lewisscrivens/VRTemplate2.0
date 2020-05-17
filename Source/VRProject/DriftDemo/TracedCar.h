// Copyright 2020 Lewis Scrivens. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Globals.h"
#include "GameFramework/Actor.h"
#include "TracedCar.generated.h"

/** Declare log type for this class. */
DECLARE_LOG_CATEGORY_EXTERN(LogCar, Log, All);

/** Define used classes. */
class UStaticMeshComponent;
class USceneComponent;
class UStaticMesh;

/** Ray traced vehicle actor for performing simple arcade like car movement mechanics. */
UCLASS()
class VRPROJECT_API ATracedCar : public AActor
{
	GENERATED_BODY()

public:

	/** The car body mesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Car")
	UStaticMeshComponent* carBody;

	/** The location of a given wheel. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Car")
	USceneComponent* frontLeftWheel;

	/** The location of a given wheel. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Car")
	USceneComponent* frontRightWheel;

	/** The location of a given wheel. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Car")
	USceneComponent* backLeftWheel;

	/** The location of a given wheel. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Car")
	USceneComponent* backRightWheel;

	/** Front left wheel mesh spawned in on edit change of the wheelMesh variable. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Car")
	UStaticMeshComponent* frontLeftWheelMesh;

	/** Front right wheel mesh spawned in on edit change of the wheelMesh variable. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Car")
	UStaticMeshComponent* frontRightWheelMesh;

	/** Back left wheel mesh spawned in on edit change of the wheelMesh variable. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Car")
	UStaticMeshComponent* backLeftWheelMesh;

	/** Back right wheel mesh spawned in on edit change of the wheelMesh variable. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Car")
	UStaticMeshComponent* backRightWheelMesh;

	/** Array of wheels for looping. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Car")
	TArray<USceneComponent*> wheels;

	/** The location of a given wheel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car")
	UStaticMesh* wheelMesh;

	/** Force that can be added to reposition the car. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (UIMin = "0.0", ClampMin = "0.0"))
	float maxForce;

	/** The tracing distance for each wheel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (UIMin = "0.0", ClampMin = "0.0"))
	float traceDistance;

	/** The dampening to use on the cars body mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (UIMin = "0.0", ClampMin = "0.0"))
	float traceDampening;

	/** Wheel radius. Radius of sphere trace for the wheels as wheel mesh is just a visual representation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (UIMin = "0.0", ClampMin = "0.0"))
	float wheelRadius;

	/** Debug the traces. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car")
	bool traceDebug;

private:

	float steeringWheelAlpha; // The steering wheel alpha direction of how much to turn the car.
	float handbreakAlpha; // How much is the hand-break effecting the force at the back end of the car.
	bool handbreakEngaged; // Has the hand break been engaged.
	bool isGrounded; // Is the car touching the ground.
	FCollisionQueryParams wheelTraceParams; // The trace parameters for tracing a wheel collision.

	/** Update the physics forces being added from traced data in the world. */
	void UpdateCarTrace();

	/** Update rotational force over time to target a given rotation of the steering wheel. */
	void TargetRotation();

public:
	
	/** Constructor. */
	ATracedCar();

	/** Frame. */
	virtual void Tick(float DeltaTime) override;

	/** Set the direction of the steering wheel on the car. Values should be between -1 and 1 where -1 is fully locked left and 1 is fully locked right. */
	UFUNCTION(BlueprintCallable, Category = "Car")
	void SetSteeringWheelAlpha(float newAlpha);

	/** Update the amount the hand-break is pulled If 0.0f it will disengage the hand-break movement. */
	UFUNCTION(BlueprintCallable, Category = "Car")
	void SetHandbreakAlpha(float newAlpha);

	/** Update the cars movement in the forward vector at the given car speed. Negative will indicate the car is breaking force. */
	UFUNCTION(BlueprintCallable, Category = "Car")
	void UpdateCarMovement(float currentSpeed);

protected:

#if WITH_EDITOR
	/** Post edit change. */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	
	/** Level start. */
	virtual void BeginPlay() override;
};
