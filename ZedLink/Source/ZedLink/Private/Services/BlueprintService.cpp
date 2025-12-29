// Copyright Krishna Teja Mekala. All Rights Reserved.

#include "Services/BlueprintService.h"
#include "Server/ZedLinkServer.h"
#include "ZedLinkModule.h"

#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "SourceCodeNavigation.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

FBlueprintService::FBlueprintService(TSharedPtr<FZedLinkServer> InServer)
	: Server(InServer)
{
}

FBlueprintService::~FBlueprintService()
{
	Shutdown();
}

void FBlueprintService::Initialize()
{
	SubscribeToBlueprintEvents();

	// Subscribe to server messages
	TSharedPtr<FZedLinkServer> ServerPtr = Server.Pin();
	if (ServerPtr.IsValid())
	{
		OnMessageHandle = ServerPtr->OnMessageReceived.AddRaw(this, &FBlueprintService::HandleRequest);
	}

	UE_LOG(LogZedLink, Log, TEXT("BlueprintService: Initialized"));
}

void FBlueprintService::Shutdown()
{
	UnsubscribeBlueprintEvents();

	TSharedPtr<FZedLinkServer> ServerPtr = Server.Pin();
	if (ServerPtr.IsValid())
	{
		ServerPtr->OnMessageReceived.Remove(OnMessageHandle);
	}
}

void FBlueprintService::SubscribeToBlueprintEvents()
{
	if (GEditor)
	{
		OnCompiledHandle = GEditor->OnBlueprintCompiled().AddRaw(this, &FBlueprintService::OnBlueprintCompiled);
	}
}

void FBlueprintService::UnsubscribeBlueprintEvents()
{
	if (GEditor)
	{
		GEditor->OnBlueprintCompiled().Remove(OnCompiledHandle);
	}
}

TArray<FString> FBlueprintService::GetAllBlueprintAssets(const FString& Filter)
{
	TArray<FString> Results;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter ARFilter;
	ARFilter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	ARFilter.bRecursiveClasses = true;

	TArray<FAssetData> AssetList;
	AssetRegistry.GetAssets(ARFilter, AssetList);

	for (const FAssetData& Asset : AssetList)
	{
		FString AssetPath = Asset.GetObjectPathString();

		if (Filter.IsEmpty() || AssetPath.Contains(Filter))
		{
			Results.Add(AssetPath);
		}
	}

	return Results;
}

TSharedPtr<FBlueprintInfo> FBlueprintService::GetBlueprintDetails(const FString& AssetPath)
{
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!Blueprint)
	{
		UE_LOG(LogZedLink, Warning, TEXT("BlueprintService: Failed to load Blueprint: %s"), *AssetPath);
		return nullptr;
	}

	TSharedPtr<FBlueprintInfo> Info = MakeShared<FBlueprintInfo>();
	Info->AssetPath = AssetPath;

	if (Blueprint->GeneratedClass)
	{
		Info->ClassName = Blueprint->GeneratedClass->GetName();
	}

	// Get parent class info
	if (Blueprint->ParentClass)
	{
		Info->ParentClassName = Blueprint->ParentClass->GetName();

		// Try to find C++ source file for parent
		FString ModuleName, RelativePath;
		if (FSourceCodeNavigation::FindClassHeaderPath(Blueprint->ParentClass, RelativePath))
		{
			Info->ParentSourceFile = RelativePath;
			Info->ParentSourceLine = 1; // Header file, line 1
		}
	}

	// Collect function graphs
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph)
		{
			Info->Functions.Add(Graph->GetName());
		}
	}

	// Collect event graphs
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph)
		{
			Info->EventGraphs.Add(Graph->GetName());
		}
	}

	// Collect variables
	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		Info->Variables.Add(Var.VarName.ToString());
	}

	return Info;
}

bool FBlueprintService::OpenBlueprintInEditor(const FString& AssetPath, const FString& FocusNode)
{
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!Blueprint)
	{
		UE_LOG(LogZedLink, Warning, TEXT("BlueprintService: Failed to load Blueprint for opening: %s"), *AssetPath);
		return false;
	}

	// Open the Blueprint editor
	if (GEditor)
	{
		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		if (AssetEditorSubsystem)
		{
			AssetEditorSubsystem->OpenEditorForAsset(Blueprint);
			UE_LOG(LogZedLink, Log, TEXT("BlueprintService: Opened Blueprint: %s"), *AssetPath);
			return true;
		}
	}

	return false;
}

bool FBlueprintService::FindCppSourceLocation(const FString& ClassName, FString& OutFile, int32& OutLine)
{
	// Try to find the class by name
	UClass* FoundClass = FindObject<UClass>(ANY_PACKAGE, *ClassName);
	if (!FoundClass)
	{
		// Try with prefix
		FoundClass = FindObject<UClass>(ANY_PACKAGE, *(TEXT("U") + ClassName));
		if (!FoundClass)
		{
			FoundClass = FindObject<UClass>(ANY_PACKAGE, *(TEXT("A") + ClassName));
		}
	}

	if (FoundClass)
	{
		FString HeaderPath;
		if (FSourceCodeNavigation::FindClassHeaderPath(FoundClass, HeaderPath))
		{
			OutFile = HeaderPath;
			OutLine = 1;
			return true;
		}
	}

	return false;
}

