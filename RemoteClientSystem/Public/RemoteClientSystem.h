// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ServoInfo.h"
#include "CLIErrorCode.h"
#include "CLIStatusCode.h"
#include "RemoteClientSystem.generated.h"

/**
 * Remote client system, used as interface for remote operation of the Youbionic Half robot.
 */
UCLASS()
class REMOTECLIENTSYSTEM_API URemoteClientSystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

	public:

		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Config")
		int32 Port = 54817;

		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Config")
		FString IpAdr = "192.168.137.1";

		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Config")
		FString mcuName = "smartMCU";

		virtual void Initialize(FSubsystemCollectionBase& Collection) override;
		virtual void Deinitialize() override;

		UFUNCTION(BlueprintCallable, Category="Remote client system")
		void ConnectToServer();

		UFUNCTION(BlueprintCallable, Category="Remote client system")
		void DisconnectFromServer();
		
		UFUNCTION(BlueprintCallable, Category="Remote client system")
		void SelectMCU(FString MCU_Name);

		UFUNCTION(BlueprintCallable, Category="Remote client system")
		void RetrieveMCUInfo();

		UFUNCTION(BlueprintCallable, Category="Remote client system")
		TArray<FServoInfo> GetCurrentServoPositions();

		TArray<uint8> _GetCurrentServoPositions();		

		UFUNCTION(BlueprintCallable, Category="Remote client system")
		void SendMovement(TArray<FServoInfo> servoMovements);

		UFUNCTION(BlueprintCallable, Category="Remote client system")
		void ClearErr();

		UFUNCTION(BlueprintCallable, Category="Remote client system")
		ECLIErrorCode GetErr();

	private:
	
		FSocket* sck;
		FCriticalSection MTX_statusCheck;
		FCriticalSection MTX_pendingMovements;
		FCriticalSection MTX_currentPositions;

		std::atomic<bool> err = false;
		std::atomic<ECLIErrorCode> errCode = ECLIErrorCode::CLEAR;

		std::atomic<ECLIStatusCode> status = ECLIStatusCode::STARTING_UP;

		std::atomic<bool> PENDING_MOVEMENT = false;
		TArray<uint8> pendingMovements;

		TArray<uint8> currentServoPositions;
		uint8 servoCount=0;

		bool _is_ACK(const TArray<uint8>& query);
		bool _is_NACK(const TArray<uint8>& query);
		bool _is_iMCU(const TArray<uint8>& query);
		void _CloseConnection();
};

DECLARE_LOG_CATEGORY_EXTERN(LogRemoteClientSystem, Log, All);