
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
    NoServerConnection          =4   UMETA(DisplayName = "No active server connection"),

	INVALID_SERVO_ID	   		=100 UMETA(DisplayName = "ServoID out of range"),
	INVALID_SERVO_POSITION 		=101 UMETA(DisplayName = "ServoPosition out of range"),

	NACK_InvalidQuery			=255 UMETA(DisplayName = "Server NACK - Invalid Query"),
    NACK_NoActiveMCU			=254 UMETA(DisplayName = "Server NACK - No Active MCU"),
    NACK_OnRTMode			    =253 UMETA(DisplayName = "Server NACK - On Real Time mode"),    
    NACK_InvalidParameter		=252 UMETA(DisplayName = "Server NACK - Invalid Parameter"),
    NACK_ServoCountMismatch		=251 UMETA(DisplayName = "Server NACK - Servo Count Mismatch"),
    NACK_NoMCUInfo				=250 UMETA(DisplayName = "Server NACK - No MCU Info"),
    NACK_MCUOffline				=249 UMETA(DisplayName = "Server NACK - MCU Offline"),
    NACK_ErrorContactingMCU		=248 UMETA(DisplayName = "Server NACK - Error Contacting MCU"),
    NACK_ErrorLoadingMINMAX		=247 UMETA(DisplayName = "Server NACK - Error loading Min-Max information")
};