TArray<FString> FBlueprintService::FindBlueprintReferences(const FString& ClassName)
{
	TArray<FString> Results;

	// Get all Blueprints and check their parent classes
	TArray<FString> AllBlueprints = GetAllBlueprintAssets(TEXT(""));

	for (const FString& BpPath : AllBlueprints)
	{
		TSharedPtr<FBlueprintInfo> Info = GetBlueprintDetails(BpPath);
		if (Info.IsValid() && Info->ParentClassName == ClassName)
		{
			Results.Add(BpPath);
		}
	}

	return Results;
}

void FBlueprintService::OnBlueprintCompiled(UBlueprint* Blueprint)
{
	if (!Blueprint)
	{
		return;
	}

	TSharedPtr<FZedLinkServer> ServerPtr = Server.Pin();
	if (!ServerPtr.IsValid() || !ServerPtr->IsClientConnected())
	{
		return;
	}

	TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("assetPath"), Blueprint->GetPathName());
	Params->SetStringField(TEXT("changeType"), TEXT("compiled"));

	if (Blueprint->GeneratedClass)
	{
		Params->SetStringField(TEXT("className"), Blueprint->GeneratedClass->GetName());
	}

	ServerPtr->SendNotification(TEXT("blueprint/changed"), Params);
}

void FBlueprintService::HandleRequest(const FString& JsonMessage)
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

	// Only handle blueprint/* methods
	if (!Method.StartsWith(TEXT("blueprint/")))
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

	if (Method == TEXT("blueprint/listClasses"))
	{
		const TSharedPtr<FJsonObject>* ParamsPtr = nullptr;
		FString Filter;

		if (JsonObject->TryGetObjectField(TEXT("params"), ParamsPtr) && ParamsPtr)
		{
			(*ParamsPtr)->TryGetStringField(TEXT("filter"), Filter);
		}

		TArray<FString> Blueprints = GetAllBlueprintAssets(Filter);

		TArray<TSharedPtr<FJsonValue>> JsonArray;
		for (const FString& Bp : Blueprints)
		{
			JsonArray.Add(MakeShared<FJsonValueString>(Bp));
		}

		ServerPtr->SendResponse(Id, MakeShared<FJsonValueArray>(JsonArray));
	}
	else if (Method == TEXT("blueprint/getDetails"))
	{
		const TSharedPtr<FJsonObject>* ParamsPtr = nullptr;
		FString AssetPath;

		if (JsonObject->TryGetObjectField(TEXT("params"), ParamsPtr) && ParamsPtr)
		{
			(*ParamsPtr)->TryGetStringField(TEXT("assetPath"), AssetPath);
		}

		TSharedPtr<FBlueprintInfo> Info = GetBlueprintDetails(AssetPath);

		if (Info.IsValid())
		{
			TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
			Result->SetStringField(TEXT("assetPath"), Info->AssetPath);
			Result->SetStringField(TEXT("className"), Info->ClassName);
			Result->SetStringField(TEXT("parentClassName"), Info->ParentClassName);
			Result->SetStringField(TEXT("parentSourceFile"), Info->ParentSourceFile);
			Result->SetNumberField(TEXT("parentSourceLine"), Info->ParentSourceLine);

			TArray<TSharedPtr<FJsonValue>> FunctionsArray;
			for (const FString& Func : Info->Functions)
			{
				FunctionsArray.Add(MakeShared<FJsonValueString>(Func));
			}
			Result->SetArrayField(TEXT("functions"), FunctionsArray);

			TArray<TSharedPtr<FJsonValue>> VariablesArray;
			for (const FString& Var : Info->Variables)
			{
				VariablesArray.Add(MakeShared<FJsonValueString>(Var));
			}
			Result->SetArrayField(TEXT("variables"), VariablesArray);

			ServerPtr->SendResponse(Id, MakeShared<FJsonValueObject>(Result));
		}
		else
		{
			ServerPtr->SendError(Id, -32602, TEXT("Blueprint not found"));
		}
	}
	else if (Method == TEXT("blueprint/openAsset"))
	{
		const TSharedPtr<FJsonObject>* ParamsPtr = nullptr;
		FString AssetPath;
		FString FocusNode;

		if (JsonObject->TryGetObjectField(TEXT("params"), ParamsPtr) && ParamsPtr)
		{
			(*ParamsPtr)->TryGetStringField(TEXT("assetPath"), AssetPath);
			(*ParamsPtr)->TryGetStringField(TEXT("focusNode"), FocusNode);
		}

		bool bSuccess = OpenBlueprintInEditor(AssetPath, FocusNode);

		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), bSuccess);
		ServerPtr->SendResponse(Id, MakeShared<FJsonValueObject>(Result));
	}
	else if (Method == TEXT("blueprint/findReferences"))
	{
		const TSharedPtr<FJsonObject>* ParamsPtr = nullptr;
		FString ClassName;

		if (JsonObject->TryGetObjectField(TEXT("params"), ParamsPtr) && ParamsPtr)
		{
			(*ParamsPtr)->TryGetStringField(TEXT("className"), ClassName);
		}

		TArray<FString> References = FindBlueprintReferences(ClassName);

		TArray<TSharedPtr<FJsonValue>> JsonArray;
		for (const FString& Ref : References)
		{
			JsonArray.Add(MakeShared<FJsonValueString>(Ref));
		}

		ServerPtr->SendResponse(Id, MakeShared<FJsonValueArray>(JsonArray));
	}
}
