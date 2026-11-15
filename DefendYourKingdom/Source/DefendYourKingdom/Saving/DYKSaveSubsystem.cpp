#include "DYKSaveSubsystem.h"

#include "DYKProfileSaveGame.h"
#include "../DefendYourKingdom.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/ObjectVersion.h"

namespace
{
	constexpr uint32 RecordMagic = 0x44594B50;
	constexpr uint32 RecordVersion = 1;
	constexpr int32 MetadataSize = 24;
	constexpr int32 DigestSize = 20;
	constexpr int32 HeaderSize = MetadataSize + DigestSize;
	constexpr int32 MaxRecordSize = 16 * 1024 * 1024;

	bool ReadBoundedString(FMemoryReader& Reader, FString& Value, int32 MaxCharacters)
	{
		const int64 Start = Reader.Tell();
		int32 Count = 0;
		Reader << Count;
		const int64 Characters = FMath::Abs(static_cast<int64>(Count));
		const int64 ByteCount = Characters * (Count < 0 ? 2 : 1);
		if (Reader.IsError() || Characters > MaxCharacters || ByteCount > Reader.TotalSize() - Reader.Tell())
		{
			return false;
		}
		Reader.Seek(Start);
		Reader << Value;
		return !Reader.IsError();
	}

	bool ReadLegacyHeader(const TArray<uint8>& Bytes, int32& BodyOffset)
	{
		// Preflight the actual UE5.5 GVAS header before UE's permissive loader:
		// malformed custom-version formats can assert, and string/count fields
		// must be bounded before any allocation. Editor and Game may register
		// different custom versions, so their header bytes need not be identical.
		FMemoryReader Reader(Bytes, true);
		uint32 Magic = 0, Changelist = 0;
		int32 Format = 0, UE4Version = 0, UE5Version = 0;
		uint16 Major = 0, Minor = 0, Patch = 0;
		Reader << Magic << Format << UE4Version << UE5Version << Major << Minor << Patch << Changelist;
		if (Reader.IsError() || Magic != 0x53415647 || Format != 3 || Major != 5 || Minor != 5
			|| UE4Version != GPackageFileUEVersion.FileVersionUE4 || UE5Version != GPackageFileUEVersion.FileVersionUE5)
		{
			return false;
		}
		FString Branch, ClassName;
		if (!ReadBoundedString(Reader, Branch, 2048))
		{
			return false;
		}
		int32 CustomFormat = 0, CustomCount = 0;
		Reader << CustomFormat << CustomCount;
		constexpr int32 CustomVersionSize = 20; // FGuid + int32, Optimized format.
		if (Reader.IsError() || CustomFormat != 3 || CustomCount < 0 || CustomCount > 4096
			|| static_cast<int64>(CustomCount) * CustomVersionSize > Reader.TotalSize() - Reader.Tell())
		{
			return false;
		}
		Reader.Seek(Reader.Tell() + static_cast<int64>(CustomCount) * CustomVersionSize);
		if (!ReadBoundedString(Reader, ClassName, 512)
			|| ClassName != UDYKProfileSaveGame::StaticClass()->GetPathName())
		{
			return false;
		}
		BodyOffset = static_cast<int32>(Reader.Tell());
		return true;
	}

	bool HasLegacyFraming(const TArray<uint8>& Bytes)
	{
		int32 BodyOffset = 0;
		const uint8 EndTag[] = { 5, 0, 0, 0, 'N', 'o', 'n', 'e', 0, 0, 0, 0, 0 };
		return ReadLegacyHeader(Bytes, BodyOffset) && Bytes.Num() - BodyOffset >= sizeof(EndTag)
			&& FMemory::Memcmp(Bytes.GetData() + Bytes.Num() - sizeof(EndTag), EndTag, sizeof(EndTag)) == 0;
	}

