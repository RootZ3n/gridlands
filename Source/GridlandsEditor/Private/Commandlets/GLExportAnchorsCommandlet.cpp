#include "Commandlets/GLExportAnchorsCommandlet.h"

#include "Anchors/GLAnchorExport.h"
#include "Content/GLContentDefinitions.h"
#include "Content/GLContentRegistry.h"
#include "GridlandsEditor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

int32 UGLExportAnchorsCommandlet::Main(const FString& Params)
{
	const FString RepoRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FGLContentRegistry Registry;
	if (!Registry.LoadRepository(RepoRoot))
	{
		UE_LOG(LogGridlandsEditor, Error, TEXT("ExportAnchors: content has problems; run Tools/data.sh validate"));
		return 1;
	}
	int32 Written = 0, Failed = 0;
	Registry.ForEachEntry([&](const FGLContentEntry& Entry)
	{
		const FGLCellDef* Cell = Entry.Definition.GetPtr<FGLCellDef>();
		if (Entry.Kind != TEXT("cell") || !Cell || Cell->Level.IsEmpty())
		{
			return;
		}
		UWorld* World = GLAnchorExport::LoadMap(Cell->Level);
		if (!World)
		{
			UE_LOG(LogGridlandsEditor, Error, TEXT("ExportAnchors: %s names level %s, which does not load"), *Entry.Id.ToString(), *Cell->Level);
			++Failed;
			return;
		}
		TArray<FString> Problems;
		const FString Text = GLAnchorExport::Render(World, Entry.Id.ToString(), Problems);
		for (const FString& Problem : Problems)
		{
			UE_LOG(LogGridlandsEditor, Error, TEXT("ExportAnchors: %s"), *Problem);
		}
		if (Problems.Num() > 0)
		{
			++Failed;
			return;
		}
		const FString Path = FPaths::Combine(RepoRoot, GLAnchorExport::FilePathForCell(Entry.Id.ToString()));
		FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		UE_LOG(LogGridlandsEditor, Display, TEXT("ExportAnchors: wrote %s"), *GLAnchorExport::FilePathForCell(Entry.Id.ToString()));
		++Written;
	});
	UE_LOG(LogGridlandsEditor, Display, TEXT("ExportAnchors: %d file(s) written, %d failed"), Written, Failed);
	return Failed == 0 ? 0 : 1;
}
