// Copyright Krishna Teja Mekala. All Rights Reserved.

#include "Services/PlayService.h"
#include "Server/ZedLinkServer.h"
#include "ZedLinkModule.h"

#include "Editor.h"
#include "LevelEditor.h"
#include "ILevelViewport.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

FPlayService::FPlayService(TSharedPtr<FZedLinkServer> InServer)
	: Server(InServer)
{
}

FPlayService::~FPlayService()
{
	Shutdown();
}

void FPlayService::Initialize()
{
	// Subscribe to PIE events
	OnStartedHandle = FEditorDelegates::PostPIEStarted.AddRaw(this, &FPlayService::OnPIEStarted);
	OnEndedHandle = FEditorDelegates::EndPIE.AddRaw(this, &FPlayService::OnPIEEnded);

	// Subscribe to server messages
	TSharedPtr<FZedLinkServer> ServerPtr = Server.Pin();
	if (ServerPtr.IsValid())
	{
		OnMessageHandle = ServerPtr->OnMessageReceived.AddRaw(this, &FPlayService::HandleRequest);
	}

	UE_LOG(LogZedLink, Log, TEXT("PlayService: Initialized"));
}

void FPlayService::Shutdown()
{
	FEditorDelegates::PostPIEStarted.Remove(OnStartedHandle);
	FEditorDelegates::EndPIE.Remove(OnEndedHandle);

	TSharedPtr<FZedLinkServer> ServerPtr = Server.Pin();
	if (ServerPtr.IsValid())
	{
		ServerPtr->OnMessageReceived.Remove(OnMessageHandle);
	}
}

bool FPlayService::StartPIE(const FString& Mode, int32 PlayerCount, bool bDedicatedServer)
{
	if (!GUnrealEd)
	{
		UE_LOG(LogZedLink, Error, TEXT("PlayService: GUnrealEd not available"));
		return false;
	}

	if (IsPlaying())
	{
		UE_LOG(LogZedLink, Warning, TEXT("PlayService: PIE session already running"));
		return false;
	}

	FRequestPlaySessionParams Params;

	// Configure based on mode
	if (Mode == TEXT("Simulate") || Mode == TEXT("SIE"))
	{
		Params.WorldType = EPlaySessionWorldType::SimulateInEditor;
	}
	else
	{
		Params.WorldType = EPlaySessionWorldType::PlayInEditor;
	}

	// Get the active viewport
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<IAssetViewport> ActiveViewport = LevelEditorModule.GetFirstActiveViewport();
	if (ActiveViewport.IsValid())
	{
		Params.DestinationSlateViewport = ActiveViewport;
	}

	UE_LOG(LogZedLink, Log, TEXT("PlayService: Starting %s session"), *Mode);
	GUnrealEd->RequestPlaySession(Params);

	return true;
}

bool FPlayService::StopPIE()
{
	if (!GUnrealEd)
	{
		return false;
	}

	if (!IsPlaying())
	{
		return false;
	}

	UE_LOG(LogZedLink, Log, TEXT("PlayService: Stopping PIE session"));
	GUnrealEd->RequestEndPlayMap();
	return true;
}

bool FPlayService::PausePIE()
{
	if (!GUnrealEd || !GUnrealEd->PlayWorld)
	{
		return false;
	}

	if (!IsPlaying() || IsPaused())
	{
		return false;
	}

	GUnrealEd->PlayWorld->bDebugPauseExecution = true;
	GUnrealEd->SetPIEWorldsPaused(true);
	BroadcastStateChange();

	UE_LOG(LogZedLink, Log, TEXT("PlayService: Paused PIE session"));
	return true;
}

bool FPlayService::ResumePIE()
{
	if (!GUnrealEd || !GUnrealEd->PlayWorld)
	{
		return false;
	}

	if (!IsPaused())
	{
		return false;
	}

	GUnrealEd->PlayWorld->bDebugPauseExecution = false;
	GUnrealEd->SetPIEWorldsPaused(false);
	BroadcastStateChange();

	UE_LOG(LogZedLink, Log, TEXT("PlayService: Resumed PIE session"));
	return true;
}

bool FPlayService::TogglePause()
{
	if (IsPaused())
	{
		return ResumePIE();
	}
	else
	{
		return PausePIE();
	}
}

