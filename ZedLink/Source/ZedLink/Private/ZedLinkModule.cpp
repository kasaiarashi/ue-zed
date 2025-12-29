// Copyright Krishna Teja Mekala. All Rights Reserved.

#include "ZedLinkModule.h"
#include "Server/ZedLinkServer.h"
#include "Services/BlueprintService.h"
#include "Services/LogService.h"
#include "Services/PlayService.h"
#include "Services/BuildService.h"

#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "Interfaces/IProjectManager.h"

#define LOCTEXT_NAMESPACE "FZedLinkModule"

DEFINE_LOG_CATEGORY(LogZedLink);

void FZedLinkModule::StartupModule()
{
	UE_LOG(LogZedLink, Log, TEXT("ZedLink: Starting up..."));

	Server = MakeShared<FZedLinkServer>();

	InitializeServices();

	if (Server->Start(ServerPort))
	{
		UE_LOG(LogZedLink, Log, TEXT("ZedLink: Server started on port %d"), ServerPort);
		WritePortFile();
	}
	else
	{
		UE_LOG(LogZedLink, Error, TEXT("ZedLink: Failed to start server on port %d"), ServerPort);
	}

	RegisterMenus();
}

void FZedLinkModule::ShutdownModule()
{
	UE_LOG(LogZedLink, Log, TEXT("ZedLink: Shutting down..."));

	UnregisterMenus();
	DeletePortFile();
	ShutdownServices();

	if (Server.IsValid())
	{
		Server->Stop();
		Server.Reset();
	}
}

FZedLinkModule& FZedLinkModule::Get()
{
	return FModuleManager::GetModuleChecked<FZedLinkModule>("ZedLink");
}

bool FZedLinkModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded("ZedLink");
}

void FZedLinkModule::SetServerPort(int32 Port)
{
	if (Port != ServerPort && Port > 0 && Port < 65536)
	{
		ServerPort = Port;

		if (Server.IsValid() && Server->IsRunning())
		{
			Server->Stop();
			DeletePortFile();

			if (Server->Start(ServerPort))
			{
				WritePortFile();
			}
		}
	}
}

bool FZedLinkModule::IsClientConnected() const
{
	return Server.IsValid() && Server->IsClientConnected();
}

void FZedLinkModule::InitializeServices()
{
	BlueprintService = MakeShared<FBlueprintService>(Server);
	LogService = MakeShared<FLogService>(Server);
	PlayService = MakeShared<FPlayService>(Server);
	BuildService = MakeShared<FBuildService>(Server);

	BlueprintService->Initialize();
	LogService->Initialize();
	PlayService->Initialize();
	BuildService->Initialize();

	UE_LOG(LogZedLink, Log, TEXT("ZedLink: Services initialized"));
}

void FZedLinkModule::ShutdownServices()
{
	if (BuildService.IsValid())
	{
		BuildService->Shutdown();
		BuildService.Reset();
	}

	if (PlayService.IsValid())
	{
		PlayService->Shutdown();
		PlayService.Reset();
	}

	if (LogService.IsValid())
	{
		LogService->Shutdown();
		LogService.Reset();
	}

	if (BlueprintService.IsValid())
	{
		BlueprintService->Shutdown();
		BlueprintService.Reset();
	}

	UE_LOG(LogZedLink, Log, TEXT("ZedLink: Services shut down"));
}

void FZedLinkModule::RegisterMenus()
{
	// TODO: Register toolbar button and menu items
}

void FZedLinkModule::UnregisterMenus()
{
	// TODO: Unregister toolbar button and menu items
}

void FZedLinkModule::WritePortFile()
{
	FString PortFilePath = FPaths::ProjectIntermediateDir() / TEXT("ZedLink.port");
	FString PortString = FString::FromInt(ServerPort);

	if (FFileHelper::SaveStringToFile(PortString, *PortFilePath))
	{
		UE_LOG(LogZedLink, Log, TEXT("ZedLink: Port file written to %s"), *PortFilePath);
	}
	else
	{
		UE_LOG(LogZedLink, Warning, TEXT("ZedLink: Failed to write port file"));
	}
}

void FZedLinkModule::DeletePortFile()
{
	FString PortFilePath = FPaths::ProjectIntermediateDir() / TEXT("ZedLink.port");

	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*PortFilePath))
	{
		FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*PortFilePath);
		UE_LOG(LogZedLink, Log, TEXT("ZedLink: Port file deleted"));
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FZedLinkModule, ZedLink)
