#include "Playtest/GLFrictionLog.h"

#include "Building/GLBuildModeComponent.h"
#include "Building/GLBuildText.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GridlandsGame.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const TCHAR* StateName(EGLBuildState State)
	{
		switch (State)
		{
		case EGLBuildState::Browse: return TEXT("BROWSE");
		case EGLBuildState::Finish: return TEXT("FINISH");
		case EGLBuildState::Remove: return TEXT("REMOVE");
		default: return TEXT("PLACE");
		}
	}

	const UGLBuildModeComponent* PlayerBuildMode(UWorld* World)
	{
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		return Pawn ? Pawn->FindComponentByClass<UGLBuildModeComponent>() : nullptr;
	}

	/** The F8 picker: Up / Down choose a category, the text box takes the note, Enter saves, Esc cancels. */
	class SGLFrictionPicker : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SGLFrictionPicker) {}
		SLATE_END_ARGS()

		TWeakObjectPtr<UWorld> World;
		int32 Selected = 0;
		TSharedPtr<SEditableTextBox> Note;
		TSharedPtr<STextBlock> List;

		void Construct(const FArguments&)
		{
			ChildSlot
			[
				SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SBorder).Padding(16.f).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.85f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Friction note (Up/Down: category, type a note, Enter: save, Esc: cancel)")))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
						[
							SAssignNew(List, STextBlock)
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SBox).WidthOverride(640.f)
							[
								SAssignNew(Note, SEditableTextBox).HintText(FText::FromString(TEXT("optional note")))
									.OnTextCommitted(this, &SGLFrictionPicker::Committed)
									.OnKeyDownHandler(this, &SGLFrictionPicker::Key)
							]
						]
					]
				]
			];
			Refresh();
		}

		void Refresh()
		{
			FString Text;
			for (int32 I = 0; I < GLFrictionLog::Categories().Num(); ++I)
			{
				Text += FString::Printf(TEXT("%s %s\n"), I == Selected ? TEXT(">") : TEXT(" "), *GLFrictionLog::Categories()[I]);
			}
			List->SetText(FText::FromString(Text));
		}

		FReply Key(const FGeometry&, const FKeyEvent& Event)
		{
			const int32 N = GLFrictionLog::Categories().Num();
			if (Event.GetKey() == EKeys::Up || Event.GetKey() == EKeys::Down)
			{
				Selected = ((Selected + (Event.GetKey() == EKeys::Up ? -1 : 1)) % N + N) % N;
				Refresh();
				return FReply::Handled();
			}
			if (Event.GetKey() == EKeys::Escape)
			{
				Close();
				return FReply::Handled();
			}
			return FReply::Unhandled();
		}

		void Committed(const FText& Text, ETextCommit::Type How)
		{
			if (How == ETextCommit::OnEnter)
			{
				GLFrictionLog::Write(World.Get(), GLFrictionLog::Categories()[Selected], Text.ToString());
				Close();
			}
		}

		void Close();
	};

	TSharedPtr<SGLFrictionPicker> GOpenPicker;

	void SGLFrictionPicker::Close()
	{
		if (GEngine && GEngine->GameViewport && GOpenPicker.IsValid())
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(GOpenPicker.ToSharedRef());
		}
		if (APlayerController* PC = World.IsValid() ? World->GetFirstPlayerController() : nullptr)
		{
			PC->SetInputMode(FInputModeGameOnly());
			PC->SetShowMouseCursor(false);
		}
		GOpenPicker.Reset();
	}

	FAutoConsoleCommandWithWorldAndArgs FrictionCommand(
		TEXT("gl.Friction"),
		TEXT("Playtest friction note: gl.Friction <CATEGORY> [note...] (writes Saved/Playtest/friction.jsonl)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() == 0)
			{
				UE_LOG(LogGridlands, Display, TEXT("gl.Friction: categories %s"), *FString::Join(GLFrictionLog::Categories(), TEXT(" ")));
				return;
			}
			const FString Note = FString::Join(TArrayView<const FString>(Args).RightChop(1), TEXT(" "));
			const bool bOk = GLFrictionLog::Write(World, Args[0].ToUpper(), Note);
			UE_LOG(LogGridlands, Display, TEXT("gl.Friction: %s"), bOk ? *FString::Printf(TEXT("noted in %s"), *GLFrictionLog::LogPath()) : TEXT("unknown category"));
		}));
}

