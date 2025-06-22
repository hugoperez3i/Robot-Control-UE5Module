// Fill out your copyright notice in the Description page of Project Settings.

#include "RemoteClientSystem.h"
#include "Modules/ModuleManager.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Networking.h"


DEFINE_LOG_CATEGORY(LogRemoteClientSystem);


constexpr uint8 ID_SERVO_COUNT = 8;
constexpr uint8 ID_SERVO_DATA_START = ID_SERVO_COUNT+2;

void URemoteClientSystem::Initialize(FSubsystemCollectionBase& Collection){
    Super::Initialize(Collection);

    status=ECLIStatusCode::NO_SERVER_CONN;
    err=true;                             
    errCode=ECLIErrorCode::NoServerConnection;
    ConnectToServer();

}

void URemoteClientSystem::ConnectToServer(){

    {FScopeLock Lock(&MTX_statusCheck);
        if(status.load()!=ECLIStatusCode::STARTING_UP&&status.load()!=ECLIStatusCode::NO_SERVER_CONN){
            return;
        }
    }

    UE::Tasks::Launch(TEXT("Connecting to Server"), [this](){

        ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
        TSharedPtr<FInternetAddr> srvAddr = SocketSubsystem->CreateInternetAddr();
        bool bIsValid;
        srvAddr->SetIp(*IpAdr, bIsValid);
        srvAddr->SetPort(Port);    

        if (!bIsValid){
            /* Invalid ip:port address */
            UE_LOG(LogRemoteClientSystem, Error, TEXT("Invalid Server ip:port address"));
            {FScopeLock Lock(&MTX_statusCheck);
                status=ECLIStatusCode::NO_SERVER_CONN;
            }
            return;
        }

        sck = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("Server socket"), false);
        sck->SetNonBlocking(false);
        sck->SetReuseAddr(true);

        if(!sck->Connect(*srvAddr)){
            /* Connection refused */
            UE_LOG(LogRemoteClientSystem, Error, TEXT("Server connection refused"));
            _CloseConnection();
            {FScopeLock Lock(&MTX_statusCheck);
                status=ECLIStatusCode::NO_SERVER_CONN;
            }
            return;
        }
        UE_LOG(LogRemoteClientSystem, Display, TEXT("Connected to Server"));


        TArray<uint8> loginquery; const char* tLogin = "!s-Client_here-e!";
        loginquery.Append((const uint8*)tLogin, strlen(tLogin));

        int32 bySent=0;
        if(!sck->Send(loginquery.GetData(),loginquery.Num(),bySent)){
            /* Log in failed */
            UE_LOG(LogRemoteClientSystem, Error, TEXT("Send Server log-in failed"));
            _CloseConnection();
            {FScopeLock Lock(&MTX_statusCheck);
                status=ECLIStatusCode::NO_SERVER_CONN;
            }
            return;
        }
        UE_LOG(LogRemoteClientSystem, Display, TEXT("Logged in"));

        {FScopeLock Lock(&MTX_statusCheck);
            status=ECLIStatusCode::STARTING_UP;
        }

        err=false;
        errCode=ECLIErrorCode::CLEAR;

        SelectMCU(mcuName);

    }, UE::Tasks::ETaskPriority::High);
}

void URemoteClientSystem::DisconnectFromServer(){

    {FScopeLock Lock(&MTX_statusCheck);
        if(status.load()==ECLIStatusCode::NO_SERVER_CONN){
            return;
        }
    }

    UE::Tasks::Launch(TEXT("Server Disconnection"), [this](){

        {FScopeLock Lock(&MTX_statusCheck);
            while(!err.load()&&status.load()!=ECLIStatusCode::IDLE){
                continue;
            }

            status=ECLIStatusCode::NO_SERVER_CONN;
        }

        _CloseConnection();

    }, UE::Tasks::ETaskPriority::High);

}

