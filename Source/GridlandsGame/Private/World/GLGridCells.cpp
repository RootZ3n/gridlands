#include "World/GLGridCells.h"

#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"

FName GLGridCells::CellAt(const FVector2D& WorldCm)
{
	FName Found = NAME_None;
	GLContent::Get().ForEachEntry([&](const FGLContentEntry& Entry)
	{
		const FGLCellDef* Cell = Entry.Definition.GetPtr<FGLCellDef>();
		if (Cell && Cell->SizeMetres > 0.0 && Cell->ContainsCm(WorldCm))
		{
			Found = Entry.Id;
		}
	});
	return Found;
}

TArray<FName> GLGridCells::AllCells()
{
	TArray<FName> Cells;
	GLContent::Get().ForEachEntry([&](const FGLContentEntry& Entry)
	{
		const FGLCellDef* Cell = Entry.Definition.GetPtr<FGLCellDef>();
		if (Cell && Cell->SizeMetres > 0.0)
		{
			Cells.Add(Entry.Id);
		}
	});
	Cells.Sort(FNameLexicalLess());
	return Cells;
}

double GLGridCells::DistanceToCell(const FGLCellDef& Cell, const FVector2D& WorldCm)
{
	const double Half = Cell.SizeMetres * 50.0;
	const FVector2D L = WorldCm - Cell.CentreCm();
	const double DX = FMath::Max(0.0, FMath::Abs(L.X) - Half);
	const double DY = FMath::Max(0.0, FMath::Abs(L.Y) - Half);
	return FMath::Sqrt(DX * DX + DY * DY);
}
