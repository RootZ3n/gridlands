#include "Commandlets/GLImportArtCommandlet.h"

#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "GridlandsEditor.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "IAssetTools.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionNoise.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSceneTexture.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace GLArt
{
	const TCHAR* MaterialsPath = TEXT("/Game/Gridlands/Art/Materials");
	const TCHAR* MeshesPath = TEXT("/Game/Gridlands/Art/Meshes");

	/** A fresh material asset in its own package (overwritten every run: the commandlet is the source). */
	UMaterial* NewMaterial(const FString& Name, UPackage*& OutPackage)
	{
		const FString PackageName = FString(MaterialsPath) / Name;
		const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		if (IFileManager::Get().FileExists(*Filename))
		{
			IFileManager::Get().Delete(*Filename);
		}
		OutPackage = CreatePackage(*PackageName);
		return NewObject<UMaterial>(OutPackage, *Name, RF_Public | RF_Standalone);
	}

	template <typename T>
	T* Add(UMaterial* M)
	{
		T* E = NewObject<T>(M);
		M->GetExpressionCollection().AddExpression(E);
		return E;
	}

	UMaterialExpressionVectorParameter* Vec(UMaterial* M, const TCHAR* Name, const FLinearColor& Default)
	{
		UMaterialExpressionVectorParameter* P = Add<UMaterialExpressionVectorParameter>(M);
		P->ParameterName = Name;
		P->DefaultValue = Default;
		return P;
	}

	UMaterialExpressionScalarParameter* Scalar(UMaterial* M, const TCHAR* Name, float Default)
	{
		UMaterialExpressionScalarParameter* P = Add<UMaterialExpressionScalarParameter>(M);
		P->ParameterName = Name;
		P->DefaultValue = Default;
		return P;
	}

	UMaterialExpressionConstant* Const(UMaterial* M, float Value)
	{
		UMaterialExpressionConstant* C = Add<UMaterialExpressionConstant>(M);
		C->R = Value;
		return C;
	}

	UMaterialExpressionMultiply* Mul(UMaterial* M, UMaterialExpression* A, UMaterialExpression* B)
	{
		UMaterialExpressionMultiply* X = Add<UMaterialExpressionMultiply>(M);
		X->A.Expression = A;
		X->B.Expression = B;
		return X;
	}

	UMaterialExpressionCustom* Custom(UMaterial* M, const FString& Code, ECustomMaterialOutputType Output, TArray<TPair<FString, UMaterialExpression*>> Inputs)
	{
		UMaterialExpressionCustom* C = Add<UMaterialExpressionCustom>(M);
		C->Code = Code;
		C->OutputType = Output;
		C->Inputs.Reset();
		for (const TPair<FString, UMaterialExpression*>& In : Inputs)
		{
			FCustomInput& Pin = C->Inputs.AddDefaulted_GetRef();
			Pin.InputName = FName(*In.Key);
			Pin.Input.Expression = In.Value;
		}
		return C;
	}

	bool Save(UPackage* Package, UObject* Asset)
	{
		if (UMaterial* Material = Cast<UMaterial>(Asset))
		{
			Material->PostEditChange();
		}
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		const bool bSaved = UPackage::SavePackage(Package, Asset, *Filename, Args);
		UE_LOG(LogGridlandsEditor, Display, TEXT("ImportArt: %s %s"), bSaved ? TEXT("saved") : TEXT("FAILED to save"), *Package->GetName());
		return bSaved;
	}

	/** Stylized master: painted vertex colour x tint, a graphic rim of light (fresnel), soft specular. */
	bool Painted()
	{
		UPackage* Pkg = nullptr;
		UMaterial* M = NewMaterial(TEXT("M_GLPainted"), Pkg);
		UMaterialEditorOnlyData* E = M->GetEditorOnlyData();
		UMaterialExpressionVertexColor* VC = Add<UMaterialExpressionVertexColor>(M);
		E->BaseColor.Expression = Mul(M, VC, Vec(M, TEXT("Tint"), FLinearColor::White));
		UMaterialExpressionFresnel* Rim = Add<UMaterialExpressionFresnel>(M);
		Rim->Exponent = 4.0f;
		E->EmissiveColor.Expression = Mul(M, Mul(M, Rim, Vec(M, TEXT("RimColor"), FLinearColor(1.0f, 0.86f, 0.7f))), Scalar(M, TEXT("RimStrength"), 0.18f));
		E->Roughness.Expression = Scalar(M, TEXT("Roughness"), 0.72f);
		E->Specular.Expression = Const(M, 0.3f);
		M->bUsedWithStaticLighting = false;
		M->bUsedWithInstancedStaticMeshes = true;
		return Save(Pkg, M);
	}

	/** Emissive parts (signs, Pehlichi's eye): they read at night. */
	bool Glow()
	{
		UPackage* Pkg = nullptr;
		UMaterial* M = NewMaterial(TEXT("M_GLGlow"), Pkg);
		UMaterialEditorOnlyData* E = M->GetEditorOnlyData();
		UMaterialExpressionVertexColor* VC = Add<UMaterialExpressionVertexColor>(M);
		E->BaseColor.Expression = Mul(M, VC, Const(M, 0.3f));
		E->EmissiveColor.Expression = Mul(M, VC, Scalar(M, TEXT("Glow"), 3.0f));
		E->Roughness.Expression = Const(M, 0.5f);
		return Save(Pkg, M);
	}

	bool Glass()
	{
		UPackage* Pkg = nullptr;
		UMaterial* M = NewMaterial(TEXT("M_GLGlass"), Pkg);
		UMaterialEditorOnlyData* E = M->GetEditorOnlyData();
		UMaterialExpressionVertexColor* VC = Add<UMaterialExpressionVertexColor>(M);
		E->BaseColor.Expression = VC;
		E->EmissiveColor.Expression = Mul(M, VC, Const(M, 0.2f));
		E->Roughness.Expression = Const(M, 0.08f);
		E->Specular.Expression = Const(M, 1.0f);
		E->Metallic.Expression = Const(M, 0.25f);
		return Save(Pkg, M);
	}

	/**
	 * NICE's corruption: unnaturally precise. Unlit electric blue with crisp bright edges (from the
	 * cube's face UVs) and a slow scan; nothing organic, nothing painted.
	 */
	bool Corruption()
	{
		UPackage* Pkg = nullptr;
		UMaterial* M = NewMaterial(TEXT("M_GLCorruption"), Pkg);
		M->SetShadingModel(MSM_Unlit);
		UMaterialEditorOnlyData* E = M->GetEditorOnlyData();
		UMaterialExpressionTextureCoordinate* UV = Add<UMaterialExpressionTextureCoordinate>(M);
		UMaterialExpressionTime* Time = Add<UMaterialExpressionTime>(M);
		E->EmissiveColor.Expression = Custom(M, TEXT(
			"float2 d = min(UV, 1.0 - UV);\n"
			"float edge = 1.0 - smoothstep(Width * 0.6, Width, min(d.x, d.y));\n"
			"float scan = 0.5 + 0.5 * sin(UV.y * 48.0 - T * 3.0);\n"
			"return Base * (0.55 + 0.2 * scan) + Edge * edge;"), CMOT_Float3,
			{ { TEXT("UV"), UV }, { TEXT("T"), Time }, { TEXT("Width"), Scalar(M, TEXT("EdgeWidth"), 0.07f) },
			  { TEXT("Base"), Vec(M, TEXT("CorruptionColor"), FLinearColor(0.02f, 0.28f, 1.6f)) },
			  { TEXT("Edge"), Vec(M, TEXT("EdgeColor"), FLinearColor(1.2f, 2.4f, 6.0f)) } });
		return Save(Pkg, M);
	}

	/**
	 * Stylized ground (P7): the chunk's vertex colours are masks (R flatness, G exposed earth where
	 * Zenny dug or raised, B variation); the palette and painterly patches are here, as parameters.
	 */
	bool Terrain()
	{
		UPackage* Pkg = nullptr;
		UMaterial* M = NewMaterial(TEXT("M_GLTerrain"), Pkg);
		UMaterialEditorOnlyData* E = M->GetEditorOnlyData();
		UMaterialExpressionVertexColor* VC = Add<UMaterialExpressionVertexColor>(M);
		UMaterialExpressionWorldPosition* WP = Add<UMaterialExpressionWorldPosition>(M);
		UMaterialExpressionNoise* Large = Add<UMaterialExpressionNoise>(M);
		Large->Scale = 0.0012f;
		Large->Quality = 1;
		Large->Position.Expression = WP;
		UMaterialExpressionNoise* Small = Add<UMaterialExpressionNoise>(M);
		Small->Scale = 0.009f;
		Small->Quality = 1;
		Small->Position.Expression = WP;
		E->BaseColor.Expression = Custom(M, TEXT(
			"float n1 = saturate(L * 0.5 + 0.5), n2 = saturate(S * 0.5 + 0.5);\n"
			"float3 grass = lerp(GrassA, GrassB, saturate(n1 * 0.9 + VC.b * 0.25));\n"
			"grass = lerp(grass, Meadow, smoothstep(0.7, 0.92, n2) * 0.45);\n"
			"float3 c = lerp(Rock, grass, smoothstep(0.55, 0.85, VC.r));\n"
			"float3 dirt = Dirt * (0.82 + 0.36 * n2);\n"
			"c = lerp(c, dirt, smoothstep(0.02, 0.35, VC.g));\n"
			"return c;"), CMOT_Float3,
			{ { TEXT("VC"), VC }, { TEXT("L"), Large }, { TEXT("S"), Small },
			  { TEXT("GrassA"), Vec(M, TEXT("GrassA"), FLinearColor(0.10f, 0.42f, 0.05f)) },
			  { TEXT("GrassB"), Vec(M, TEXT("GrassB"), FLinearColor(0.30f, 0.62f, 0.07f)) },
			  { TEXT("Meadow"), Vec(M, TEXT("Meadow"), FLinearColor(0.62f, 0.66f, 0.10f)) },
			  { TEXT("Rock"), Vec(M, TEXT("Rock"), FLinearColor(0.26f, 0.23f, 0.40f)) },
			  { TEXT("Dirt"), Vec(M, TEXT("Dirt"), FLinearColor(0.50f, 0.20f, 0.06f)) } });
		E->Roughness.Expression = Const(M, 0.92f);
		E->Specular.Expression = Const(M, 0.2f);
		return Save(Pkg, M);
	}

	/**
	 * The stylize post-process (P7-D): dark graphic outlines only where an outlined object is (it
	 * writes custom depth), found from depth and normal edges, fading with distance; optional
	 * light banding on lit surfaces. Every knob is a parameter so the cost of each can be measured.
	 */
	bool Post()
	{
		UPackage* Pkg = nullptr;
		UMaterial* M = NewMaterial(TEXT("PP_GLStylize"), Pkg);
		M->MaterialDomain = MD_PostProcess;
		M->BlendableLocation = BL_SceneColorBeforeBloom;
		UMaterialEditorOnlyData* E = M->GetEditorOnlyData();
		// Scene texture nodes enable the lookups the custom code makes.
		UMaterialExpressionSceneTexture* Input = Add<UMaterialExpressionSceneTexture>(M);
		Input->SceneTextureId = PPI_PostProcessInput0;
		UMaterialExpressionSceneTexture* Depth = Add<UMaterialExpressionSceneTexture>(M);
		Depth->SceneTextureId = PPI_SceneDepth;
		UMaterialExpressionSceneTexture* Custom_ = Add<UMaterialExpressionSceneTexture>(M);
		Custom_->SceneTextureId = PPI_CustomDepth;
		UMaterialExpressionSceneTexture* Normal = Add<UMaterialExpressionSceneTexture>(M);
		Normal->SceneTextureId = PPI_WorldNormal;
		UMaterialExpressionSceneTexture* Diffuse = Add<UMaterialExpressionSceneTexture>(M);
		Diffuse->SceneTextureId = PPI_DiffuseColor;
		E->EmissiveColor.Expression = Custom(M, TEXT(
			"float2 uv = GetDefaultSceneTextureUV(Parameters, 14);\n"
			"float3 col = SceneTextureLookup(uv, 14, false).rgb;\n"
			"float d0 = SceneTextureLookup(uv, 1, false).r;\n"
			"float c0 = SceneTextureLookup(uv, 13, false).r;\n"
			"float m0 = (c0 < d0 + 8.0) ? 1.0 : 0.0;\n"
			"float3 n0 = SceneTextureLookup(uv, 8, false).rgb;\n"
			"float2 px = View.BufferSizeAndInvSize.zw * Width;\n"
			"float edge = 0.0;\n"
			"float2 offs[8] = { float2(px.x, 0), float2(-px.x, 0), float2(0, px.y), float2(0, -px.y), px, -px, float2(px.x, -px.y), float2(-px.x, px.y) };\n"
			"for (int i = 0; i < 8; i++) {\n"
			"  float2 u2 = uv + offs[i];\n"
			"  float d = SceneTextureLookup(u2, 1, false).r;\n"
			"  float c = SceneTextureLookup(u2, 13, false).r;\n"
			"  float m = (c < d + 8.0) ? 1.0 : 0.0;\n"
			"  float3 n = SceneTextureLookup(u2, 8, false).rgb;\n"
			"  float dz = abs(d - d0) / max(min(d, d0), 1.0);\n"
			"  float e = max(step(DepthThreshold, dz), step(NormalThreshold, 1.0 - saturate(dot(n, n0))));\n"
			"  edge = max(edge, e * max(m, m0));\n"
			"}\n"
			"edge *= OutlineOn * (1.0 - saturate((d0 - FadeStart) / max(FadeEnd - FadeStart, 1.0)));\n"
			"float3 base = SceneTextureLookup(uv, 2, false).rgb;\n"
			"float blum = dot(base, float3(0.3, 0.59, 0.11));\n"
			"float lum = dot(col, float3(0.3, 0.59, 0.11));\n"
			"float lit = lum / max(blum, 0.03);\n"
			"float band = (floor(lit * Bands) + smoothstep(0.35, 0.65, frac(lit * Bands))) / Bands;\n"
			"float celMask = CelOn * step(0.03, blum) * step(d0, 500000.0);\n"
			"float3 cel = col * lerp(1.0, band / max(lit, 1e-3), celMask * CelStrength);\n"
			"return lerp(cel, cel * OutlineDarkness, edge);"), CMOT_Float3,
			{ { TEXT("Input"), Input }, { TEXT("Depth"), Depth }, { TEXT("CustomD"), Custom_ }, { TEXT("Normal"), Normal }, { TEXT("Diffuse"), Diffuse },
			  { TEXT("Width"), Scalar(M, TEXT("OutlineWidth"), 1.5f) }, { TEXT("OutlineOn"), Scalar(M, TEXT("OutlineOn"), 1.0f) },
			  { TEXT("OutlineDarkness"), Scalar(M, TEXT("OutlineDarkness"), 0.08f) },
			  { TEXT("DepthThreshold"), Scalar(M, TEXT("DepthThreshold"), 0.06f) }, { TEXT("NormalThreshold"), Scalar(M, TEXT("NormalThreshold"), 0.35f) },
			  { TEXT("FadeStart"), Scalar(M, TEXT("FadeStart"), 3500.0f) }, { TEXT("FadeEnd"), Scalar(M, TEXT("FadeEnd"), 9000.0f) },
			  { TEXT("CelOn"), Scalar(M, TEXT("CelOn"), 1.0f) }, { TEXT("CelStrength"), Scalar(M, TEXT("CelStrength"), 0.55f) },
			  { TEXT("Bands"), Scalar(M, TEXT("CelBands"), 3.0f) } });
		return Save(Pkg, M);
	}

	UMaterialInterface* Master(const FString& Slot)
	{
		const TCHAR* Name = Slot == TEXT("GL_Glow") ? TEXT("M_GLGlow") : Slot == TEXT("GL_Glass") ? TEXT("M_GLGlass") : TEXT("M_GLPainted");
		return LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), MaterialsPath, Name, Name));
	}

	/** Imports one FBX with the pipeline's fixed settings; returns the mesh. */
	UStaticMesh* Import(const FString& File, const FString& Name)
	{
		UFbxFactory* Factory = NewObject<UFbxFactory>();
		Factory->AddToRoot();
		UFbxImportUI* UI = Factory->ImportUI;
		UI->bImportMaterials = false;
		UI->bImportTextures = false;
		UI->bImportAnimations = false;
		UI->bImportAsSkeletal = false;
		UI->MeshTypeToImport = FBXIT_StaticMesh;
		UI->bAutomatedImportShouldDetectType = false;
		UI->StaticMeshImportData->bCombineMeshes = true;
		UI->StaticMeshImportData->VertexColorImportOption = EVertexColorImportOption::Replace;
		UI->StaticMeshImportData->bAutoGenerateCollision = false;
		UI->StaticMeshImportData->bGenerateLightmapUVs = false;
		UI->StaticMeshImportData->bBuildNanite = false;
		UI->StaticMeshImportData->NormalImportMethod = FBXNIM_ImportNormals;
		UI->StaticMeshImportData->bRemoveDegenerates = true;
		UAssetImportTask* Task = NewObject<UAssetImportTask>();
		Task->AddToRoot();
		Task->Filename = File;
		Task->DestinationPath = MeshesPath;
		Task->DestinationName = Name;
		Task->bReplaceExisting = true;
		Task->bAutomated = true;
		Task->bSave = false;
		Task->Factory = Factory;
		Task->Options = UI; // automated imports read their settings from the task
		FAssetToolsModule& Tools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		Tools.Get().ImportAssetTasks({ Task });
		UStaticMesh* Mesh = nullptr;
		for (UObject* Object : Task->GetObjects())
		{
			Mesh = Mesh ? Mesh : Cast<UStaticMesh>(Object);
		}
		Task->RemoveFromRoot();
		Factory->RemoveFromRoot();
		return Mesh;
	}
}