const TArray<FString>& GLFrictionLog::Categories()
{
	static const TArray<FString> Vocabulary = { TEXT("CAMERA"), TEXT("SNAP"), TEXT("ROTATION"), TEXT("BROWSER"), TEXT("PIECE_FIND"), TEXT("FINISH"),
		TEXT("STRUCTURE_FEEDBACK"), TEXT("REMOVAL"), TEXT("SALVAGE"), TEXT("RESOURCE"), TEXT("CLAIM"), TEXT("INPUT"), TEXT("REPETITION"),
		TEXT("VISIBILITY"), TEXT("OTHER") };
	return Vocabulary;
}

FString GLFrictionLog::LogPath()
{
	return FPaths::ProjectSavedDir() / TEXT("Playtest") / TEXT("friction.jsonl");
}

bool GLFrictionLog::Write(UWorld* World, const FString& Category, const FString& Note)
{
	if (!Categories().Contains(Category))
	{
		return false;
	}
	TSharedRef<FJsonObject> Line = MakeShared<FJsonObject>();
	Line->SetStringField(TEXT("timestamp"), FDateTime::UtcNow().ToIso8601());
	Line->SetStringField(TEXT("category"), Category);
	Line->SetStringField(TEXT("note"), Note.Left(500));
	if (const UGLBuildModeComponent* Build = PlayerBuildMode(World))
	{
		const FGLBuildView& View = Build->GetView();
		TSharedRef<FJsonObject> Context = MakeShared<FJsonObject>();
		Context->SetStringField(TEXT("mode"), View.Mode == EGLToolMode::Build ? TEXT("BUILD") : View.Mode == EGLToolMode::None ? TEXT("PLAY") : TEXT("TERRAFORM"));
		Context->SetStringField(TEXT("state"), StateName(View.State));
		Context->SetStringField(TEXT("piece"), View.Piece.ToString());
		Context->SetStringField(TEXT("finish"), View.Finish.ToString());
		Context->SetNumberField(TEXT("yawDegrees"), GLStructureRules::YawDegrees(View.YawStep));
		Context->SetBoolField(TEXT("snapped"), View.bHasCandidate && View.Snap.bSnapped);
		Context->SetStringField(TEXT("structure"), View.bHasCandidate ? GLBuildText::StateWord(View.Check.Preview) : FString());
		Context->SetStringField(TEXT("reason"), View.Reason);
		Context->SetNumberField(TEXT("predictedCollapse"), View.Predicted.Num());
		Context->SetBoolField(TEXT("buildCamera"), View.bBuildCamera);
		Line->SetObjectField(TEXT("context"), Context);
	}
	FString Text;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text);
	FJsonSerializer::Serialize(Line, Writer);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(LogPath()), true);
	return FFileHelper::SaveStringToFile(Text + TEXT("\n"), *LogPath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
}

bool GLFrictionLog::IsPickerOpen()
{
	return GOpenPicker.IsValid();
}

void GLFrictionLog::OpenPicker(UWorld* World)
{
	if (GOpenPicker.IsValid() || !GEngine || !GEngine->GameViewport || !World)
	{
		return;
	}
	GOpenPicker = SNew(SGLFrictionPicker);
	GOpenPicker->World = World;
	GEngine->GameViewport->AddViewportWidgetContent(GOpenPicker.ToSharedRef(), 100);
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(GOpenPicker->Note);
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);
	}
}
