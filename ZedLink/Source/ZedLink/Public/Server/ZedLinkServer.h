// Copyright Krishna Teja Mekala. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnZedLinkMessageReceived, const FString& /* JsonMessage */);
DECLARE_MULTICAST_DELEGATE(FOnZedLinkClientConnected);
DECLARE_MULTICAST_DELEGATE(FOnZedLinkClientDisconnected);

/**
 * TCP Server for communication with Zed editor's helper binary.
 * Uses JSON-RPC 2.0 protocol with 4-byte length-prefixed messages.
 */
class ZEDLINK_API FZedLinkServer : public FRunnable, public TSharedFromThis<FZedLinkServer>
{
public:
	FZedLinkServer();
	virtual ~FZedLinkServer();

	/** Start the server on the specified port */
	bool Start(int32 Port = 21567);

	/** Stop the server and close all connections */
	void Stop();

	/** Check if server is running */
	bool IsRunning() const { return bIsRunning; }

	/** Check if a client is connected */
	bool IsClientConnected() const { return bIsConnected; }

	/** Send a JSON message to the connected client */
	void SendMessage(const FString& JsonMessage);

	/** Send a JSON-RPC notification (no response expected) */
	void SendNotification(const FString& Method, const TSharedPtr<FJsonObject>& Params);

	/** Send a JSON-RPC response to a request */
	void SendResponse(const FString& RequestId, const TSharedPtr<FJsonValue>& Result);

	/** Send a JSON-RPC error response */
	void SendError(const FString& RequestId, int32 Code, const FString& Message);

	/** Event delegates */
	FOnZedLinkMessageReceived OnMessageReceived;
	FOnZedLinkClientConnected OnClientConnected;
	FOnZedLinkClientDisconnected OnClientDisconnected;

protected:
	// FRunnable interface
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override { bShouldStop = true; }
	virtual void Exit() override;

private:
	/** Create and bind the listen socket */
	bool CreateListenSocket(int32 Port);

	/** Accept incoming client connection */
	void AcceptConnection();

	/** Handle data from connected client */
	void HandleClientData();

	/** Read a complete message from socket */
	bool ReceiveMessage(FSocket* Socket, FString& OutMessage);

	/** Send a message with length prefix */
	bool SendMessageInternal(FSocket* Socket, const FString& Message);

	/** Process received JSON-RPC message */
	void ProcessMessage(const FString& JsonMessage);

	/** Handle JSON-RPC request */
	void HandleRequest(const FString& Id, const FString& Method, const TSharedPtr<FJsonObject>& Params);

	/** Close client connection */
	void CloseClientConnection();

private:
	FSocket* ListenSocket = nullptr;
	FSocket* ClientSocket = nullptr;
	FRunnableThread* Thread = nullptr;

	FThreadSafeBool bShouldStop;
	FThreadSafeBool bIsRunning;
	FThreadSafeBool bIsConnected;

	int32 CurrentPort = 0;

	FCriticalSection SendLock;
	TQueue<FString, EQueueMode::Mpsc> OutgoingMessages;

	/** Buffer for partial message reception */
	TArray<uint8> ReceiveBuffer;
};
