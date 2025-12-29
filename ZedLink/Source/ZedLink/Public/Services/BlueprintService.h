// Copyright Krishna Teja Mekala. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FZedLinkServer;
class UBlueprint;

/**
 * Information about a Blueprint asset.
 */
struct FBlueprintInfo
{
	FString AssetPath;
	FString ClassName;
	FString ParentClassName;
	FString ParentSourceFile;
	int32 ParentSourceLine = 0;
	TArray<FString> Functions;
	TArray<FString> Variables;
	TArray<FString> EventGraphs;
};

/**
 * Service for Blueprint introspection and navigation.
 */
class ZEDLINK_API FBlueprintService
{
public:
	FBlueprintService(TSharedPtr<FZedLinkServer> InServer);
	~FBlueprintService();

	void Initialize();
	void Shutdown();

	/** Query operations */
	TArray<FString> GetAllBlueprintAssets(const FString& Filter = TEXT(""));
	TSharedPtr<FBlueprintInfo> GetBlueprintDetails(const FString& AssetPath);

	/** Navigation operations */
	bool OpenBlueprintInEditor(const FString& AssetPath, const FString& FocusNode = TEXT(""));
	bool FindCppSourceLocation(const FString& ClassName, FString& OutFile, int32& OutLine);

	/** Find Blueprint usages of C++ classes/functions */
	TArray<FString> FindBlueprintReferences(const FString& ClassName);

private:
	void OnBlueprintCompiled();
	void SubscribeToBlueprintEvents();
	void UnsubscribeBlueprintEvents();

	/** Handle incoming requests */
	void HandleRequest(const FString& JsonMessage);

private:
	TWeakPtr<FZedLinkServer> Server;

	FDelegateHandle OnCompiledHandle;
	FDelegateHandle OnMessageHandle;
};
