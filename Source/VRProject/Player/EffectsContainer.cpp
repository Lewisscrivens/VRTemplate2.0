// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/EffectsContainer.h"
#include "GameFramework/Actor.h"
#include "Haptics/HapticFeedbackEffect_Base.h"

DEFINE_LOG_CATEGORY(LogEffectsContainer);

UEffectsContainer::UEffectsContainer()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEffectsContainer::BeginPlay()
{
	Super::BeginPlay();
}

UHapticFeedbackEffect_Base* UEffectsContainer::GetFeedback(FName feedbackName)
{
	// If there is no intialised feedback effects in the feedbackContainer map print to log and return null.
	if (UHapticFeedbackEffect_Base* feedbackFound = *feedbackContainer.Find(feedbackName))
	{
		return feedbackFound;
	}
	return nullptr;
}

UParticleSystemComponent* UEffectsContainer::GetParticleSystem(FName particleName)
{
	// If there is no audioContainer effects return.
	if (UParticleSystemComponent* particleFound = *particlesContainer.Find(particleName))
	{
		return particleFound;
	}
	return nullptr;
}

USoundBase* UEffectsContainer::GetAudio(FName audioName)
{
	// If there is no audioContainer effects return.
	if (USoundBase* audioFound = *audioContainer.Find(audioName))
	{
		return audioFound;
	}
	return nullptr;
}