bool FPlayService::IsPlaying() const
{
	return GUnrealEd && GUnrealEd->PlayWorld != nullptr;
}

bool FPlayService::IsPaused() const
{
	return GUnrealEd && GUnrealEd->PlayWorld && GUnrealEd->PlayWorld->bDebugPauseExecution;
}

bool FPlayService::IsSimulating() const
{
	return GUnrealEd && GUnrealEd->IsSimulatingInEditor();
}

FString FPlayService::GetCurrentState() const
{
	if (!IsPlaying())
	{
		return TEXT("Stopped");
	}

	if (IsPaused())
	{
		return TEXT("Paused");
	}

	if (IsSimulating())
	{
		return TEXT("Simulating");
	}

	return TEXT("Playing");
}

void FPlayService::OnPIEStarted(bool bSimulating)
{
	UE_LOG(LogZedLink, Log, TEXT("PlayService: PIE started (simulating: %s)"), bSimulating ? TEXT("true") : TEXT("false"));
	BroadcastStateChange();
}

void FPlayService::OnPIEEnded(bool bSimulating)
{
	UE_LOG(LogZedLink, Log, TEXT("PlayService: PIE ended"));
	BroadcastStateChange();
}

void FPlayService::BroadcastStateChange()
{
	TSharedPtr<FZedLinkServer> ServerPtr = Server.Pin();
	if (!ServerPtr.IsValid() || !ServerPtr->IsClientConnected())
	{
		return;
	}

	TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("state"), GetCurrentState());

	ServerPtr->SendNotification(TEXT("play/stateChanged"), Params);
}

void FPlayService::HandleRequest(const FString& JsonMessage)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonMessage);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return;
	}

	FString Method;
	if (!JsonObject->TryGetStringField(TEXT("method"), Method))
	{
		return;
	}

	// Only handle play/* methods
	if (!Method.StartsWith(TEXT("play/")))
	{
		return;
	}

	FString Id;
	JsonObject->TryGetStringField(TEXT("id"), Id);

	TSharedPtr<FZedLinkServer> ServerPtr = Server.Pin();
	if (!ServerPtr.IsValid())
	{
		return;
	}

	if (Method == TEXT("play/start"))
	{
		const TSharedPtr<FJsonObject>* ParamsPtr = nullptr;
		FString Mode = TEXT("PIE");
		int32 PlayerCount = 1;
		bool bDedicated = false;

		if (JsonObject->TryGetObjectField(TEXT("params"), ParamsPtr) && ParamsPtr)
		{
			(*ParamsPtr)->TryGetStringField(TEXT("mode"), Mode);
			(*ParamsPtr)->TryGetNumberField(TEXT("playerCount"), PlayerCount);
			(*ParamsPtr)->TryGetBoolField(TEXT("dedicated"), bDedicated);
		}

		bool bSuccess = StartPIE(Mode, PlayerCount, bDedicated);

		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), bSuccess);
		ServerPtr->SendResponse(Id, MakeShared<FJsonValueObject>(Result));
	}
	else if (Method == TEXT("play/stop"))
	{
		bool bSuccess = StopPIE();

		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), bSuccess);
		ServerPtr->SendResponse(Id, MakeShared<FJsonValueObject>(Result));
	}
	else if (Method == TEXT("play/pause"))
	{
		const TSharedPtr<FJsonObject>* ParamsPtr = nullptr;
		bool bPaused = true;

		if (JsonObject->TryGetObjectField(TEXT("params"), ParamsPtr) && ParamsPtr)
		{
			(*ParamsPtr)->TryGetBoolField(TEXT("paused"), bPaused);
		}

		bool bSuccess = bPaused ? PausePIE() : ResumePIE();

		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), bSuccess);
		ServerPtr->SendResponse(Id, MakeShared<FJsonValueObject>(Result));
	}
	else if (Method == TEXT("play/getState"))
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("state"), GetCurrentState());
		Result->SetBoolField(TEXT("isPlaying"), IsPlaying());
		Result->SetBoolField(TEXT("isPaused"), IsPaused());
		Result->SetBoolField(TEXT("isSimulating"), IsSimulating());
		ServerPtr->SendResponse(Id, MakeShared<FJsonValueObject>(Result));
	}
}
