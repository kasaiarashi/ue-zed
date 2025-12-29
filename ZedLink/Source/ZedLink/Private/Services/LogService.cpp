// Copyright Krishna Teja Mekala. All Rights Reserved.

#include "Services/LogService.h"
#include "Server/ZedLinkServer.h"
#include "ZedLinkModule.h"

#include "Misc/DateTime.h"
#include "Dom/JsonObject.h"

FLogService::FLogService(TSharedPtr<FZedLinkServer> InServer)
	: Server(InServer)
{
}

FLogService::~FLogService()
{
	Shutdown();
}

void FLogService::Initialize()
{
	if (GLog)
	{
		GLog->AddOutputDevice(this);
		UE_LOG(LogZedLink, Log, TEXT("LogService: Registered as output device"));
	}
}

void FLogService::Shutdown()
{
	if (GLog)
	{
		GLog->RemoveOutputDevice(this);
	}
}

void FLogService::SetCategoryFilter(const TArray<FName>& Categories)
{
	FScopeLock Lock(&LogLock);
	FilteredCategories.Empty();
	for (const FName& Category : Categories)
	{
		FilteredCategories.Add(Category);
	}
	bFilterByCategory = FilteredCategories.Num() > 0;
}

void FLogService::SetMinVerbosity(ELogVerbosity::Type Verbosity)
{
	MinVerbosity = Verbosity;
}

void FLogService::ClearFilters()
{
	FScopeLock Lock(&LogLock);
	FilteredCategories.Empty();
	bFilterByCategory = false;
	MinVerbosity = ELogVerbosity::All;
}

void FLogService::Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category)
{
	SendLogMessage(Message, Verbosity, Category);
}

void FLogService::Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category, const double Time)
{
	SendLogMessage(Message, Verbosity, Category);
}

bool FLogService::ShouldLogCategory(const FName& Category) const
{
	if (!bFilterByCategory)
	{
		return true;
	}
	return FilteredCategories.Contains(Category);
}

FString FLogService::VerbosityToString(ELogVerbosity::Type Verbosity) const
{
	switch (Verbosity)
	{
		case ELogVerbosity::Fatal:       return TEXT("Fatal");
		case ELogVerbosity::Error:       return TEXT("Error");
		case ELogVerbosity::Warning:     return TEXT("Warning");
		case ELogVerbosity::Display:     return TEXT("Display");
		case ELogVerbosity::Log:         return TEXT("Log");
		case ELogVerbosity::Verbose:     return TEXT("Verbose");
		case ELogVerbosity::VeryVerbose: return TEXT("VeryVerbose");
		default:                         return TEXT("Unknown");
	}
}

void FLogService::SendLogMessage(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category)
{
	if (!bIsEnabled)
	{
		return;
	}

	if (Verbosity > MinVerbosity)
	{
		return;
	}

	if (!ShouldLogCategory(Category))
	{
		return;
	}

	TSharedPtr<FZedLinkServer> ServerPtr = Server.Pin();
	if (!ServerPtr.IsValid() || !ServerPtr->IsClientConnected())
	{
		return;
	}

	TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("category"), Category.ToString());
	Params->SetStringField(TEXT("verbosity"), VerbosityToString(Verbosity));
	Params->SetStringField(TEXT("message"), Message);
	Params->SetStringField(TEXT("timestamp"), FDateTime::UtcNow().ToIso8601());
	Params->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));

	ServerPtr->SendNotification(TEXT("logging/message"), Params);
}
