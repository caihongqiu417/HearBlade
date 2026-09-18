#include "BeatComboEditorWidget.h"

#include "Combat/AttackData.h"
#include "AssetRegistry/AssetData.h"
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "IDetailsView.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "FileHelpers.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"

namespace
{
uint16 ReadU16(const uint8* Ptr)
{
	uint16 Value;
	FMemory::Memcpy(&Value, Ptr, sizeof(Value));
	return Value;
}

uint32 ReadU32(const uint8* Ptr)
{
	uint32 Value;
	FMemory::Memcpy(&Value, Ptr, sizeof(Value));
	return Value;
}

void DrawLine(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry,
	float X1, float Y1, float X2, float Y2, const FLinearColor& Color, float Width = 1.0f)
{
	TArray<FVector2D> Points;
	Points.Add(FVector2D(X1, Y1));
	Points.Add(FVector2D(X2, Y2));
	FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), Points,
		ESlateDrawEffect::None, Color, true, Width);
}
}

bool FBeatWaveform::Load(const FString& Path)
{
	Peaks.Reset();
	DurationSeconds = 0.0;
	Error.Reset();
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path))
	{
		Error = TEXT("无法读取 WAV 文件");
		return false;
	}
	if (Bytes.Num() < 44 || FMemory::Memcmp(Bytes.GetData(), "RIFF", 4) != 0 ||
		FMemory::Memcmp(Bytes.GetData() + 8, "WAVE", 4) != 0)
	{
		Error = TEXT("需要 RIFF/WAVE 格式的音频");
		return false;
	}

	uint16 Format = 0;
	uint16 Channels = 0;
	uint16 Bits = 0;
	uint32 SampleRate = 0;
	uint16 BlockAlign = 0;
	int64 DataOffset = -1;
	uint32 DataSize = 0;
	for (int64 Offset = 12; Offset + 8 <= Bytes.Num();)
	{
		const uint32 ChunkSize = ReadU32(Bytes.GetData() + Offset + 4);
		const int64 Body = Offset + 8;
		if (Body + ChunkSize > Bytes.Num())
		{
			break;
		}
		if (FMemory::Memcmp(Bytes.GetData() + Offset, "fmt ", 4) == 0 && ChunkSize >= 16)
		{
			Format = ReadU16(Bytes.GetData() + Body);
			Channels = ReadU16(Bytes.GetData() + Body + 2);
			SampleRate = ReadU32(Bytes.GetData() + Body + 4);
			BlockAlign = ReadU16(Bytes.GetData() + Body + 12);
			Bits = ReadU16(Bytes.GetData() + Body + 14);
		}
		else if (FMemory::Memcmp(Bytes.GetData() + Offset, "data", 4) == 0)
		{
			DataOffset = Body;
			DataSize = ChunkSize;
		}
		Offset = Body + ChunkSize + (ChunkSize & 1);
	}
	if (DataOffset < 0 || SampleRate == 0 || Channels == 0 || BlockAlign == 0 ||
		!((Format == 1 && (Bits == 16 || Bits == 24)) || (Format == 3 && Bits == 32)))
	{
		Error = TEXT("支持 PCM 16/24 位或 IEEE float 32 位 WAV");
		return false;
	}
	const int64 FrameCount = FMath::Min<int64>(DataSize, Bytes.Num() - DataOffset) / BlockAlign;
	if (FrameCount <= 0)
	{
		Error = TEXT("WAV 没有可用样本");
		return false;
	}
	DurationSeconds = static_cast<double>(FrameCount) / SampleRate;
	const int32 BinCount = FMath::Min<int64>(16384, FrameCount);
	Peaks.SetNumZeroed(BinCount);
	const int32 BytesPerSample = Bits / 8;
	for (int64 Frame = 0; Frame < FrameCount; ++Frame)
	{
		const uint8* Ptr = Bytes.GetData() + DataOffset + Frame * BlockAlign;
		float Peak = 0.0f;
		for (uint16 Channel = 0; Channel < Channels; ++Channel)
		{
			const uint8* Sample = Ptr + Channel * BytesPerSample;
			float Value = 0.0f;
			if (Format == 3)
			{
				FMemory::Memcpy(&Value, Sample, sizeof(float));
			}
			else if (Bits == 16)
			{
				int16 IntValue;
				FMemory::Memcpy(&IntValue, Sample, sizeof(IntValue));
				Value = static_cast<float>(IntValue) / 32768.0f;
			}
			else
			{
				int32 IntValue = static_cast<int32>(Sample[0]) |
					(static_cast<int32>(Sample[1]) << 8) |
					(static_cast<int32>(Sample[2]) << 16);
				if ((IntValue & 0x800000) != 0) IntValue |= ~0xFFFFFF;
				Value = static_cast<float>(IntValue) / 8388608.0f;
			}
			Peak = FMath::Max(Peak, FMath::Abs(Value));
		}
		const int32 Bin = FMath::Min<int64>(BinCount - 1, Frame * BinCount / FrameCount);
		Peaks[Bin] = FMath::Max(Peaks[Bin], FMath::Clamp(Peak, 0.0f, 1.0f));
	}
	return true;
}