int32 UGLImportArtCommandlet::Main(const FString& Params)
{
	using namespace GLArt;
	bool bOk = Painted() && Glow() && Glass() && Corruption() && Terrain() && Post();
	if (!bOk || Params.Contains(TEXT("-MaterialsOnly")))
	{
		return bOk ? 0 : 1;
	}
	FString ManifestPath;
	if (!FParse::Value(*Params, TEXT("-Manifest="), ManifestPath))
	{
		UE_LOG(LogGridlandsEditor, Error, TEXT("ImportArt: -Manifest=<manifest.json> is required"));
		return 2;
	}
	FString Text;
	TSharedPtr<FJsonObject> Manifest;
	if (!FFileHelper::LoadFileToString(Text, *ManifestPath) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Manifest) || !Manifest.IsValid())
	{
		UE_LOG(LogGridlandsEditor, Error, TEXT("ImportArt: cannot read %s"), *ManifestPath);
		return 2;
	}
	// The classic FBX importer honours the pipeline's explicit settings.
	if (IConsoleVariable* Interchange = IConsoleManager::Get().FindConsoleVariable(TEXT("Interchange.FeatureFlags.Import.FBX")))
	{
		Interchange->Set(false);
	}
	const FString Dir = FPaths::GetPath(ManifestPath);
	TArray<TSharedPtr<FJsonValue>> Report;
	int32 Failures = 0;
	for (const TSharedPtr<FJsonValue>& Value : Manifest->GetArrayField(TEXT("assets")))
	{
		const TSharedPtr<FJsonObject> Asset = Value->AsObject();
		const FString Name = Asset->GetStringField(TEXT("name"));
		TArray<FString> Problems;
		UStaticMesh* Mesh = Import(Dir / Asset->GetStringField(TEXT("file")), Name);
		if (!Mesh)
		{
			Problems.Add(TEXT("import failed"));
		}
		else
		{
			// Bind material slots by name to the master materials.
			TArray<FStaticMaterial>& Slots = Mesh->GetStaticMaterials();
			TArray<FString> Expected;
			for (const TSharedPtr<FJsonValue>& S : Asset->GetArrayField(TEXT("slots")))
			{
				Expected.Add(S->AsString());
			}
			for (FStaticMaterial& Slot : Slots)
			{
				FString SlotName = Slot.MaterialSlotName.ToString();
				if (!Expected.ContainsByPredicate([&SlotName](const FString& E) { return SlotName.StartsWith(E); }))
				{
					Problems.Add(FString::Printf(TEXT("unexpected material slot %s"), *SlotName));
				}
				Slot.MaterialInterface = Master(SlotName);
			}
			if (Slots.Num() != Expected.Num())
			{
				Problems.Add(FString::Printf(TEXT("%d slots, manifest says %d"), Slots.Num(), Expected.Num()));
			}
			Mesh->SetLightMapResolution(4);
			Mesh->PostEditChange();
			// Validate against what Blender wrote (metres -> cm).
			const FBox Bounds = Mesh->GetBoundingBox();
			const TArray<TSharedPtr<FJsonValue>>& Min = Asset->GetArrayField(TEXT("boundsMin"));
			const TArray<TSharedPtr<FJsonValue>>& Max = Asset->GetArrayField(TEXT("boundsMax"));
			const FVector WantMin(Min[0]->AsNumber() * 100.0, -Max[1]->AsNumber() * 100.0, Min[2]->AsNumber() * 100.0);
			const FVector WantMax(Max[0]->AsNumber() * 100.0, -Min[1]->AsNumber() * 100.0, Max[2]->AsNumber() * 100.0);
			if (!Bounds.Min.Equals(WantMin, 1.5) || !Bounds.Max.Equals(WantMax, 1.5))
			{
				Problems.Add(FString::Printf(TEXT("bounds %s..%s, manifest (converted) %s..%s"), *Bounds.Min.ToCompactString(), *Bounds.Max.ToCompactString(), *WantMin.ToCompactString(), *WantMax.ToCompactString()));
			}
			if (FMath::Abs(Bounds.Min.Z) > 1.0 || FMath::Abs(Bounds.GetCenter().X) > 1.0 || FMath::Abs(Bounds.GetCenter().Y) > 1.0)
			{
				Problems.Add(TEXT("pivot is not the bottom centre"));
			}
			const int32 Triangles = Mesh->GetNumTriangles(0);
			const int32 WantTriangles = static_cast<int32>(Asset->GetNumberField(TEXT("triangles")));
			if (FMath::Abs(Triangles - WantTriangles) > FMath::Max(4, WantTriangles / 20))
			{
				Problems.Add(FString::Printf(TEXT("%d triangles, manifest %d"), Triangles, WantTriangles));
			}
			const FMeshDescription* Description = Mesh->GetMeshDescription(0);
			bool bColoured = false;
			if (Description)
			{
				FStaticMeshConstAttributes Attributes(*Description);
				const TVertexInstanceAttributesConstRef<FVector4f> Colours = Attributes.GetVertexInstanceColors();
				for (const FVertexInstanceID Id : Description->VertexInstances().GetElementIDs())
				{
					const FVector4f C = Colours[Id];
					if (!FMath::IsNearlyEqual(C.X, 1.f) || !FMath::IsNearlyEqual(C.Y, 1.f) || !FMath::IsNearlyEqual(C.Z, 1.f))
					{
						bColoured = true;
						break;
					}
				}
			}
			if (!bColoured)
			{
				Problems.Add(TEXT("no painted vertex colours"));
			}
			if (!Save(Mesh->GetOutermost(), Mesh))
			{
				Problems.Add(TEXT("save failed"));
			}
			TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("name"), Name);
			Row->SetNumberField(TEXT("triangles"), Triangles);
			Row->SetStringField(TEXT("bounds"), FString::Printf(TEXT("%s..%s"), *Bounds.Min.ToCompactString(), *Bounds.Max.ToCompactString()));
			TArray<TSharedPtr<FJsonValue>> ProblemValues;
			for (const FString& P : Problems)
			{
				ProblemValues.Add(MakeShared<FJsonValueString>(P));
			}
			Row->SetArrayField(TEXT("problems"), ProblemValues);
			Report.Add(MakeShared<FJsonValueObject>(Row));
		}
		for (const FString& P : Problems)
		{
			UE_LOG(LogGridlandsEditor, Error, TEXT("ImportArt: %s: %s"), *Name, *P);
		}
		Failures += Problems.Num() > 0 ? 1 : 0;
		UE_LOG(LogGridlandsEditor, Display, TEXT("ImportArt: %s %s"), *Name, Problems.Num() ? TEXT("FAILED validation") : TEXT("imported and valid"));
	}
	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	Out->SetArrayField(TEXT("assets"), Report);
	Out->SetNumberField(TEXT("failures"), Failures);
	FString OutText;
	FJsonSerializer::Serialize(Out, TJsonWriterFactory<>::Create(&OutText));
	FFileHelper::SaveStringToFile(OutText, *(Dir / TEXT("import-report.json")));
	UE_LOG(LogGridlandsEditor, Display, TEXT("ImportArt: %d assets, %d failed validation"), Report.Num(), Failures);
	return Failures ? 1 : 0;
}
