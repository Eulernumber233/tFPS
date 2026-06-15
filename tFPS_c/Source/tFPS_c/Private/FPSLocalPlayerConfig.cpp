#include "FPSLocalPlayerConfig.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FString FFPSLocalPlayerConfig::GetConfigFilePath()
{
	return FPaths::ProjectSavedDir() / TEXT("PlayerConfig.json");
}

void FFPSLocalPlayerConfig::SaveIdentity(const FString& PlayerName, const FString& IconPath)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("PlayerName"), PlayerName);
	Json->SetStringField(TEXT("PlayerIcon"), IconPath);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(Json, Writer);

	FFileHelper::SaveStringToFile(JsonString, *GetConfigFilePath());
}

bool FFPSLocalPlayerConfig::LoadIdentity(FString& OutPlayerName, FString& OutIconPath)
{
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *GetConfigFilePath()))
		return false;

	TSharedPtr<FJsonObject> Json;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
		return false;

	OutPlayerName = Json->GetStringField(TEXT("PlayerName"));
	OutIconPath = Json->GetStringField(TEXT("PlayerIcon"));
	return true;
}