	bool IsCompleteLegacyProfile(const TArray<uint8>& Original, UDYKProfileSaveGame* Profile)
	{
		TArray<uint8> Canonical;
		if (!UGameplayStatics::SaveGameToMemory(Profile, Canonical))
		{
			return false;
		}
		int32 OriginalBody = 0, CanonicalBody = 0;
		if (!ReadLegacyHeader(Original, OriginalBody) || !ReadLegacyHeader(Canonical, CanonicalBody))
		{
			return false;
		}
		const auto Matches = [&]()
		{
			return Canonical.Num() - CanonicalBody == Original.Num() - OriginalBody
				&& FMemory::Memcmp(Canonical.GetData() + CanonicalBody,
					Original.GetData() + OriginalBody, Original.Num() - OriginalBody) == 0;
		};
		if (Matches())
		{
			return true;
		}
		if (!Profile->AppliedCommandIds.IsEmpty())
		{
			return false;
		}
		// Schema 1 originally had no ledger. It is the only optional property in
		// that supported snapshot; missing/truncated gameplay fields must fail.
		TArray<uint8> LedgerTag;
		FMemoryWriter TagWriter(LedgerTag);
		FString Name = TEXT("AppliedCommandIds");
		TagWriter << Name;
		constexpr int32 EndTagSize = 13;
		for (int32 Offset = Canonical.Num() - EndTagSize - LedgerTag.Num(); Offset >= CanonicalBody; --Offset)
		{
			if (FMemory::Memcmp(Canonical.GetData() + Offset, LedgerTag.GetData(), LedgerTag.Num()) == 0)
			{
				Canonical.RemoveAt(Offset, Canonical.Num() - EndTagSize - Offset);
				return Matches();
			}
		}
		return false;
	}

	enum class ERecordState { Missing, Valid, Corrupt, Unsupported };
	struct FRecord
	{
		ERecordState State = ERecordState::Missing;
		int64 Revision = 0;
		UDYKProfileSaveGame* Profile = nullptr;
	};

	void HashRecord(const TArray<uint8>& Data, uint8* Digest)
	{
		FSHA1 Hash;
		Hash.Update(Data.GetData(), MetadataSize);
		Hash.Update(Data.GetData() + HeaderSize, Data.Num() - HeaderSize);
		Hash.Final();
		Hash.GetHash(Digest);
	}

	FRecord ReadRecord(const FString& Path)
	{
		FRecord Record;
		const int64 FileSize = IFileManager::Get().FileSize(*Path);
		if (FileSize == INDEX_NONE)
		{
			Record.State = IFileManager::Get().DirectoryExists(*Path) ? ERecordState::Corrupt : ERecordState::Missing;
			return Record;
		}
		Record.State = ERecordState::Corrupt;
		TArray<uint8> Bytes;
		if (FileSize < HeaderSize || FileSize > MaxRecordSize || !FFileHelper::LoadFileToArray(Bytes, *Path))
		{
			return Record;
		}
		FMemoryReader Reader(Bytes);
		uint32 Magic = 0, Format = 0;
		int32 Schema = 0, PayloadSize = 0;
		Reader << Magic << Format << Record.Revision << Schema << PayloadSize;
		if (Magic != RecordMagic || PayloadSize <= 0 || PayloadSize != Bytes.Num() - HeaderSize || Record.Revision <= 0)
		{
			return Record;
		}
		uint8 Digest[DigestSize];
		HashRecord(Bytes, Digest);
		if (FMemory::Memcmp(Digest, Bytes.GetData() + MetadataSize, DigestSize) != 0)
		{
			return Record;
		}
		if (Format != RecordVersion || Schema != UDYKProfileSaveGame::CurrentSchemaVersion)
		{
			Record.State = ERecordState::Unsupported;
			return Record;
		}
		TArray<uint8> Payload;
		Payload.Append(Bytes.GetData() + HeaderSize, PayloadSize);
		Record.Profile = Cast<UDYKProfileSaveGame>(UGameplayStatics::LoadGameFromMemory(Payload));
		if (!Record.Profile)
		{
			return Record;
		}
		if (Record.Profile->SchemaVersion != Schema)
		{
			Record.State = ERecordState::Unsupported;
			return Record;
		}
		Record.State = ERecordState::Valid;
		return Record;
	}
}

void UDYKSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SlotPrefix = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames/DYK_LocalProfile"));
	LoadOrCreateProfile();
}

FString UDYKSaveSubsystem::GetSavePath(int32 Slot) const
{
	return SlotPrefix + (Slot == 0 ? TEXT("_A.sav") : TEXT("_B.sav"));
}

bool UDYKSaveSubsystem::SaveProfile()
{
	if (!IsInGameThread() || bWriting || !ActiveProfile
		|| (LoadStatus != EDYKProfileLoadStatus::Loaded && LoadStatus != EDYKProfileLoadStatus::Created
			&& LoadStatus != EDYKProfileLoadStatus::Recovered))
	{
		return false;
	}
	TGuardValue<bool> Guard(bWriting, true);
	return WriteProfile(ActiveProfile);
}

