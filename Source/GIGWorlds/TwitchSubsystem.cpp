// Fill out your copyright notice in the Description page of Project Settings.

#include "TwitchSubsystem.h"
#include "SocketSubsystem.h"
#include "Runtime/Networking/Public/Networking.h"
#include "string"

void UTwitchSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	//UE_LOG(LogTemp, Warning, TEXT("UTwitchSubsystem::Initialize"));

	FIPv4Endpoint Endpoint(FIPv4Address(127, 0, 0, 1), 6667);
	StreamerListenerSocket = FTcpSocketBuilder(TEXT("TwitchListener"))
		.AsReusable()
		.BoundToEndpoint(Endpoint)
		.Listening(8);

	//Set Buffer Size
	int32 NewSize = 0;
	StreamerListenerSocket->SetReceiveBufferSize(2 * 1024 * 1024, NewSize);

	GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &UTwitchSubsystem::SocketListener, 0.01, true);

	StreamerPartialMessage = TEXT("");
	SendLogin();
}

void UTwitchSubsystem::Deinitialize()
{
	Super::Deinitialize();
	TimerHandle.Invalidate();
	StreamerListenerSocket = nullptr;
	StreamerConnectionSocket = nullptr;
	StreamerPartialMessage = nullptr;
	//UE_LOG(LogTemp, Warning, TEXT("UTwitchSubsystem::Deinitialize"));
}

void UTwitchSubsystem::SendLogin()
{
	//UE_LOG(LogTemp, Warning, TEXT("UTwitchSubsystem::SendLogin"));
	auto ResolveInfo = ISocketSubsystem::Get()->GetHostByName("irc.twitch.tv");
	while (!ResolveInfo->IsComplete());
	if (ResolveInfo->GetErrorCode() != 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Couldn't resolve hostname."));
		return;
	}

	const FInternetAddr* Addr = &ResolveInfo->GetResolvedAddress();
	uint32 OutIP = 0;
	Addr->GetIp(OutIP);
	int32 port = 6667;

	TSharedRef<FInternetAddr> addr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	addr->SetIp(OutIP);
	addr->SetPort(port);

	StreamerListenerSocket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("default"), false);

	Connected = StreamerListenerSocket->Connect(*addr);
	if (!Connected)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to connect."));
		if (StreamerListenerSocket)
			StreamerListenerSocket->Close();
		return;
	}

	//Join as the anonymous user.
	SendString(TEXT("NICK justinfan31617"));
	//Request full viewer information on for each message
	SendString(TEXT("CAP REQ :twitch.tv/tags twitch.tv/commands twitch.tv/membership"));

}

bool UTwitchSubsystem::JoinChannel(const FString& JoinChannelName) const
{
	UE_LOG(LogTemp, Warning, TEXT("UTwitchSubsystem::JoinChannel: %s"), *JoinChannelName);
	SendString(TEXT("JOIN #") + JoinChannelName.ToLower().TrimStartAndEnd());
	return true;
}

void UTwitchSubsystem::SocketListener()
{
	//UE_LOG(LogTemp, Warning, TEXT("UTwitchSubsystem::SocketListener"));
	TArray<uint8> ReceivedData;
	uint32 Size;
	bool Received = false;
	while (StreamerListenerSocket->HasPendingData(Size))
	{
		Received = true;
		ReceivedData.SetNumUninitialized(FMath::Min(Size, 65507u));

		int32 Read = 0;
		StreamerListenerSocket->Recv(ReceivedData.GetData(), ReceivedData.Num(), Read);
	}
	if (Received)
	{
		const std::string cstr(reinterpret_cast<const char*>(ReceivedData.GetData()), ReceivedData.Num());
		FString fs(cstr.c_str());

		ParseMessage(fs);
	}
}


