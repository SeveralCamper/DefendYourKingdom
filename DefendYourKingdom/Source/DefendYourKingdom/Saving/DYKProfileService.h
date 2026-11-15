#pragma once

#include "CoreMinimal.h"

struct FDYKPlayerProfileSaveData;

// Domain callers supply a local command; storage and USaveGame stay behind this boundary.
// A successful command is durable. Reusing its ID never applies it a second time.
class DEFENDYOURKINGDOM_API IDYKProfileService
{
public:
	virtual ~IDYKProfileService() = default;
	// Borrowed read-only view: copy data before retaining it across a successful command.
	virtual const FDYKPlayerProfileSaveData* GetProfileSnapshot() const = 0;
	virtual int64 GetRevision() const = 0;
	virtual bool ApplyProfileCommand(const FGuid& CommandId,
		TFunctionRef<bool(FDYKPlayerProfileSaveData&)> Command) = 0;
};