void URemoteClientSystem::_CloseConnection(){
    err=true;                             
    errCode=ECLIErrorCode::NoServerConnection;
    UE_LOG(LogRemoteClientSystem, Display, TEXT("Closing socket connection."));
    if(sck){
        sck->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(sck);
        sck = nullptr;
    }
}

void URemoteClientSystem::SelectMCU(FString MCU_Name){

    {FScopeLock Lock(&MTX_statusCheck);
        if(err.load()||(status.load()!=ECLIStatusCode::IDLE&&status.load()!=ECLIStatusCode::STARTING_UP)){
            return;
        }
        status=ECLIStatusCode::ON_MCU_SELECT;
    }

    UE::Tasks::Launch(TEXT("Task_SelectionMCU"), [MCU_Name, this](){
    // Async(EAsyncExecution::Thread, [MCU_Name, this](){

        TArray<uint8> sMCU_Query; const char* tSMCU = "!s-sMCU-"; const char* tail = "-e!";
        sMCU_Query.Append((const uint8*)tSMCU, strlen(tSMCU));
        FTCHARToUTF8 name(MCU_Name);
        sMCU_Query.Append((const uint8*)name.Get(), name.Length());
        sMCU_Query.Append((const uint8*)tail, strlen(tail));
    
        int32 bySent=0;
        if(!sck || !sck->Send(sMCU_Query.GetData(),sMCU_Query.Num(),bySent)){
            err=true;                            
            errCode=ECLIErrorCode::ServerConnError;
            UE_LOG(LogRemoteClientSystem, Error, TEXT("Send sMCU query failed"));
            return;
        }
        
        TArray<uint8> recvBuffer;
        recvBuffer.SetNumUninitialized(256);
        int32 byRead = 0;
        if(sck->Recv(recvBuffer.GetData(), recvBuffer.Num(), byRead) && byRead > 0){
            recvBuffer.SetNum(byRead); // Trim excess buffer size

            if(_is_ACK(recvBuffer)){

                UE_LOG(LogRemoteClientSystem, Display, TEXT("sMCU successful: Now controlling %s"), *MCU_Name);

            }else if(_is_NACK(recvBuffer)){
                err=true;                             /*         8   */
                errCode=ECLIErrorCode(recvBuffer[8]); /* !s-NACK-x-e!*/
                status=ECLIStatusCode::IDLE;
                UE_LOG(LogRemoteClientSystem, Error, TEXT("sMCU Failed %d"), recvBuffer[8]);
                return;
            }else{
                err=true;                            
                errCode=ECLIErrorCode::ServerConnError;
                UE_LOG(LogRemoteClientSystem, Error, TEXT("Corrupt Server Response"));
                return;
            }
            

        }else{
            err=true;
            errCode=ECLIErrorCode::ServerConnError;
            UE_LOG(LogRemoteClientSystem, Error, TEXT("Server connection failed"));
            return;
        }

        status=ECLIStatusCode::RETRIEVING_INFO_sMCU;
        RetrieveMCUInfo();
    // });
    }, UE::Tasks::ETaskPriority::High);

}