bool UDYKSaveSubsystem::WriteProfile(UDYKProfileSaveGame* Candidate)
{
	if (Candidate->SchemaVersion != UDYKProfileSaveGame::CurrentSchemaVersion || Revision == MAX_int64)
	{
		LoadError = TEXT("Unsupported profile schema or exhausted revision counter; nothing was written.");
		return false;
	}
	TArray<uint8> Payload;
	if (!UGameplayStatics::SaveGameToMemory(Candidate, Payload) || Payload.IsEmpty()
		|| Payload.Num() > MaxRecordSize - HeaderSize)
	{
		LoadError = TEXT("The local profile could not be serialized within its size limit.");
		return false;
	}
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	uint32 Magic = RecordMagic, Format = RecordVersion;
	int64 NextRevision = Revision + 1;
	int32 Schema = Candidate->SchemaVersion, PayloadSize = Payload.Num();
	Writer << Magic << Format << NextRevision << Schema << PayloadSize;
	Bytes.AddZeroed(DigestSize);
	Bytes.Append(Payload);
	HashRecord(Bytes, Bytes.GetData() + MetadataSize);

	const int32 TargetSlot = ActiveSlot == 0 ? 1 : 0;
	const FString Target = GetSavePath(TargetSlot);
	const FString Temporary = Target + TEXT(".tmp");
	IFileManager& Files = IFileManager::Get();
	if (!Files.MakeDirectory(*FPaths::GetPath(Target), true))
	{
		LoadError = TEXT("The local save directory could not be created.");
		return false;
	}
	const FRecord Existing = ReadRecord(Target);
	if (Existing.State == ERecordState::Unsupported)
	{
		LoadError = TEXT("A save with an unsupported version was left untouched.");
		return false;
	}
	// Bounded immediate retry uses the same serialized snapshot and revision.
	for (int32 Attempt = 0; Attempt < 2; ++Attempt)
	{
		if (!FFileHelper::SaveArrayToFile(Bytes, *Temporary))
		{
			continue;
		}
		const FRecord Verified = ReadRecord(Temporary);
		if (Verified.State != ERecordState::Valid || Verified.Revision != NextRevision)
		{
			continue;
		}
		if (Existing.State == ERecordState::Corrupt && Files.FileExists(*Target))
		{
			const FString Preserved = Target + TEXT(".corrupt-") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
			if (!Files.Move(*Preserved, *Target, false, false, false, true))
			{
				break;
			}
			UE_LOG(LogDYK, Warning, TEXT("Damaged local save preserved at %s"), *Preserved);
		}
		// The other slot remains intact. Unconfirmed .tmp files are ignored at startup.
		if (Files.Move(*Target, *Temporary, true, false, false, true))
		{
			ActiveProfile = Candidate;
			ActiveSlot = TargetSlot;
			Revision = NextRevision;
			LoadError.Reset();
			return true;
		}
	}
	LoadError = TEXT("The local profile write failed after two attempts; the confirmed profile was retained.");
	return false;
}

bool UDYKSaveSubsystem::ApplyProfileCommand(const FGuid& CommandId,
	TFunctionRef<bool(FDYKPlayerProfileSaveData&)> Command)
{
	if (!IsInGameThread() || bWriting || !ActiveProfile || !CommandId.IsValid()
		|| (LoadStatus != EDYKProfileLoadStatus::Loaded && LoadStatus != EDYKProfileLoadStatus::Created
			&& LoadStatus != EDYKProfileLoadStatus::Recovered))
	{
		return false;
	}
	if (ActiveProfile->AppliedCommandIds.Contains(CommandId))
	{
		LoadError.Reset();
		return true;
	}
	TGuardValue<bool> Guard(bWriting, true);
	UDYKProfileSaveGame* Candidate = DuplicateObject<UDYKProfileSaveGame>(ActiveProfile, this);
	if (!Command(Candidate->Profile))
	{
		LoadError = TEXT("The local profile command was rejected; no changes were committed.");
		return false;
	}
	Candidate->AppliedCommandIds.Add(CommandId);
	return WriteProfile(Candidate);
}

const UDYKProfileSaveGame* UDYKSaveSubsystem::GetActiveProfile() const { return ActiveProfile; }
const FDYKPlayerProfileSaveData* UDYKSaveSubsystem::GetProfileSnapshot() const
{
	return ActiveProfile ? &ActiveProfile->Profile : nullptr;
}
int64 UDYKSaveSubsystem::GetRevision() const { return Revision; }
EDYKProfileLoadStatus UDYKSaveSubsystem::GetLoadStatus() const { return LoadStatus; }
const FString& UDYKSaveSubsystem::GetLoadError() const { return LoadError; }

