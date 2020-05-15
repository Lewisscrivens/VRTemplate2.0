// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EffectsContainer.generated.h"

/** Declare log type for this class. */
DECLARE_LOG_CATEGORY_EXTERN(LogEffectsContainer, Log, All);

/** Define classes used. */
class UHapticFeedbackEffect_Base;
class UParticleSystemComponent;
class USoundBase;

/** Component to store and play haptic feedback and audio for the VRPawn and hands it owns. */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class VRPROJECT_API UEffectsContainer : public UActorComponent
{
	GENERATED_BODY()

protected:

	/** Level Start */
	virtual void BeginPlay() override;

public:

	/** The map to store each feedback effect with a name reference attached to it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
	TMap<FName, UHapticFeedbackEffect_Base*> feedbackContainer;

	/** The map to store each particle system with a name reference attached to it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
	TMap<FName, UParticleSystemComponent*> particlesContainer;

	/** The map to store each audio sound cue with a name reference attached to it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
	TMap<FName, USoundBase*> audioContainer;

	/** Constructor */
	UEffectsContainer();

	/** Function for returning a haptic feedback effect from a given name. */
	UFUNCTION(BlueprintCallable, Category = "EffectsContainer")
	UHapticFeedbackEffect_Base* GetFeedback(FName feedbackName);

	/** Function for returning a particle effect from a given name. */
	UFUNCTION(BlueprintCallable, Category = "EffectsContainer")
	UParticleSystemComponent* GetParticleSystem(FName particleName);

	/** Function for returning a sound cue from a given name. */
	UFUNCTION(BlueprintCallable, Category = "EffectsContainer")
	USoundBase* GetAudio(FName audioName);
};