void URemoteClientSystem::RetrieveMCUInfo(){

    {FScopeLock Lock(&MTX_statusCheck);
        if(err.load()||(status.load()!=ECLIStatusCode::IDLE&&status.load()!=ECLIStatusCode::RETRIEVING_INFO_sMCU)){
            return;
        }
        status=ECLIStatusCode::RETRIEVING_INFO;
    }

    UE::Tasks::Launch(TEXT("Task_RetrievingMCUInformation"), [this](){

        TArray<uint8> iMCU_Query; const char* tIMCU = "!s-iMCU-e!"; 
        iMCU_Query.Append((const uint8*)tIMCU, strlen(tIMCU));

        int32 bySent=0;
        if(!sck || !sck->Send(iMCU_Query.GetData(),iMCU_Query.Num(),bySent)){
            err=true;                            
            errCode=ECLIErrorCode::ServerConnError;
            UE_LOG(LogRemoteClientSystem, Error, TEXT("Send iMCU query failed"));
            return;
        }

        TArray<uint8> recvBuffer;
        recvBuffer.SetNumUninitialized(256);
        int32 byRead = 0;
        if(sck->Recv(recvBuffer.GetData(), recvBuffer.Num(), byRead) && byRead > 0){
            recvBuffer.SetNum(byRead); // Trim excess buffer size

            if(_is_iMCU(recvBuffer)){

                UE_LOG(LogRemoteClientSystem, Display, TEXT("iMCU successful"));

                TArray<uint8> tmp; tmp.Empty();
                servoCount=recvBuffer[ID_SERVO_COUNT];
                if(servoCount==0){
                    err=true;
                    errCode=ECLIErrorCode::CorruptedICMU;
                    return;
                }
                tmp.AddZeroed(servoCount);

                for(auto i=0; i<servoCount; i++){
                    if(!recvBuffer.IsValidIndex(ID_SERVO_DATA_START+2*i)){
                        err=true;
                        errCode=ECLIErrorCode::CorruptedICMU;
                        return;
                    }
                    tmp[i]=recvBuffer[ID_SERVO_DATA_START+2*i];
                }

                {FScopeLock Lock(&MTX_currentPositions);
                    /* Save current movements */
                    currentServoPositions.Empty();
                    currentServoPositions.Append(tmp);
                }

                UE_LOG(LogRemoteClientSystem, Display, TEXT("Retrieved %d servo positions"), servoCount);

            }else if(_is_NACK(recvBuffer)){
                err=true;                             /*         8   */
                errCode=ECLIErrorCode(recvBuffer[8]); /* !s-NACK-x-e!*/
                status=ECLIStatusCode::IDLE;
                UE_LOG(LogRemoteClientSystem, Error, TEXT("iMCU Failed %d"), recvBuffer[8]);
                return;
            }else{
                err=true;                            
                errCode=ECLIErrorCode::ServerConnError;
                UE_LOG(LogRemoteClientSystem, Error, TEXT("Corrupt Server Response"));
                return;
            }
            

        }else{
            err=true;
            errCode=ECLIErrorCode::ServerConnError;
            UE_LOG(LogRemoteClientSystem, Error, TEXT("Server connection failed"));
            return;
        }

        PENDING_MOVEMENT=false;
        pendingMovements.Empty();
        pendingMovements.AddZeroed(servoCount);
        status=ECLIStatusCode::IDLE; /* Return to idle status */
    }, UE::Tasks::ETaskPriority::High);

}

/** For BP use, servo positions in the range 0-179 */
TArray<FServoInfo> URemoteClientSystem::GetCurrentServoPositions(){

    TArray<uint8> tmp; tmp.Empty();
    {FScopeLock Lock(&MTX_currentPositions);
        /* Copy the current positions */
        tmp.Append(currentServoPositions);
    }

    TArray<FServoInfo> inf;inf.Empty();

    for(auto i=0; i<tmp.Num(); i++){
        inf.Add(FServoInfo(i,tmp[i]-1)); /* Remove the +1 offset */
    }
    return inf;
}

/** For C++ use, offset HAS NOT BEEN REMOVED of servo positions: servo positions are in the range 1-180 (TArray[i]= ServoPosition of servo with id = i) */
TArray<uint8> URemoteClientSystem::_GetCurrentServoPositions(){

    TArray<uint8> tmp; tmp.Empty();
    {FScopeLock Lock(&MTX_currentPositions);
        /* Copy the current positions */
        tmp.Append(currentServoPositions);
    }

    return tmp;
}

