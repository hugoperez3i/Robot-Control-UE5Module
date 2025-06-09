// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ServoInfo.generated.h"

/**
 * Struct used to store the information pair (servo id,servo position)
 */
USTRUCT(BlueprintType)
struct FServoInfo
{
    GENERATED_BODY()

public:

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Servo Information Struct", meta=(ClampMin=0, ClampMax=31))
    uint8 servoID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Servo Information Struct", meta=(ClampMin=0, ClampMax=179))
    uint8 servoPosition;

    FServoInfo(): servoID(0), servoPosition(0){}
    
    FServoInfo(uint8 id, uint8 pos){
        servoID = FMath::Clamp<uint8>(id, 0, 31);
        servoPosition = FMath::Clamp<uint8>(pos, 0, 179);
    }
};