bool UTwitchSubsystem::SendString(const FString msg) const
{
	FString serialized = msg + TEXT("\r\n");
	TCHAR* serializedChar = serialized.GetCharArray().GetData();
	int32 size = FCString::Strlen(serializedChar);
	int32 sent = 0;

	return StreamerListenerSocket->Send((uint8*)TCHAR_TO_UTF8(serializedChar), size, sent);
}


void UTwitchSubsystem::ParseMessage(FString TwitchResponse)
{


	TArray<FString> MessageLines;

	TwitchResponse.ParseIntoArrayLines(MessageLines);

	//Add The Last message onto the first message, if it's a partial, otherwise process because its a full message.

	if (StreamerPartialMessage.StartsWith(TEXT("@badge-info"), ESearchCase::CaseSensitive))
	{
		//Check to see if it's not a full message by checking it's start characters
		if (!MessageLines[0].StartsWith(TEXT("@"), ESearchCase::CaseSensitive)
			&& !MessageLines[0].StartsWith(TEXT(":justinfan"), ESearchCase::CaseSensitive)
			&& !MessageLines[0].StartsWith(TEXT(":tmi.twitch.tv"), ESearchCase::CaseSensitive)
			&& !MessageLines[0].StartsWith(TEXT("PING :tmi.twitch.tv"), ESearchCase::CaseSensitive)
			&& !MessageLines[0].Contains(TEXT("JOIN"), ESearchCase::CaseSensitive)
			&& !MessageLines[0].Contains(TEXT("PART"), ESearchCase::CaseSensitive)
			)
		{
			MessageLines[0] = StreamerPartialMessage + MessageLines[0];
		}
		else
		{
			ParseMessageLines(StreamerPartialMessage);
		}
	}

	//Remove the last message and save it until next cycle in case it's a partial message.
	if (MessageLines.Last().StartsWith(TEXT("@badge-info"), ESearchCase::CaseSensitive) && MessageLines.Num() > 1)
	{
		StreamerPartialMessage = MessageLines.Last();
		MessageLines.RemoveAt(MessageLines.Num() - 1);
	}

	//Cycle through each message, except the last one, as this has been stored in the partial message.
	int32 MaxMessages = MessageLines.Num();
	for (size_t i = 0; i < MaxMessages; i++)
	{

		ParseMessageLines(MessageLines[i]);

	}

}

