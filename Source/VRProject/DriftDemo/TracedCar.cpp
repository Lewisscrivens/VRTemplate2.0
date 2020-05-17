// Copyright 2020 Lewis Scrivens. All Rights Reserved.

#include "TracedCar.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY(LogCar);

ATracedCar::ATracedCar()
{
	PrimaryActorTick.bCanEverTick = true;

	// Setup default car body mesh.
	carBody = CreateDefaultSubobject<UStaticMeshComponent>("CarBody");
	carBody->SetCollisionProfileName("PhysicsBody");
	carBody->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	RootComponent = carBody;

	// Setup wheel locations.
	frontLeftWheel = CreateDefaultSubobject<USceneComponent>("FrontLeftWheel");
	frontLeftWheel->SetupAttachment(carBody);
	frontLeftWheel->SetRelativeLocation(FVector(20.0f, -10.0f, 0.0f));
	frontRightWheel = CreateDefaultSubobject<USceneComponent>("FrontRightWheel");
	frontRightWheel->SetupAttachment(carBody);
	frontRightWheel->SetRelativeLocation(FVector(20.0f, 10.0f, 0.0f));
	backLeftWheel = CreateDefaultSubobject<USceneComponent>("BackLeftWheel");
	backLeftWheel->SetupAttachment(carBody);
	backLeftWheel->SetRelativeLocation(FVector(-20.0f, -10.0f, 0.0f));
	backRightWheel = CreateDefaultSubobject<USceneComponent>("BackRightWheel");
	backRightWheel->SetupAttachment(carBody);
	backRightWheel->SetRelativeLocation(FVector(-20.0f, 10.0f, 0.0f));

	// Spawn/Setup front left wheel mesh.
	frontLeftWheelMesh = CreateDefaultSubobject<UStaticMeshComponent>("FrontLeftWheelMesh");
	frontLeftWheelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	frontLeftWheelMesh->SetupAttachment(frontLeftWheel);

	// Spawn/Setup front right wheel mesh.
	frontRightWheelMesh = CreateDefaultSubobject<UStaticMeshComponent>("FrontRightWheelMesh");
	frontRightWheelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	frontRightWheelMesh->SetupAttachment(frontRightWheel);

	// Spawn/Setup back left wheel mesh.
	backLeftWheelMesh = CreateDefaultSubobject<UStaticMeshComponent>("BackLeftWheelMesh");
	backLeftWheelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	backLeftWheelMesh->SetupAttachment(backLeftWheel);

	// Spawn/Setup back right wheel mesh.
	backRightWheelMesh = CreateDefaultSubobject<UStaticMeshComponent>("BackRightWheelMesh");
	backRightWheelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	backRightWheelMesh->SetupAttachment(backRightWheel);

	// Setup default traced car variables.
	maxForce = 40000.0f;
	steeringWheelAlpha = 0.0f;
	traceDebug = false;
	handbreakEngaged = false;
	isGrounded = false;
	traceDistance = 40.0f;
	traceDampening = 1.0f;
	wheelMesh = nullptr;
}

