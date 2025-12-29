// Copyright Krishna Teja Mekala. All Rights Reserved.

#include "Services/BuildService.h"
#include "Server/ZedLinkServer.h"
#include "ZedLinkModule.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

#if WITH_LIVE_CODING
#include "ILiveCodingModule.h"
#endif

FBuildService::FBuildService(TSharedPtr<FZedLinkServer> InServer)
	: Server(InServer)
{
}

FBuildService::~FBuildService()
{
	Shutdown();
}

void FBuildService::Initialize()
{
#if WITH_LIVE_CODING
	if (ILiveCodingModule* LiveCoding = FModuleManager::LoadModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME))
	{
		OnPatchCompleteHandle = LiveCoding->GetOnPatchCompleteDelegate().AddRaw(this, &FBuildService::OnPatchComplete);
		UE_LOG(LogZedLink, Log, TEXT("BuildService: Live Coding hooks registered"));
	}
#endif

	// Subscribe to server messages
	TSharedPtr<FZedLinkServer> ServerPtr = Server.Pin();
	if (ServerPtr.IsValid())
	{
		OnMessageHandle = ServerPtr->OnMessageReceived.AddRaw(this, &FBuildService::HandleRequest);
	}

	UE_LOG(LogZedLink, Log, TEXT("BuildService: Initialized"));
}

void FBuildService::Shutdown()
{
#if WITH_LIVE_CODING
	if (ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME))
	{
		LiveCoding->GetOnPatchCompleteDelegate().Remove(OnPatchCompleteHandle);
	}
#endif

	TSharedPtr<FZedLinkServer> ServerPtr = Server.Pin();
	if (ServerPtr.IsValid())
	{
		ServerPtr->OnMessageReceived.Remove(OnMessageHandle);
	}
}

bool FBuildService::TriggerLiveCoding()
{
#if WITH_LIVE_CODING
	if (ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME))
	{
		if (LiveCoding->IsEnabledForSession())
		{
			UE_LOG(LogZedLink, Log, TEXT("BuildService: Triggering Live Coding compile"));
			bIsCompiling = true;
			BroadcastBuildStatus(TEXT("Compiling"));

			LiveCoding->Compile();
			return true;
		}
		else
		{
			UE_LOG(LogZedLink, Warning, TEXT("BuildService: Live Coding not enabled for this session"));
		}
	}
#else
	UE_LOG(LogZedLink, Warning, TEXT("BuildService: Live Coding not available in this build"));
#endif
	return false;
}

bool FBuildService::CancelLiveCoding()
{
#if WITH_LIVE_CODING
	// Note: Live Coding doesn't have a direct cancel API
	// This is a placeholder for future implementation
	UE_LOG(LogZedLink, Warning, TEXT("BuildService: Cancel Live Coding not supported"));
#endif
	return false;
}

bool FBuildService::IsLiveCodingEnabled() const
{
#if WITH_LIVE_CODING
	if (ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME))
	{
		return LiveCoding->IsEnabledForSession();
	}
#endif
	return false;
}

bool FBuildService::IsCompiling() const
{
	return bIsCompiling;
}

void FBuildService::OnPatchComplete()
{
	bIsCompiling = false;
	UE_LOG(LogZedLink, Log, TEXT("BuildService: Live Coding patch complete"));
	BroadcastBuildStatus(TEXT("Success"));
}

void FBuildService::OnCompilationStarted()
{
	bIsCompiling = true;
	BroadcastBuildStatus(TEXT("Compiling"));
}

void FBuildService::OnCompilationFinished(bool bSuccess)
{
	bIsCompiling = false;
	BroadcastBuildStatus(bSuccess ? TEXT("Success") : TEXT("Failed"));
}

void FBuildService::BroadcastBuildStatus(const FString& Status, float Progress, const FString& CurrentFile)
{
	TSharedPtr<FZedLinkServer> ServerPtr = Server.Pin();
	if (!ServerPtr.IsValid() || !ServerPtr->IsClientConnected())
	{
		return;
	}

	TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("status"), Status);
	Params->SetNumberField(TEXT("progress"), Progress);

	if (!CurrentFile.IsEmpty())
	{
		Params->SetStringField(TEXT("currentFile"), CurrentFile);
	}

	ServerPtr->SendNotification(TEXT("build/status"), Params);
}

void FBuildService::HandleRequest(const FString& JsonMessage)
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

	// Only handle build/* methods
	if (!Method.StartsWith(TEXT("build/")))
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

	if (Method == TEXT("build/liveCoding"))
	{
		bool bSuccess = TriggerLiveCoding();

		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), bSuccess);
		Result->SetBoolField(TEXT("liveCodingEnabled"), IsLiveCodingEnabled());
		ServerPtr->SendResponse(Id, MakeShared<FJsonValueObject>(Result));
	}
	else if (Method == TEXT("build/getStatus"))
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("isCompiling"), IsCompiling());
		Result->SetBoolField(TEXT("liveCodingEnabled"), IsLiveCodingEnabled());
		ServerPtr->SendResponse(Id, MakeShared<FJsonValueObject>(Result));
	}
}
