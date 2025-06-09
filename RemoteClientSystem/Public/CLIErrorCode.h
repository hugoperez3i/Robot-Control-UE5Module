
#pragma once
	
#include "CoreMinimal.h"
#include "CLIErrorCode.generated.h"
	
/**
 * Enum with the different Error codes the Remote Client System might take
 */
UENUM(BlueprintType)
enum class ECLIErrorCode : uint8
{
	CLEAR 						=0 	 UMETA(DisplayName = "No Error"),
	GenericError 				=1 	 UMETA(DisplayName = "Generic Error"),
    ServerConnError             =2   UMETA(DisplayName = "Server Connection Error"),
    CorruptedICMU               =3   UMETA(DisplayName = "Information from server (iMCU) was corrupted"),

	INVALID_SERVO_ID	   		=100 UMETA(DisplayName = "ServoID out of range"),
	INVALID_SERVO_POSITION 		=101 UMETA(DisplayName = "ServoPosition out of range"),

	NACK_InvalidQuery			=250 UMETA(DisplayName = "Server NACK - Invalid Query"),
    NACK_NoActiveMCU			=249 UMETA(DisplayName = "Server NACK - No Active MCU"),
    NACK_InvalidParameter		=247 UMETA(DisplayName = "Server NACK - Invalid Parameter"),
    NACK_ServoCountMismatch		=246 UMETA(DisplayName = "Server NACK - Servo Count Mismatch"),
    NACK_NoMCUInfo				=245 UMETA(DisplayName = "Server NACK - No MCU Info"),
    NACK_MCUOffline				=244 UMETA(DisplayName = "Server NACK - MCU Offline"),
    NACK_ErrorContactingMCU		=243 UMETA(DisplayName = "Server NACK - Error Contacting MCU"),
};