#include "Building/GLBuildCatalog.h"

#include "Content/GLContentDefinitions.h"
#include "Content/GLContentRegistry.h"

FName GLBuildCatalog::CategoryOf(const FGLContentRegistry& Content, FName Piece)
{
	const FGLBuildPieceDef* Def = Content.Find<FGLBuildPieceDef>(Piece);
	if (!Def || !Def->Buildable)
	{
		return NAME_None;
	}
	if (!Def->Category.IsNone() && Content.Find<FGLBuildCategoryDef>(Def->Category))
	{
		return Def->Category;
	}
	FName Found;
	int32 FoundOrder = TNumericLimits<int32>::Max();
	Content.ForEachEntry([&](const FGLContentEntry& Entry)
	{
		const FGLBuildCategoryDef* Category = Entry.Definition.GetPtr<FGLBuildCategoryDef>();
		// The lowest order wins if data ever let a role fall back twice (CAT-1 refuses that): deterministic either way.
		if (Category && Category->FallbackRoles.Contains(Def->Role) && Category->Order < FoundOrder)
		{
			Found = Entry.Id;
			FoundOrder = Category->Order;
		}
	});
	return Found.IsNone() ? OtherCategory : Found;
}

TArray<FGLCatalogCategory> GLBuildCatalog::Build(const FGLContentRegistry& Content)
{
	TArray<FGLCatalogCategory> Out;
	Content.ForEachEntry([&Out](const FGLContentEntry& Entry)
	{
		if (const FGLBuildCategoryDef* Category = Entry.Definition.GetPtr<FGLBuildCategoryDef>())
		{
			Out.Add({ Entry.Id, Category->DisplayName, Category->Order, {} });
		}
	});
	Out.Sort([](const FGLCatalogCategory& A, const FGLCatalogCategory& B) { return A.Order != B.Order ? A.Order < B.Order : A.Id.LexicalLess(B.Id); });
	TArray<FName> Pieces;
	Content.ForEachEntry([&Pieces](const FGLContentEntry& Entry)
	{
		const FGLBuildPieceDef* Def = Entry.Definition.GetPtr<FGLBuildPieceDef>();
		if (Def && Def->Buildable)
		{
			Pieces.Add(Entry.Id);
		}
	});
	auto Name = [&Content](FName Id) { const FGLBuildPieceDef* Def = Content.Find<FGLBuildPieceDef>(Id); return Def ? Def->DisplayName : Id.ToString(); };
	Pieces.Sort([&Name](FName A, FName B)
	{
		const FString NA = Name(A), NB = Name(B);
		return NA != NB ? NA < NB : A.LexicalLess(B);
	});
	for (const FName& Piece : Pieces)
	{
		const FName Category = CategoryOf(Content, Piece);
		FGLCatalogCategory* Into = Out.FindByPredicate([Category](const FGLCatalogCategory& C) { return C.Id == Category; });
		if (!Into)
		{
			Into = &Out.Add_GetRef({ OtherCategory, TEXT("Other"), TNumericLimits<int32>::Max(), {} });
		}
		Into->Pieces.Add(Piece);
	}
	Out.RemoveAll([](const FGLCatalogCategory& C) { return C.Pieces.Num() == 0; });
	return Out;
}
