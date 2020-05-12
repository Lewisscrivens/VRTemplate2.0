// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/HandsAnimInstance.h"

DEFINE_LOG_CATEGORY(LogHandAnimInst);

UHandsAnimInstance::UHandsAnimInstance(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	handLerpSpeed = 15.0f;
}