class SBeatComboTimeline : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SBeatComboTimeline) {}
		SLATE_ARGUMENT(TWeakPtr<SBeatComboEditorWidget>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args) { Owner = Args._Owner; }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(900.0f, 280.0f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
		FSlateWindowElementList& Elements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;

private:
	void Pick(const FGeometry& Geometry, const FPointerEvent& Event);
	TWeakPtr<SBeatComboEditorWidget> Owner;
	bool bDragging = false;
};

int32 SBeatComboTimeline::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry,
	const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 LayerId,
	const FWidgetStyle& Style, bool bParentEnabled) const
{
	const TSharedPtr<SBeatComboEditorWidget> Editor = Owner.Pin();
	if (!Editor.IsValid()) return LayerId;
	const FVector2D Size = Geometry.GetLocalSize();
	const float Width = Size.X;
	const float Height = Size.Y;
	FSlateDrawElement::MakeBox(Elements, LayerId, Geometry.ToPaintGeometry(),
		FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None,
		FLinearColor(0.035f, 0.045f, 0.065f, 1.0f));
	const double Start = Editor->GetViewStart();
	const double Span = FMath::Max(Editor->GetViewDuration(), 0.1);
	const double End = Start + Span;
	auto ToX = [Start, Span, Width](double Seconds)
	{
		return static_cast<float>((Seconds - Start) / Span * Width);
	};
	const FSlateFontInfo SmallFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 9);
	DrawLine(Elements, LayerId + 1, Geometry, 0, Height * 0.5f, Width, Height * 0.5f,
		FLinearColor(0.18f, 0.25f, 0.32f));
	const FBeatWaveform& Wave = Editor->GetWaveform();
	if (!Wave.Peaks.IsEmpty() && Wave.DurationSeconds > 0.0)
	{
		const int32 Columns = FMath::Min(1400, FMath::Max(1, FMath::FloorToInt(Width)));
		for (int32 Column = 0; Column < Columns; ++Column)
		{
			const double Time = Start + (Column + 0.5) / Columns * Span;
			if (Time < 0 || Time >= Wave.DurationSeconds) continue;
			const int32 Index = FMath::Clamp(FMath::FloorToInt(Time / Wave.DurationSeconds * Wave.Peaks.Num()),
				0, Wave.Peaks.Num() - 1);
			const float Amplitude = FMath::Sqrt(Wave.Peaks[Index]) * Height * 0.40f;
			const float X = static_cast<float>(Column) / Columns * Width;
			DrawLine(Elements, LayerId + 1, Geometry, X, Height * 0.5f - Amplitude,
				X, Height * 0.5f + Amplitude, FLinearColor(0.14f, 0.66f, 0.80f, 0.70f));
		}
	}
	const double Period = Editor->GetBeatPeriod();
	const double FirstBeat = Editor->GetFirstBeatSourceSeconds();
	const int32 First = FMath::Max(0, FMath::FloorToInt((Start - FirstBeat) / Period));
	const int32 Last = FMath::Min(First + 1000, FMath::CeilToInt((End - FirstBeat) / Period));
	for (int32 Beat = First; Beat <= Last; ++Beat)
	{
		const float X = ToX(FirstBeat + Beat * Period);
		const bool bBar = Beat % 4 == 0;
		DrawLine(Elements, LayerId + 2, Geometry, X, 0, X, Height,
			bBar ? FLinearColor(0.95f, 0.78f, 0.26f, 0.72f) : FLinearColor(0.58f, 0.54f, 0.38f, 0.32f),
			bBar ? 1.5f : 1.0f);
		if (bBar && X >= 0 && X < Width - 50)
		{
			FSlateDrawElement::MakeText(Elements, LayerId + 3,
				Geometry.ToPaintGeometry(FVector2D(58, 17), FSlateLayoutTransform(FVector2D(X + 3, 3))),
				FText::FromString(FString::Printf(TEXT("小节 %d"), Beat / 4 + 1)), SmallFont,
				ESlateDrawEffect::None, FLinearColor(1.0f, 0.82f, 0.38f));
		}
	}
	for (int32 Second = FMath::CeilToInt(Start / 5.0) * 5; Second <= End; Second += 5)
	{
		const float X = ToX(Second);
		FSlateDrawElement::MakeText(Elements, LayerId + 3,
			Geometry.ToPaintGeometry(FVector2D(52, 17), FSlateLayoutTransform(FVector2D(X + 2, Height - 19))),
			FText::FromString(FString::Printf(TEXT("%d s"), Second)), SmallFont,
			ESlateDrawEffect::None, FLinearColor(0.72f, 0.78f, 0.86f));
	}
	const double RequestTime = Editor->GetPreviewRequestSourceSeconds();
	if (RequestTime >= Start && RequestTime <= End)
	{
		const float X = ToX(RequestTime);
		DrawLine(Elements, LayerId + 3, Geometry, X, 0, X, Height,
			FLinearColor(0.86f, 0.45f, 1.0f, 0.9f), 2.0f);
		FSlateDrawElement::MakeText(Elements, LayerId + 4,
			Geometry.ToPaintGeometry(FVector2D(72, 17), FSlateLayoutTransform(FVector2D(X + 3, 5))),
			FText::FromString(TEXT("触发攻击")), SmallFont,
			ESlateDrawEffect::None, FLinearColor(0.9f, 0.6f, 1.0f));
	}
	const UAttackDataAsset* Data = Editor->GetAttackData();
	if (Data && Data->AttackMontage && !Data->ImpactMarkers.IsEmpty() && Editor->GetSelectedBeat() >= 0)
	{
		const double MontageStart = Editor->GetMontageStartSourceSeconds();
		const double MontageEnd = MontageStart + Editor->GetPreviewElapsedAtAnimationTime(Data->GetClipEndSeconds()) -
			Editor->GetPreviewElapsedAtAnimationTime(Data->ClipStartSeconds);
		for (int32 Edge = 0; Edge < 2; ++Edge)
		{
			const double Time = Edge == 0 ? MontageStart : MontageEnd;
			if (Time >= Start && Time <= End)
			{
				const float X = ToX(Time);
				DrawLine(Elements, LayerId + 3, Geometry, X, 0, X, Height,
					FLinearColor(0.45f, 1.0f, 0.72f, 0.95f), 2.0f);
				FSlateDrawElement::MakeText(Elements, LayerId + 4,
					Geometry.ToPaintGeometry(FVector2D(62, 17), FSlateLayoutTransform(FVector2D(X + 3, 24))),
					FText::FromString(Edge == 0 ? TEXT("起手") : TEXT("收招")), SmallFont,
					ESlateDrawEffect::None, FLinearColor(0.45f, 1.0f, 0.72f));
			}
		}
		const int32 Count = Data->ImpactMarkers.Num();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const double AnimationTime = Data->ImpactMarkers[Index].AnimationTimeSeconds;
			if (AnimationTime < Data->ClipStartSeconds || AnimationTime > Data->GetClipEndSeconds()) continue;
			const double Time = MontageStart + Editor->GetPreviewElapsedAtAnimationTime(AnimationTime) -
				Editor->GetPreviewElapsedAtAnimationTime(Data->ClipStartSeconds);
			if (Time < Start || Time > End) continue;
			const float X = ToX(Time);
			DrawLine(Elements, LayerId + 4, Geometry, X, 0, X, Height,
				Index == 0 ? FLinearColor(1.0f, 0.30f, 0.22f) : FLinearColor(1.0f, 0.55f, 0.24f), 3.0f);
			FSlateDrawElement::MakeText(Elements, LayerId + 5,
				Geometry.ToPaintGeometry(FVector2D(38, 17), FSlateLayoutTransform(FVector2D(X + 4, 46))),
				FText::FromString(FString::Printf(TEXT("击%d"), Index + 1)), SmallFont,
				ESlateDrawEffect::None, FLinearColor(1.0f, 0.52f, 0.30f));
		}
	}
	return LayerId + 6;
}

