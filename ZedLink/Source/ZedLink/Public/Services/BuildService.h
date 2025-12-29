// Copyright Krishna Teja Mekala. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_LIVE_CODING
#include "ILiveCodingModule.h"
#endif

class FZedLinkServer;

/**
 * Service for build operations and Live Coding integration.
 */
class ZEDLINK_API FBuildService
{
public:
	FBuildService(TSharedPtr<FZedLinkServer> InServer);
	~FBuildService();

	void Initialize();
	void Shutdown();

	/** Build commands */
	bool TriggerLiveCoding();
	bool CancelLiveCoding();

	/** State queries */
	bool IsLiveCodingEnabled() const;
	bool IsCompiling() const;

private:
	void OnPatchComplete();
	void OnCompilationStarted();
	void OnCompilationFinished(bool bSuccess);
	void BroadcastBuildStatus(const FString& Status, float Progress = 0.0f, const FString& CurrentFile = TEXT(""));

	/** Handle incoming requests */
	void HandleRequest(const FString& JsonMessage);

private:
	TWeakPtr<FZedLinkServer> Server;

#if WITH_LIVE_CODING
	FDelegateHandle OnPatchCompleteHandle;
	FDelegateHandle OnCompilationStartedHandle;
	FDelegateHandle OnCompilationFinishedHandle;
#endif

	FDelegateHandle OnMessageHandle;
	bool bIsCompiling = false;
};
