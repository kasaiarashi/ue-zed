// Copyright Krishna Teja Mekala. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FZedLinkServer;

/**
 * Service for controlling Play-In-Editor (PIE) sessions from Zed.
 */
class ZEDLINK_API FPlayService
{
public:
	FPlayService(TSharedPtr<FZedLinkServer> InServer);
	~FPlayService();

	void Initialize();
	void Shutdown();

	/** PIE control methods */
	bool StartPIE(const FString& Mode = TEXT("PIE"), int32 PlayerCount = 1, bool bDedicatedServer = false);
	bool StopPIE();
	bool PausePIE();
	bool ResumePIE();
	bool TogglePause();

	/** State queries */
	bool IsPlaying() const;
	bool IsPaused() const;
	bool IsSimulating() const;
	FString GetCurrentState() const;

private:
	void OnPIEStarted(bool bSimulating);
	void OnPIEEnded(bool bSimulating);
	void BroadcastStateChange();

	/** Handle incoming requests */
	void HandleRequest(const FString& JsonMessage);

private:
	TWeakPtr<FZedLinkServer> Server;

	FDelegateHandle OnStartedHandle;
	FDelegateHandle OnEndedHandle;
	FDelegateHandle OnMessageHandle;
};