void URemoteClientSystem::SendMovement(TArray<FServoInfo> servoMovements){

    /* Check that system is not errored */
    if(err.load()){
        return;
    }

    /* Validate movement ids & positions */
    for (auto &&i : servoMovements){
        if(i.servoID>=servoCount){
            err=true;
            errCode=ECLIErrorCode::INVALID_SERVO_ID;
            return;
        }
        if(i.servoPosition>=180){
            err=true;
            errCode=ECLIErrorCode::INVALID_SERVO_POSITION;
            return;
        }
    }

    /* Add movements to the "pending movement list", overriding old values */
    /* Add +1 offset here, zero value means no update for that given servo */
    {FScopeLock Lock(&MTX_pendingMovements);
        for (auto &&mv : servoMovements){
            pendingMovements[mv.servoID]=1+mv.servoPosition;
        }
    }
    PENDING_MOVEMENT=true;

    {FScopeLock Lock(&MTX_statusCheck);

        if(!err.load() && status.load()==ECLIStatusCode::IDLE){
            status=ECLIStatusCode::WAITING_SERVER_ACK;
            /* Create thread & send data */
        }
        
    }

    UE::Tasks::Launch(TEXT("Task_SendMovementOrder"), [this](){

        do{
            PENDING_MOVEMENT=false;
            status=ECLIStatusCode::WAITING_SERVER_ACK;
            TArray<uint8> mvToExecute;

            {FScopeLock Lock(&MTX_pendingMovements);
                mvToExecute = pendingMovements;
                pendingMovements.Empty();
                pendingMovements.AddZeroed(servoCount);
            }

            /* Build query using the local mvToExecute copy */
            TArray<uint8> SRVP_Query; const char* tSRVP = "!s-SRVP-c-"; const char* tail = "e!";
            SRVP_Query.Append((const uint8*)tSRVP, strlen(tSRVP));
            uint8 count=0;
            for(auto i=0; i<mvToExecute.Num(); i++){
                if(mvToExecute[i]==0){continue;}
                SRVP_Query.Add(i+1); // Add servoId offset
                SRVP_Query.Add(':');SRVP_Query.Add(mvToExecute[i]);SRVP_Query.Add('-');
                count++;
            }
            SRVP_Query[ID_SERVO_COUNT]=count;
            SRVP_Query.Append((const uint8*)tail, strlen(tail));

            /* Send query to server */
            int32 bySent=0;
            if(!sck || !sck->Send(SRVP_Query.GetData(),SRVP_Query.Num(),bySent)){
                err=true;                            
                errCode=ECLIErrorCode::ServerConnError;
                UE_LOG(LogRemoteClientSystem, Error, TEXT("Send SRVP query failed"));
                return;
            }

            {/* Wait for first response (server confirmation) */
                TArray<uint8> recvBuffer;
                recvBuffer.SetNumUninitialized(256);
                int32 byRead = 0;
                if(sck->Recv(recvBuffer.GetData(), recvBuffer.Num(), byRead) && byRead > 0){
                    recvBuffer.SetNum(byRead); // Trim excess buffer size

                    if(_is_ACK(recvBuffer)){

                        UE_LOG(LogRemoteClientSystem, Display, TEXT("Movement order Accepted by server"));

                    }else if(_is_NACK(recvBuffer)){
                        err=true;                             /*         8   */
                        errCode=ECLIErrorCode(recvBuffer[8]); /* !s-NACK-x-e!*/
                        status=ECLIStatusCode::IDLE;
                        UE_LOG(LogRemoteClientSystem, Error, TEXT("SRVP Denied with error code: %d"), recvBuffer[8]);
                        return;
                    }else{
                        err=true;                            
                        errCode=ECLIErrorCode::ServerConnError;
                        UE_LOG(LogRemoteClientSystem, Error, TEXT("Corrupt Server Response"));
                        return;
                    }
                    
                }else{
                    err=true;
                    errCode=ECLIErrorCode::ServerConnError;
                    UE_LOG(LogRemoteClientSystem, Error, TEXT("Server connection failed"));
                    return;
                }
            }
            
            status=ECLIStatusCode::WAITING_MCU_ACK;
            {/* Wait for second response (relayed MCU confirmation) */
                TArray<uint8> recvBuffer;
                recvBuffer.SetNumUninitialized(256);
                int32 byRead = 0;
                if(sck->Recv(recvBuffer.GetData(), recvBuffer.Num(), byRead) && byRead > 0){
                    recvBuffer.SetNum(byRead); // Trim excess buffer size

                    if(_is_ACK(recvBuffer)){

                        UE_LOG(LogRemoteClientSystem, Display, TEXT("Movement order completed by MCU"));

                        {FScopeLock Lock(&MTX_currentPositions);
                            for(auto i=0; i<mvToExecute.Num(); i++){
                                if(mvToExecute[i]==0){continue;}
                                /* Update current servo positions */
                                currentServoPositions[i]=mvToExecute[i]; 
                            }
                        }

                    }else if(_is_NACK(recvBuffer)){
                        err=true;                             /*         8   */
                        errCode=ECLIErrorCode(recvBuffer[8]); /* !s-NACK-x-e!*/
                        status=ECLIStatusCode::IDLE;
                        UE_LOG(LogRemoteClientSystem, Error, TEXT("SRVP Denied by MCU with error code: %d"), recvBuffer[8]);
                        return;
                    }else{
                        err=true;                            
                        errCode=ECLIErrorCode::ServerConnError;
                        UE_LOG(LogRemoteClientSystem, Error, TEXT("Corrupt Server Response"));
                        return;
                    }
                    
                }else{
                    err=true;
                    errCode=ECLIErrorCode::ServerConnError;
                    UE_LOG(LogRemoteClientSystem, Error, TEXT("Server connection failed"));
                    return;
                }
            }                   

        }while(PENDING_MOVEMENT.load());

        status=ECLIStatusCode::IDLE; /* Return to idle status */

    }, UE::Tasks::ETaskPriority::High);
        

    
}

