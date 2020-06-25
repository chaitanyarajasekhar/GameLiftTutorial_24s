// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GameLiftTutorialGameMode.h"
#include "GameLiftTutorialCharacter.h"
#include "UObject/ConstructorHelpers.h"

AGameLiftTutorialGameMode::AGameLiftTutorialGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPersonCPP/Blueprints/ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}

void AGameLiftTutorialGameMode::BeginPlay()
{
	Super::BeginPlay();

#if WITH_GAMELIFT

	auto InitSDKOutcome = Aws::GameLift::Server::InitSDK();

	if (InitSDKOutcome.IsSuccess())
	{
		//callback function params are passed by us
		auto OnStartGameSession = [](Aws::GameLift::Server::Model::GameSession GameSessionObj, void* Params)
		{
			// pass params to the state 
			FStartGameSessionState* State = (FStartGameSessionState*)Params;

			// 
			State->Status = Aws::GameLift::Server::ActivateGameSession().IsSuccess();
		};

		// backfill case 
		auto OnUpdateGameSession = [](Aws::GameLift::Server::Model::UpdateGameSession UpdateGameSessionObj, void* Params)
		{
			FUpdateGameSessionState* State = (FUpdateGameSessionState*)Params;
		};

		// gamelift feels server is unhealth, and spot interuption 
		auto OnProcessTerminate = [](void* Params)
		{
			FProcessTerminateState* State = (FProcessTerminateState*)Params;

			auto GetTerminationTimeOutcome = Aws::GameLift::Server::GetTerminationTime();
			if (GetTerminationTimeOutcome.IsSuccess())
			{
				State->TerminationTime = GetTerminationTimeOutcome.GetResult();
				// clean up outside the call back function 
			}

			auto ProcessEndingOutcome = Aws::GameLift::Server::ProcessEnding();

			if (ProcessEndingOutcome.IsSuccess())
			{
				State->Status = true;
				FGenericPlatformMisc::RequestExit(false);
			}
								   
		};

		auto OnHealthCheck = [](void* Params)
		{
			//every 60sec check for the health
			FHealthCheckState* State = (FHealthCheckState*)Params;
			State->Status = true;

			return State->Status;
		};

		TArray<FString> CommandLineTokens;
		TArray<FString> CommandLineSwitches;
		int Port = FURL::UrlConfig.DefaultPort;

		//GameLiftTutorialServer.exe token -port=7777
		FCommandLine::Parse(FCommandLine::Get(), CommandLineTokens, CommandLineSwitches);

		for (FString Str : CommandLineSwitches)
		{
			FString Key;
			FString Value;

			if (Str.Split("=", &Key, &Value))
			{
				if (Key.Equals("port"))
				{
					Port = FCString::Atoi(*Value);
				}
			}
		}

		// create a log for every session 
		const char* LogFile = "aLogFile.txt";
		const char** LogFiles = &LogFile;
		auto LogParams = new Aws::GameLift::Server::LogParameters(LogFiles, 1);

		auto Params = new Aws::GameLift::Server::ProcessParameters(
			OnStartGameSession,
			&StartGameSessionState,
			OnUpdateGameSession,
			&UpdateGameSessionState,
			OnProcessTerminate,
			&ProcessTerminateState,
			OnHealthCheck,
			&HealthCheckState,
			Port,
			*LogParams
		);

		//start the session
		auto ProcessReadyOutcome = Aws::GameLift::Server::ProcessReady(*Params);

	}
#endif
}