// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TwitchSubsystem.generated.h"

/**
 * 
 */
UCLASS(DisplayName = "Twitch")
class GIGWORLDS_API UTwitchSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:

	// Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// End USubsystem

	UFUNCTION(BlueprintCallable, Category = OnlineServices)
		void SendLogin();

	UFUNCTION(BlueprintCallable, Category = OnlineServices)
		bool JoinChannel(const FString& ChannelName) const;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMessageReceived, FString, ChannelName, FString, DisplayName, FString, MessageText);
	

	UPROPERTY(BlueprintAssignable, Category = OnlineServices)
		FMessageReceived OnMessageReceived;


private:
	FSocket* StreamerListenerSocket;
	FSocket* StreamerConnectionSocket;
	FTimerHandle TimerHandle;
	FString StreamerPartialMessage;
	bool Connected;

	void SocketListener();
	void ParseMessage(FString msg);

	void ParseMessageLines(FString MessageLines);

	bool SendString(const FString msg) const;
};