void UTwitchSubsystem::ParseMessageLines(FString IndividualMessage)
{
	TArray<FString> MessageParameters;
	TArray<FString> NamedPairs;
	TArray<FString> MessageParts;
	TArray<FString> MessageMeta;
	FString ChannelName;
	FString DisplayName;
	FString ChatText;
	FString Username;
	FString Badges;

	FString tmp;


	IndividualMessage.ParseIntoArray(MessageParameters, TEXT(";"));

	//All chat messages begin with the @ character, if not return.
	if (!MessageParameters[0].StartsWith(TEXT("@"), ESearchCase::CaseSensitive))
	{
		//Catch Connection Connection Ping and Send Response.
		if (MessageParameters[0].StartsWith(TEXT("PING :tmi.twitch.tv"), ESearchCase::CaseSensitive))
		{
			SendString(TEXT("PONG :tmi.twitch.tv"));
		}

		return;
	}
	//Cycle through each parameter
	for (FString IndividualParameters : MessageParameters)
	{
		IndividualParameters.ParseIntoArray(NamedPairs, TEXT("="));

		if (NamedPairs[0] == TEXT("@badge-info"))
		{

		}
		else if (NamedPairs[0] == TEXT("badges"))
		{

		}
		else if (NamedPairs[0] == TEXT("display-name"))
		{
			//TODO:  Need to address broken lines here as well.
			DisplayName = NamedPairs[1];
		}
		else if (NamedPairs[0] == TEXT("color"))
		{

		}
		else if (NamedPairs[0] == TEXT("emotes"))
		{

		}
		else if (NamedPairs[0] == TEXT("flags"))
		{

		}
		else if (NamedPairs[0] == TEXT("id"))
		{

		}
		else if (NamedPairs[0] == TEXT("login"))
		{

		}
		else if (NamedPairs[0] == TEXT("mod"))
		{

		}
		else if (NamedPairs[0] == TEXT("msg-id"))
		{

		}
		else if (NamedPairs[0] == TEXT("msg-param-cumulative-months"))
		{

		}
		else if (NamedPairs[0] == TEXT("msg-param-months"))
		{

		}
		else if (NamedPairs[0] == TEXT("msg-param-multimonth-duration"))
		{

		}
		else if (NamedPairs[0] == TEXT("msg-param-multimonth-tenure"))
		{

		}
		else if (NamedPairs[0] == TEXT("msg-param-should-share-streak"))
		{

		}
		else if (NamedPairs[0] == TEXT("msg-param-streak-months"))
		{

		}
		else if (NamedPairs[0] == TEXT("msg-param-sub-plan-name"))
		{

		}
		else if (NamedPairs[0] == TEXT("msg-param-sub-plan"))
		{

		}
		else if (NamedPairs[0] == TEXT("msg-param-was-gifted"))
		{

		}
		else if (NamedPairs[0] == TEXT("room-id"))
		{

		}
		else if (NamedPairs[0] == TEXT("subscriber"))
		{

		}
		else if (NamedPairs[0] == TEXT("system-msg"))
		{

		}
		else if (NamedPairs[0] == TEXT("tmi-sent-ts"))
		{

		}
		else if (NamedPairs[0] == TEXT("turbo"))
		{

		}
		else if (NamedPairs[0] == TEXT("user-id"))
		{

		}
		else if (NamedPairs[0] == TEXT("user-type"))
		{
			IndividualParameters.ParseIntoArray(MessageParts, TEXT(":"));
			if (MessageParts.Num() >= 2)
			{
				MessageParts[1].ParseIntoArrayWS(MessageMeta);
				if (MessageMeta.Num() == 3 && MessageMeta[1] == TEXT("PRIVMSG"))
				{
					if (MessageParts.Num() > 2)
					{
						MessageParts[1].Split(TEXT("#"), &tmp, &ChannelName);
						ChatText = MessageParts[2];
					}
					MessageMeta[0].Split(TEXT("!"), &Username, &tmp);

					OnMessageReceived.Broadcast(ChannelName.TrimStartAndEnd(), DisplayName.TrimStartAndEnd(), ChatText);
					continue;
				}
			}

		}


	}
	//TArray<FString> parts;
	//fs.ParseIntoArray(parts, TEXT(":"));
	//TArray<FString> meta;
	//parts[0].ParseIntoArrayWS(meta);
	//if (parts.Num() >= 2)
	//{
	//	if (meta[0] == TEXT("PING"))
	//	{
	//		SendString(TEXT("PONG :tmi.twitch.tv"));
	//	}
	//	else if (meta.Num() == 3 && meta[1] == TEXT("PRIVMSG"))
	//	{
	//		FString message = parts[1];
	//		if (parts.Num() > 2)
	//		{
	//			for (int i = 2; i < parts.Num(); i++)
	//			{
	//				message += TEXT(":") + parts[i];
	//			}
	//		}
	//		FString username;
	//		FString tmp;
	//		meta[0].Split(TEXT("!"), &username, &tmp);
	//		ReceivedChatMessage(username, message);
	//		continue;
	//	}
	//}

}

// USERNOTICE
/*
@badge-info=subscriber/5;
badges=subscriber/3,premium/1;
client-nonce=8da74ffbf2873291d91e767693466e9f;
color=#FFE82D;
display-name=malou_cs;
emotes=;
flags=;
id=134aa2f8-00c4-4ad8-b33c-2ed03bc977c4;
mod=0;
room-id=71092938;
subscriber=1;
tmi-sent-ts=1612241788969;
turbo=0;
user-id=272114009;
user-type= :malou_cs!malou_cs@malou_cs.tmi.twitch.tv PRIVMSG #xqcow :OMEGALUL
*/