#if WITH_EDITOR
void ATracedCar::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Get the name of the property that was changed.
	FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	// If the wheel mesh has been inputted spawn in the wheels and attach them to the car body, they don't have collision.
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ATracedCar, wheelMesh))
	{
		// Set wheel mesh.
		if (wheelMesh)
		{			
			frontLeftWheelMesh->SetStaticMesh(wheelMesh);		
			frontRightWheelMesh->SetStaticMesh(wheelMesh);	
			backLeftWheelMesh->SetStaticMesh(wheelMesh);
			backRightWheelMesh->SetStaticMesh(wheelMesh);
			return;
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void ATracedCar::BeginPlay()
{
	Super::BeginPlay();
	
	// Add wheel locations to an array.
	wheels.Add(frontLeftWheel);
	wheels.Add(frontRightWheel);
	wheels.Add(backLeftWheel);
	wheels.Add(backRightWheel);
	wheelTraceParams.AddIgnoredActor(this);

	// Ensure car body is simulating physics.
	carBody->SetSimulatePhysics(true);
	carBody->SetMassOverrideInKg(NAME_None, 28.0f);
}

void ATracedCar::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Start updating the car trace each frame.
	UpdateCarTrace();
	TargetRotation();
}

void ATracedCar::UpdateCarTrace()
{
	// For each wheel location in the wheels array.
	for (USceneComponent* wheelLoc : wheels)
	{
		FHitResult currentHit;
		FVector startLoc = wheelLoc->GetComponentLocation();
		FVector endLoc = startLoc - (wheelLoc->GetUpVector() * traceDistance);
		GetWorld()->SweepSingleByChannel(currentHit, startLoc, endLoc, FQuat::Identity, ECC_PhysicsBody, FCollisionShape::MakeSphere(wheelRadius), wheelTraceParams);

		// If the wheel has hit something make sure to mirror that force in onto the car body.
		FVector wheelCenter = currentHit.Location + ((startLoc - endLoc).GetSafeNormal() * wheelRadius);
		isGrounded = currentHit.bBlockingHit;
		if (isGrounded)
		{
			// Get dampening.
			float currentUpwardVelocityAtPoint = carBody->GetComponentRotation().UnrotateVector(carBody->GetPhysicsLinearVelocityAtPoint(startLoc)).Z;

			// Add force at spring location.
			float forceAlpha = (currentHit.Location - startLoc).Size() / traceDistance;
			float forceToAdd = FMath::Lerp(maxForce, 0.0f, forceAlpha);
			FVector force = currentHit.ImpactNormal * FMath::Clamp(forceToAdd - (currentUpwardVelocityAtPoint * traceDampening), 0.0f, BIG_NUMBER);
			carBody->AddForceAtLocation(force, startLoc);

			// Position wheel meshes wheel radius away from the hit location along the trace direction.



		}
		else
		{
			// Position wheel meshes at the end of the trace.



		}

		// If debug is enabled draw any important debug information about this trace before moving on.
		if (traceDebug)
		{
			if (isGrounded)
			{
				DrawDebugLine(GetWorld(), startLoc, currentHit.Location, FColor::Green, false, 0.02f, 0.0f, 1.0f);
				DrawDebugSphere(GetWorld(), wheelCenter, wheelRadius * 2.0f, 12.0f, FColor::Green, false, 0.02f, 0.0f, 1.0f);
			}
			else
			{
				DrawDebugLine(GetWorld(), startLoc, endLoc, FColor::Red, false, 0.02f, 0.0f, 1.0f);
				DrawDebugSphere(GetWorld(), endLoc, wheelRadius * 2.0f, 12.0f, FColor::Red, false, 0.02f, 0.0f, 1.0f);
			}
		}
	}
}

void ATracedCar::TargetRotation()
{
	if (isGrounded)
	{
		// Add angular torque on the car body.
		float currentForwardVelocity = carBody->GetComponentRotation().UnrotateVector(carBody->GetPhysicsLinearVelocity()).X;
		float max = FMath::Abs(currentForwardVelocity * 10000.0f);
		float targetTorque = steeringWheelAlpha >= 0.0f ? FMath::Clamp(1000.0f * steeringWheelAlpha, 0.0f, max) : FMath::Clamp(1000.0f * steeringWheelAlpha, -max, 0.0f);
		carBody->AddTorqueInDegrees(FVector(0.0f, 0.0f, targetTorque));

		// Add linear force at the front end of the car.
		// Trying to straighten up while driving at an angle...
		FVector wheelDiff = frontRightWheel->GetComponentLocation() - frontLeftWheel->GetComponentLocation();
		FVector centerFrontAxel = frontLeftWheel->GetComponentLocation() + wheelDiff;
		FVector force = carBody->GetRightVector() * (targetTorque / 1000.0f);
		carBody->AddForceAtLocation(force, centerFrontAxel);

		// If the hand break is engaged add force to the back of the car.
		if (handbreakEngaged)
		{
			// Add linear force at the back end of the car.
			FVector wheelBackDiff = backRightWheel->GetComponentLocation() - backLeftWheel->GetComponentLocation();
			FVector centerBackAxel = backLeftWheel->GetComponentLocation() + wheelBackDiff;
			FVector force = -carBody->GetRightVector() * (targetTorque / 1000.0f) * handbreakAlpha;
			carBody->AddForceAtLocation(force, centerBackAxel);
		}
	}
}

void ATracedCar::SetSteeringWheelAlpha(float newAlpha)
{
	steeringWheelAlpha = newAlpha;

	// Update wheel meshes rotations.



}

void ATracedCar::SetHandbreakAlpha(float newAlpha)
{
	float alpha = FMath::Abs(newAlpha);
	handbreakAlpha = alpha;
	if (alpha != 0.0f) handbreakEngaged = true;
	else handbreakEngaged = false;
}

void ATracedCar::UpdateCarMovement(float currentSpeed)
{
	if (isGrounded)
	{
		// Speed up and break the car depending on current car velocity.
		FVector currentRelativeVelocity = carBody->GetComponentRotation().UnrotateVector(carBody->GetPhysicsLinearVelocity());
		float currentForwardVelocity = carBody->GetComponentRotation().UnrotateVector(carBody->GetPhysicsLinearVelocity()).X;
		if (currentSpeed > 0.0f)
		{
			float forwardForce = FMath::Clamp((currentSpeed * 1000.0f) - (currentForwardVelocity * traceDampening), 0.0f, BIG_NUMBER);
			carBody->AddForce(forwardForce * carBody->GetForwardVector());

			// Update wheel meshes rotations.
			
		}
		else
		{		
			float breakForceX = FMath::Clamp(currentRelativeVelocity.X * currentSpeed, -BIG_NUMBER, 0.0f);
			float sideBreakForce = currentRelativeVelocity.Y * -80.0f;
			float breakForceY = currentRelativeVelocity.Y >= 0.0f ? FMath::Clamp(sideBreakForce, -BIG_NUMBER, 0.0f) : FMath::Clamp(sideBreakForce, 0.0f, BIG_NUMBER);
			carBody->AddForce(carBody->GetComponentRotation().RotateVector(FVector(breakForceX, breakForceY, 0.0f)));
		}
	}
}