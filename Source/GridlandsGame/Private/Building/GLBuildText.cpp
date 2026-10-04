#include "Building/GLBuildText.h"

#include "Building/GLBuildingSubsystem.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"

FString GLBuildText::StateWord(EGLPreview Preview)
{
	return Preview == EGLPreview::Green ? TEXT("OK") : Preview == EGLPreview::Yellow ? TEXT("LIMIT") : TEXT("NO");
}

FString GLBuildText::StateIcon(EGLPreview Preview)
{
	return Preview == EGLPreview::Green ? TEXT("[+]") : Preview == EGLPreview::Yellow ? TEXT("[!]") : TEXT("[x]");
}

FLinearColor GLBuildText::StateColour(EGLPreview Preview)
{
	return Preview == EGLPreview::Green ? FLinearColor(0.2f, 0.9f, 0.3f) : Preview == EGLPreview::Yellow ? FLinearColor(0.98f, 0.82f, 0.1f) : FLinearColor(0.95f, 0.15f, 0.1f);
}

FString GLBuildText::ItemName(FName Item)
{
	const FGLItemDef* Def = GLContent::Get().Find<FGLItemDef>(Item);
	return Def && !Def->DisplayName.IsEmpty() ? Def->DisplayName : Item.ToString();
}

FString GLBuildText::Explain(const FGLBuildCheck& Check, const FString& BlockingName)
{
	const FGLMaterialDef* Material = GLContent::Get().Find<FGLMaterialDef>(Check.Material);
	const FString MaterialName = Material && !Material->DisplayName.IsEmpty() ? Material->DisplayName : FString(TEXT("its material"));
	switch (Check.Refusal)
	{
	case EGLBuildRefusal::None:
		return Check.Preview == EGLPreview::Yellow
			? FString::Printf(TEXT("At %s's limit: nothing more of %s can stand on it"), *MaterialName, *MaterialName)
			: FString(TEXT("Stands, with margin"));
	case EGLBuildRefusal::UnknownPiece: return TEXT("Not something you can build");
	case EGLBuildRefusal::NotKnown: return TEXT("You don't know how to build this yet");
	case EGLBuildRefusal::MissingItems:
		return FString::Printf(TEXT("Needs %d %s (you can use %d)"), Check.MissingNeeded, *ItemName(Check.MissingItem), Check.MissingHave);
	case EGLBuildRefusal::Overlaps:
		return BlockingName.IsEmpty() ? FString(TEXT("Something is in the way")) : FString::Printf(TEXT("The %s is in the way"), *BlockingName);
	case EGLBuildRefusal::Buried: return TEXT("The ground is in the way (flatten it first)");
	case EGLBuildRefusal::Unsupported: return TEXT("Nothing holds it up");
	case EGLBuildRefusal::OutsideClaim: return TEXT("Too close to another base (base areas may not overlap)");
	}
	return TEXT("");
}

FString GLBuildText::Cost(const FGLCostView& View)
{
	TArray<FString> Parts;
	for (const FGLCostLine& Line : View.Lines)
	{
		Parts.Add(View.Claim.IsNone()
			? FString::Printf(TEXT("%d %s: you %d"), Line.Needed, *ItemName(Line.Item), Line.Personal)
			: FString::Printf(TEXT("%d %s: base %d / you %d"), Line.Needed, *ItemName(Line.Item), Line.InStorage, Line.Personal));
	}
	return FString::Join(Parts, TEXT("  "));
}

FString GLBuildText::Items(const TMap<FName, int32>& Items)
{
	TArray<FName> Keys;
	Items.GetKeys(Keys);
	Keys.Sort(FNameLexicalLess());
	TArray<FString> Parts;
	for (const FName& Key : Keys)
	{
		Parts.Add(FString::Printf(TEXT("%d %s"), Items[Key], *ItemName(Key)));
	}
	return Parts.Num() ? FString::Join(Parts, TEXT(", ")) : FString(TEXT("nothing"));
}