void UDYKSaveSubsystem::LoadOrCreateProfile()
{
	ActiveProfile = nullptr;
	Revision = 0;
	ActiveSlot = INDEX_NONE;
	LoadError.Reset();
	const FRecord Records[] = { ReadRecord(GetSavePath(0)), ReadRecord(GetSavePath(1)) };
	if (Records[0].State == ERecordState::Unsupported || Records[1].State == ERecordState::Unsupported)
	{
		LoadStatus = EDYKProfileLoadStatus::UnsupportedVersion;
		LoadError = TEXT("A local save uses an unsupported version; all files were left untouched.");
		return;
	}
	for (int32 Slot = 0; Slot < 2; ++Slot)
	{
		if (Records[Slot].State == ERecordState::Valid && Records[Slot].Revision > Revision)
		{
			ActiveProfile = Records[Slot].Profile;
			ActiveSlot = Slot;
			Revision = Records[Slot].Revision;
		}
	}
	if (ActiveProfile)
	{
		const bool bRecovered = Records[0].State == ERecordState::Corrupt || Records[1].State == ERecordState::Corrupt;
		LoadStatus = bRecovered ? EDYKProfileLoadStatus::Recovered : EDYKProfileLoadStatus::Loaded;
		if (bRecovered)
		{
			UE_LOG(LogDYK, Warning, TEXT("Recovered local profile revision %lld from slot %d; damaged copy left untouched."), Revision, ActiveSlot);
		}
		else if (Records[0].State == ERecordState::Missing || Records[1].State == ERecordState::Missing)
		{
			// Complete a creation/migration interrupted between the two initial writes.
			if (!SaveProfile())
			{
				ActiveProfile = nullptr;
				LoadStatus = EDYKProfileLoadStatus::CreationFailed;
			}
		}
		return;
	}
	if (Records[0].State != ERecordState::Missing || Records[1].State != ERecordState::Missing)
	{
		LoadStatus = EDYKProfileLoadStatus::Corrupt;
		LoadError = TEXT("No valid local save copy is available; original files were left untouched.");
		return;
	}

	const FString LegacyPath = SlotPrefix + TEXT(".sav");
	if (IFileManager::Get().FileExists(*LegacyPath))
	{
		TArray<uint8> Legacy;
		const int64 LegacySize = IFileManager::Get().FileSize(*LegacyPath);
		if (LegacySize > 0 && LegacySize <= MaxRecordSize && FFileHelper::LoadFileToArray(Legacy, *LegacyPath)
			&& HasLegacyFraming(Legacy))
		{
			ActiveProfile = Cast<UDYKProfileSaveGame>(UGameplayStatics::LoadGameFromMemory(Legacy));
		}
		if (!ActiveProfile)
		{
			LoadStatus = EDYKProfileLoadStatus::Corrupt;
			LoadError = TEXT("The legacy local profile is damaged; the original file was left untouched.");
			return;
		}
		if (ActiveProfile->SchemaVersion != UDYKProfileSaveGame::CurrentSchemaVersion)
		{
			ActiveProfile = nullptr;
			LoadStatus = EDYKProfileLoadStatus::UnsupportedVersion;
			LoadError = TEXT("The legacy profile schema is unsupported; the original file was left untouched.");
			return;
		}
		if (!IsCompleteLegacyProfile(Legacy, ActiveProfile))
		{
			ActiveProfile = nullptr;
			LoadStatus = EDYKProfileLoadStatus::Corrupt;
			LoadError = TEXT("The legacy profile is incomplete or is not a supported canonical snapshot; the original was left untouched.");
			return;
		}
		LoadStatus = EDYKProfileLoadStatus::Loaded;
	}
	else
	{
		if (IFileManager::Get().DirectoryExists(*LegacyPath))
		{
			LoadStatus = EDYKProfileLoadStatus::Corrupt;
			LoadError = TEXT("The legacy save path is unreadable; no profile was created.");
			return;
		}
		ActiveProfile = NewObject<UDYKProfileSaveGame>(this);
		LoadStatus = EDYKProfileLoadStatus::Created;
	}
	// Migration creates a new container, never modifies the original single-file save.
	if (!SaveProfile() || !SaveProfile())
	{
		ActiveProfile = nullptr;
		LoadStatus = EDYKProfileLoadStatus::CreationFailed;
	}
}