void SBeatComboTimeline::Pick(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (const TSharedPtr<SBeatComboEditorWidget> Editor = Owner.Pin())
	{
		const float X = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()).X;
		const double SourceTime = Editor->GetViewStart() +
			FMath::Clamp(X / FMath::Max(Geometry.GetLocalSize().X, 1.0f), 0.0f, 1.0f) * Editor->GetViewDuration();
		Editor->PickRequestAtSourceSeconds(SourceTime);
	}
}

FReply SBeatComboTimeline::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
	bDragging = true;
	Pick(Geometry, Event);
	return FReply::Handled().CaptureMouse(AsShared());
}

FReply SBeatComboTimeline::OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
	bDragging = false;
	return FReply::Handled().ReleaseMouseCapture();
}

FReply SBeatComboTimeline::OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!bDragging) return FReply::Unhandled();
	Pick(Geometry, Event);
	return FReply::Handled();
}

void SBeatComboEditorWidget::Construct(const FArguments& InArgs)
{
	WavPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Music/Paramore_AintItFun_UE.wav"));
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.bAllowSearch = true;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	DetailsView = PropertyEditor.CreateDetailView(DetailsArgs);
	DetailsView->OnFinishedChangingProperties().AddSP(this, &SBeatComboEditorWidget::OnAssetPropertiesChanged);

	auto NumberRow = [this](const FString& Label, double* Field, double Min, double Max) -> TSharedRef<SWidget>
	{
		return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4)
		[
			SNew(SBox).WidthOverride(110)
			[
				SNew(STextBlock).Text(FText::FromString(Label))
			]
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(4)
		[
			SNew(SSpinBox<double>)
			.MinValue(Min).MaxValue(Max)
			.Value_Lambda([Field]() { return *Field; })
			.OnValueChanged_Lambda([this, Field](double Value)
			{
				*Field = Value;
				RefreshTimeline();
			})
		];
	};

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(6)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("节拍动作编辑器 · 单段动作 / 波形 / 小节拍位预览")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(5)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4)
				[
					SNew(SBox).WidthOverride(110)[SNew(STextBlock).Text(FText::FromString(TEXT("攻击数据")))]
				]
				+ SHorizontalBox::Slot().FillWidth(1).Padding(4)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(UAttackDataAsset::StaticClass())
					.ObjectPath(this, &SBeatComboEditorWidget::GetAssetPath)
					.OnObjectChanged(this, &SBeatComboEditorWidget::HandleAssetChanged)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(5)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4)
				[
					SNew(SBox).WidthOverride(110)[SNew(STextBlock).Text(FText::FromString(TEXT("源 WAV")))]
				]
				+ SHorizontalBox::Slot().FillWidth(1).Padding(4)
				[
					SAssignNew(WavPathBox, SEditableTextBox)
					.Text(FText::FromString(WavPath))
					.OnTextCommitted_Lambda([this](const FText& Value, ETextCommit::Type)
					{
						WavPath = Value.ToString();
						LoadWav();
					})
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(4)
				[
					SNew(SButton).Text(FText::FromString(TEXT("浏览")))
					.OnClicked(this, &SBeatComboEditorWidget::BrowseWav)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(4)
				[
					SNew(SButton).Text(FText::FromString(TEXT("加载波形")))
					.OnClicked(this, &SBeatComboEditorWidget::LoadWav)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1)[NumberRow(TEXT("BPM"), &BPM, 30.0, 300.0)]
				+ SHorizontalBox::Slot().FillWidth(1)[NumberRow(TEXT("原音频拍相位(秒)"), &BeatPhase, -10.0, 10.0)]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1)[NumberRow(TEXT("播放起点(秒)"), &SourceStart, 0.0, 3600.0)]
				+ SHorizontalBox::Slot().FillWidth(1)[NumberRow(TEXT("预览触发攻击(原音频秒)"), &PreviewRequestSourceSeconds, 0.0, 3600.0)]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1)[NumberRow(TEXT("可视起点(秒)"), &ViewStart, 0.0, 3600.0)]
				+ SHorizontalBox::Slot().FillWidth(1)[NumberRow(TEXT("可视长度(秒)"), &ViewDuration, 1.0, 300.0)]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("最低速度")))
					]
					+ SHorizontalBox::Slot().FillWidth(1).Padding(4)
					[
						SNew(SSpinBox<float>).MinValue(0.5f).MaxValue(2.0f)
						.Value_Lambda([this]() { return MinimumRate; })
						.OnValueChanged_Lambda([this](float Value)
						{
							MinimumRate = FMath::Min(Value, MaximumRate);
							PreviewRate = FMath::Clamp(PreviewRate, MinimumRate, MaximumRate);
							RefreshTimeline();
						})
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("最高速度")))
					]
					+ SHorizontalBox::Slot().FillWidth(1).Padding(4)
					[
						SNew(SSpinBox<float>).MinValue(0.5f).MaxValue(2.0f)
						.Value_Lambda([this]() { return MaximumRate; })
						.OnValueChanged_Lambda([this](float Value)
						{
							MaximumRate = FMath::Max(Value, MinimumRate);
							RefreshTimeline();
						})
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("预览速度")))
					]
					+ SHorizontalBox::Slot().FillWidth(1).Padding(4)
					[
						SNew(SSpinBox<float>).MinValue(0.5f).MaxValue(2.0f)
						.Value_Lambda([this]() { return PreviewRate; })
						.OnValueChanged_Lambda([this](float Value)
						{
							PreviewRate = FMath::Clamp(Value, MinimumRate, MaximumRate);
							RefreshTimeline();
						})
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("拖动紫线模拟行为树触发攻击；Boss 会等合适的拍点再起手。第一击按 First Impact Beat In Bar 选拍（0=任意，1=小节第一拍）。红线=第一击，橙线=后续击；紫线时间不会保存。")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SAssignNew(SegmentsBox, SVerticalBox)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SNew(SBox).HeightOverride(280)
				[
					SAssignNew(Timeline, SBeatComboTimeline).Owner(SharedThis(this))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(4)
				[
					SNew(SButton).Text(FText::FromString(TEXT("自动匹配速度")))
					.OnClicked(this, &SBeatComboEditorWidget::AutoAlign)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(4)
				[
					SNew(SButton).Text(FText::FromString(TEXT("自动逐击对齐")))
					.OnClicked(this, &SBeatComboEditorWidget::AutoAlignSegments)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(4)
				[
					SNew(SButton).Text(FText::FromString(TEXT("恢复整段同速")))
					.OnClicked(this, &SBeatComboEditorWidget::ResetSegments)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(4)
				[
					SNew(SButton).Text(FText::FromString(TEXT("保存到攻击数据")))
					.OnClicked(this, &SBeatComboEditorWidget::SaveAsset)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SNew(STextBlock).Text(this, &SBeatComboEditorWidget::GetSummary)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SNew(SBox).HeightOverride(300)[DetailsView.ToSharedRef()]
			]
		]
	];

	SelectAsset(LoadObject<UAttackDataAsset>(nullptr,
		TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/DA_DragonKnight_Combo01_Beat.DA_DragonKnight_Combo01_Beat")));
	LoadWav();
}

double SBeatComboEditorWidget::GetFirstBeatSourceSeconds() const
{
	const double Period = GetBeatPeriod();
	double Offset = FMath::Fmod(BeatPhase - SourceStart, Period);
	if (Offset < 0.0) Offset += Period;
	return SourceStart + Offset;
}

double SBeatComboEditorWidget::GetFirstImpactSourceSeconds() const
{
	return GetFirstBeatSourceSeconds() + GetSelectedBeat() * GetBeatPeriod();
}

int32 SBeatComboEditorWidget::GetSelectedBeat() const
{
	const UAttackDataAsset* Data = AttackData.Get();
	const double FirstTime = !Data ? 0.0 : Data->ImpactMarkers.IsEmpty()
		? Data->AuthoredImpactTime : Data->ImpactMarkers[0].AnimationTimeSeconds;
	const double EarliestImpact = PreviewRequestSourceSeconds +
		GetPreviewElapsedAtAnimationTime(FirstTime) -
		GetPreviewElapsedAtAnimationTime(Data ? Data->ClipStartSeconds : 0.0) + 0.1;
	int32 CandidateBeat = FMath::Max(0, FMath::CeilToInt((EarliestImpact - GetFirstBeatSourceSeconds()) / GetBeatPeriod()));
	if (Data && Data->FirstImpactBeatInBar > 0)
	{
		constexpr int32 BeatsPerBar = 4;
		const int32 WantedBeatInBar = FMath::Clamp(Data->FirstImpactBeatInBar, 1, BeatsPerBar) - 1;
		CandidateBeat += (WantedBeatInBar - CandidateBeat % BeatsPerBar + BeatsPerBar) % BeatsPerBar;
	}
	return CandidateBeat;
}

double SBeatComboEditorWidget::GetMontageStartSourceSeconds() const
{
	const UAttackDataAsset* Data = AttackData.Get();
	if (!Data || PreviewRate <= 0.0f) return GetFirstImpactSourceSeconds();
	const double FirstTime = Data->ImpactMarkers.IsEmpty()
		? Data->AuthoredImpactTime : Data->ImpactMarkers[0].AnimationTimeSeconds;
	return GetFirstImpactSourceSeconds() - GetPreviewElapsedAtAnimationTime(FirstTime) +
		GetPreviewElapsedAtAnimationTime(Data->ClipStartSeconds);
}

double SBeatComboEditorWidget::GetPreviewElapsedAtAnimationTime(double AnimationTime) const
{
	const UAttackDataAsset* Data = AttackData.Get();
	if (!Data || !bUseSegmentRates || PreviewSegmentRates.Num() != Data->ImpactMarkers.Num() + 1)
	{
		return AnimationTime / FMath::Max(0.01f, PreviewRate);
	}
	double Elapsed = 0.0;
	double Previous = 0.0;
	int32 Segment = 0;
	for (const FAttackImpactMarker& Marker : Data->ImpactMarkers)
	{
		const double End = FMath::Min(AnimationTime, static_cast<double>(Marker.AnimationTimeSeconds));
		if (End > Previous) Elapsed += (End - Previous) / PreviewSegmentRates[Segment];
		if (AnimationTime <= Marker.AnimationTimeSeconds) return Elapsed;
		Previous = Marker.AnimationTimeSeconds;
		++Segment;
	}
	return Elapsed + FMath::Max(0.0, AnimationTime - Previous) / PreviewSegmentRates[Segment];
}

void SBeatComboEditorWidget::PickRequestAtSourceSeconds(double SourceSeconds)
{
	PreviewRequestSourceSeconds = SourceSeconds;
	RefreshTimeline();
}

void SBeatComboEditorWidget::SelectAsset(UAttackDataAsset* Asset)
{
	AttackData = Asset;
	DetailsView->SetObject(Asset);
	if (!Asset) return;
	Status = Asset->ImpactMarkers.IsEmpty()
		? TEXT("尚未标定命中时间：在下方 ImpactMarkers 添加真实命中帧后再保存")
		: TEXT("点击波形模拟触发攻击；紫线时间不会保存");
	BPM = Asset->ReferenceBPM;
	BeatPhase = Asset->ReferenceBeatPhaseSeconds;
	SourceStart = Asset->ReferenceMusicStartSeconds;
	MinimumRate = Asset->MinimumPlayRate;
	MaximumRate = FMath::Max(MinimumRate, Asset->MaximumPlayRate);
	PreviewRate = FMath::Clamp(Asset->PlayRate, MinimumRate, MaximumRate);
	bUseSegmentRates = !Asset->SegmentPlayRates.IsEmpty();
	PreviewSegmentRates.SetNum(Asset->ImpactMarkers.Num() + 1);
	for (int32 Index = 0; Index < PreviewSegmentRates.Num(); ++Index)
	{
		PreviewSegmentRates[Index] = FMath::Clamp(Asset->SegmentPlayRates.IsValidIndex(Index)
			? Asset->SegmentPlayRates[Index] : PreviewRate, MinimumRate, MaximumRate);
	}
	RebuildSegmentControls();
	if (!Asset->MusicSourceWavPath.IsEmpty())
	{
		WavPath = Asset->MusicSourceWavPath;
		if (WavPathBox.IsValid()) WavPathBox->SetText(FText::FromString(WavPath));
	}
	LoadWav();
	RefreshTimeline();
}

void SBeatComboEditorWidget::HandleAssetChanged(const FAssetData& AssetData)
{
	SelectAsset(Cast<UAttackDataAsset>(AssetData.GetAsset()));
}

FString SBeatComboEditorWidget::GetAssetPath() const
{
	return AttackData.IsValid() ? AttackData->GetPathName() : FString();
}

FReply SBeatComboEditorWidget::BrowseWav()
{
	IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
	if (!Desktop) return FReply::Handled();
	TArray<FString> Files;
	const void* Parent = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	if (Desktop->OpenFileDialog(const_cast<void*>(Parent), TEXT("选择源 WAV"),
		FPaths::GetPath(WavPath), TEXT(""), TEXT("WAV 音频 (*.wav)|*.wav"),
		EFileDialogFlags::None, Files) && !Files.IsEmpty())
	{
		WavPath = Files[0];
		WavPathBox->SetText(FText::FromString(WavPath));
		LoadWav();
	}
	return FReply::Handled();
}

FReply SBeatComboEditorWidget::LoadWav()
{
	const bool bLoaded = Waveform.Load(WavPath);
	Status = bLoaded
		? FString::Printf(TEXT("已加载波形：%.2f 秒"), Waveform.DurationSeconds)
		: Waveform.Error;
	RefreshTimeline();
	return FReply::Handled();
}

FReply SBeatComboEditorWidget::AutoAlign()
{
	const UAttackDataAsset* Data = AttackData.Get();
	if (!Data || Data->ImpactMarkers.Num() < 2)
	{
		Status = TEXT("需要至少两个命中标记才能自动匹配播放速度");
		return FReply::Handled();
	}
	if (MinimumRate <= 0.0f || MaximumRate < MinimumRate)
	{
		Status = TEXT("播放速度范围无效");
		return FReply::Handled();
	}
	const double FirstTime = Data->ImpactMarkers[0].AnimationTimeSeconds;
	const double Period = GetBeatPeriod();
	double BestScore = TNumericLimits<double>::Max();
	float BestRate = MinimumRate;
	for (float Rate = MinimumRate; Rate <= MaximumRate + 0.0005f; Rate += 0.001f)
	{
		double Score = 0.0;
		for (int32 Index = 1; Index < Data->ImpactMarkers.Num(); ++Index)
		{
			const FAttackImpactMarker& Marker = Data->ImpactMarkers[Index];
			const double Actual = (Marker.AnimationTimeSeconds - FirstTime) / Rate;
			const double Desired = Marker.BeatOffset * Period;
			Score += FMath::Square(Actual - Desired);
		}
		Score += 0.00001 * FMath::Square(Rate - 1.0f);
		if (Score < BestScore)
		{
			BestScore = Score;
			BestRate = Rate;
		}
	}
	PreviewRate = BestRate;
	bUseSegmentRates = false;
	Status = FString::Printf(TEXT("自动匹配：%.3fx，命中间隔均方误差 %.1f ms"),
		PreviewRate, FMath::Sqrt(BestScore / (Data->ImpactMarkers.Num() - 1)) * 1000.0);
	RefreshTimeline();
	return FReply::Handled();
}

void SBeatComboEditorWidget::RebuildSegmentControls()
{
	if (!SegmentsBox.IsValid()) return;
	SegmentsBox->ClearChildren();
	const UAttackDataAsset* Data = AttackData.Get();
	if (!Data) return;
	for (int32 Index = 0; Index < PreviewSegmentRates.Num(); ++Index)
	{
		const FString Label = Index == 0 ? TEXT("起手→第1击") :
			Index == Data->ImpactMarkers.Num() ? TEXT("最后一击→收招") :
			FString::Printf(TEXT("第%d击→第%d击"), Index, Index + 1);
		SegmentsBox->AddSlot().AutoHeight().Padding(2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4)
			[
				SNew(SBox).WidthOverride(150)[SNew(STextBlock).Text(FText::FromString(Label))]
			]
			+ SHorizontalBox::Slot().FillWidth(1).Padding(4)
			[
				SNew(SSpinBox<float>).MinValue(0.5f).MaxValue(2.0f)
				.Value_Lambda([this, Index]() { return PreviewSegmentRates[Index]; })
				.OnValueChanged_Lambda([this, Index](float Value)
				{
				PreviewSegmentRates[Index] = FMath::Clamp(Value, MinimumRate, MaximumRate);
				bUseSegmentRates = true;
				RefreshTimeline();
			})
			]
		];
	}
}

void SBeatComboEditorWidget::OnAssetPropertiesChanged(const FPropertyChangedEvent& Event)
{
	const UAttackDataAsset* Data = AttackData.Get();
	if (Data)
	{
		Status = Data->ImpactMarkers.IsEmpty()
			? TEXT("尚未标定命中时间：请添加真实命中帧")
			: TEXT("命中标记已修改，请检查预览并保存");
	}
	if (Data && PreviewSegmentRates.Num() != Data->ImpactMarkers.Num() + 1)
	{
		PreviewSegmentRates.Init(PreviewRate, Data->ImpactMarkers.Num() + 1);
		bUseSegmentRates = false;
		RebuildSegmentControls();
	}
	RefreshTimeline();
}

FReply SBeatComboEditorWidget::AutoAlignSegments()
{
	const UAttackDataAsset* Data = AttackData.Get();
	if (!Data || Data->ImpactMarkers.Num() < 2 || MaximumRate < MinimumRate)
	{
		Status = TEXT("需要至少两个有效命中标记和速度范围");
		return FReply::Handled();
	}
	PreviewSegmentRates.Init(PreviewRate, Data->ImpactMarkers.Num() + 1);
	const double Period = GetBeatPeriod();
	for (int32 Index = 1; Index < Data->ImpactMarkers.Num(); ++Index)
	{
		const double AnimationGap = Data->ImpactMarkers[Index].AnimationTimeSeconds -
			Data->ImpactMarkers[Index - 1].AnimationTimeSeconds;
		const double BeatGap = (Data->ImpactMarkers[Index].BeatOffset -
			Data->ImpactMarkers[Index - 1].BeatOffset) * Period;
		if (AnimationGap <= 0.0 || BeatGap <= 0.0)
		{
			Status = TEXT("相邻命中时间和目标拍数必须递增");
			return FReply::Handled();
		}
		PreviewSegmentRates[Index] = FMath::Clamp(static_cast<float>(AnimationGap / BeatGap), MinimumRate, MaximumRate);
	}
	bUseSegmentRates = true;
	Status = TEXT("已分别匹配相邻命中；可手动微调每段速度，然后保存");
	RebuildSegmentControls();
	RefreshTimeline();
	return FReply::Handled();
}

FReply SBeatComboEditorWidget::ResetSegments()
{
	bUseSegmentRates = false;
	PreviewSegmentRates.Init(PreviewRate, PreviewSegmentRates.Num());
	Status = TEXT("已恢复整段同速；保存后生效");
	RefreshTimeline();
	return FReply::Handled();
}

FReply SBeatComboEditorWidget::SaveAsset()
{
	UAttackDataAsset* Data = AttackData.Get();
	if (!Data || !Data->AttackMontage || Data->ImpactMarkers.IsEmpty())
	{
		Status = TEXT("请选择含动作 Montage 和命中标记的 AttackData");
		return FReply::Handled();
	}
	if (Data->ClipStartSeconds >= Data->GetClipEndSeconds())
	{
		Status = TEXT("动作片段起止时间无效");
		return FReply::Handled();
	}
	for (const FAttackImpactMarker& Marker : Data->ImpactMarkers)
	{
		if (Marker.AnimationTimeSeconds < Data->ClipStartSeconds ||
			Marker.AnimationTimeSeconds > Data->GetClipEndSeconds())
		{
			Status = TEXT("命中时间必须位于动作片段起止之间");
			return FReply::Handled();
		}
	}
	if (bUseSegmentRates && PreviewSegmentRates.Num() != Data->ImpactMarkers.Num() + 1)
	{
		Status = TEXT("命中标记数量已变化，请重新点击自动逐击对齐");
		return FReply::Handled();
	}
	Data->Modify();
	Data->PlayRate = PreviewRate;
	Data->SegmentPlayRates = bUseSegmentRates ? PreviewSegmentRates : TArray<float>();
	Data->MusicSourceWavPath = WavPath;
	Data->ReferenceBPM = BPM;
	Data->ReferenceBeatPhaseSeconds = BeatPhase;
	Data->ReferenceMusicStartSeconds = SourceStart;
	Data->MinimumPlayRate = MinimumRate;
	Data->MaximumPlayRate = MaximumRate;
	Data->MarkPackageDirty();
	TArray<UPackage*> Packages;
	Packages.Add(Data->GetOutermost());
	const bool bSaved = UEditorLoadingAndSavingUtils::SavePackages(Packages, false);
	Status = bSaved ? TEXT("已保存动作与速度；触发攻击的预览时间未保存") : TEXT("保存失败，请检查输出日志");
	return FReply::Handled();
}

FText SBeatComboEditorWidget::GetSummary() const
{
	const UAttackDataAsset* Data = AttackData.Get();
	if (!Data) return FText::FromString(TEXT("请选择攻击数据资产。"));
	if (Data->ImpactMarkers.IsEmpty())
	{
		return FText::FromString(TEXT("尚未设置命中标记：在下方 ImpactMarkers 添加真实命中时间后，才能预览下一可用拍。"));
	}
	const double Impact = GetFirstImpactSourceSeconds();
	const double Start = GetMontageStartSourceSeconds();
	const double FirstTime = Data->ImpactMarkers.IsEmpty()
		? Data->AuthoredImpactTime : Data->ImpactMarkers[0].AnimationTimeSeconds;
	double MaxError = 0.0;
	for (const FAttackImpactMarker& Marker : Data->ImpactMarkers)
	{
		const double Actual = GetPreviewElapsedAtAnimationTime(Marker.AnimationTimeSeconds) -
			GetPreviewElapsedAtAnimationTime(FirstTime);
		const double Desired = Marker.BeatOffset * GetBeatPeriod();
		MaxError = FMath::Max(MaxError, FMath::Abs(Actual - Desired));
	}
	return FText::FromString(FString::Printf(
		TEXT("触发攻击 %.3f s  |  第一击限小节第 %d 拍  |  目标拍 %d / 第一击 %.3f s  |  起手 %.3f s  |  %s  |  最大偏差 %.1f ms  |  %s"),
		PreviewRequestSourceSeconds, Data->FirstImpactBeatInBar, GetSelectedBeat(), Impact, Start,
		bUseSegmentRates ? TEXT("逐击变速") : TEXT("整段同速"), MaxError * 1000.0, *Status));
}

void SBeatComboEditorWidget::RefreshTimeline()
{
	if (Timeline.IsValid()) Timeline->Invalidate(EInvalidateWidgetReason::Paint);
}
