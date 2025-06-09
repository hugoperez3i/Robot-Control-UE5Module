// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
	
#include "CoreMinimal.h"
#include "CLIStatusCode.generated.h"
	
/**
 * Enum with the different Client status codes the Remote Client System might take
 */
UENUM(BlueprintType)
enum class ECLIStatusCode : uint8
{
	IDLE 						=0 UMETA(DisplayName = "Idle"),
	ERRORED						=1 UMETA(DisplayName = "Idle"),

	WAITING_SERVER_ACK			=1 UMETA(DisplayName = "Waiting for server confirmation"),
	WAITING_MCU_ACK				=2 UMETA(DisplayName = "Waiting for movement completion"),
	RETRIEVING_INFO				=252 UMETA(DisplayName = "Processing iMCU query"),
	RETRIEVING_INFO_sMCU		=253 UMETA(DisplayName = "Processing iMCU query"),
	ON_MCU_SELECT				=254 UMETA(DisplayName = "Processing sMCU query"),
	STARTING_UP					=255 UMETA(DisplayName = "Client Starting up")
};