void URemoteClientSystem::ClearErr(){

    {FScopeLock Lock(&MTX_statusCheck);
        if(status.load()==ECLIStatusCode::STARTING_UP||status.load()==ECLIStatusCode::NO_SERVER_CONN){
            return;
        }
    }

    err=false;
    errCode=ECLIErrorCode::CLEAR;
    status=ECLIStatusCode::IDLE; 
}

ECLIErrorCode URemoteClientSystem::GetErr(){
    return errCode.load();
}

bool URemoteClientSystem::_is_ACK(const TArray<uint8>& query){
    static const uint8 refACK[]="!s-_ACK-c-e!";
    constexpr uint8 refLen=sizeof(refACK)-1;

    if(query.Num()!=refLen){return false;}

    for(auto i=0;i<refLen;++i){
        if(i==8){continue;} 
        if(query[i]!=refACK[i]){return false;}
    }
    return true; 
}

bool URemoteClientSystem::_is_NACK(const TArray<uint8>& query){
    static const uint8 refNACK[]="!s-NACK-c-e!";
    constexpr uint8 refLen=sizeof(refNACK)-1;

    if(query.Num()!=refLen){return false;}

    for(auto i=0;i<refLen;++i){
        if(i==8){continue;} 
        if(query[i]!=refNACK[i]){return false;}
    }
    return true; 
}

bool URemoteClientSystem::_is_iMCU(const TArray<uint8>& query){
    static const uint8 refIMCU[]="!s-iMCU-c-e!";
    constexpr uint8 refLen=sizeof(refIMCU)-1;

    if(query.Num()<refLen){return false;}
    auto offset=query.Num()-refLen;

    for(auto i=0; i<refLen;++i){
        if(i==8){continue;} 
        if(i>8){
            if(query[offset+i]!=refIMCU[i]){return false;}
        }else{
            if(query[i]!=refIMCU[i]){return false;}
        }
    }
    return true; 
}

void URemoteClientSystem::Deinitialize(){

    DisconnectFromServer();

}

IMPLEMENT_MODULE(FDefaultModuleImpl, RemoteClientSystem)