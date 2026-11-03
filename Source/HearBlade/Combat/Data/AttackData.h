// 定义敌人攻击数据，连接编辑器保存的标记与运行时连击

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimMontage.h"
#include "Engine/DataAsset.h"
#include "AttackData.generated.h"

class UTexture2D;
class USoundBase;

UENUM(BlueprintType)
enum class EBossAttackDamageShape : uint8
{
	Blade UMETA(DisplayName = "近战刀刃"),
	GroundBurst UMETA(DisplayName = "落地范围爆发")
};

UENUM(BlueprintType)
enum class EAttackRecognitionShape : uint8
{
	Auto UMETA(DisplayName = "跟随实际指令"),
	ShortGlint UMETA(DisplayName = "短刀芒"),
	WideArc UMETA(DisplayName = "横扫宽弧"),
	Cross UMETA(DisplayName = "交汇反击"),
	SustainedArc UMETA(DisplayName = "持续格挡光带"),
	GroundWarning UMETA(DisplayName = "范围危险圈")
};

USTRUCT(BlueprintType)

struct FAttackRecognitionInfo
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "识别信息") FText ActionName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "识别信息", meta = (MultiLine = true))
	FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "识别信息")
	EAttackRecognitionShape Shape = EAttackRecognitionShape::Auto;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "识别信息", meta = (ClampMin = "0.25"))
	float LeadBeats = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "识别信息") bool bOverrideColor = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "识别信息",
	          meta = (EditCondition = "bOverrideColor"))
	FLinearColor Color = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "识别信息") TObjectPtr<USoundBase> CueSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "识别信息") TObjectPtr<UTexture2D> Symbol;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "识别信息") bool bShowSymbol = true;
	// 世界中的预警形状表示应对方式，尺寸和亮度只控制表现强度
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "识别信息",
	          meta = (ClampMin = "0.5", ClampMax = "2.5"))
	float BladeCueScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "识别信息",
	          meta = (ClampMin = "0.5", ClampMax = "2.5"))
	float BladeCueIntensity = 1.f;
	// 仅供运行时识别使用，不能把允许格挡的横扫误标成只能闪避
	bool bDodgeOnlyCue = false;
	bool bCounterCue = false;
};

USTRUCT(BlueprintType)

struct FGroundBurstSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "范围攻击", meta = (ClampMin = "50"))
	float RadiusCm = 450.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "范围攻击", meta = (ClampMin = "1"))
	float WarningBeats = 4.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "范围攻击", meta = (ClampMin = "0"))
	float EscapeReward = 12.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "范围攻击",
	          meta = (ClampMin = "0", ClampMax = "0.3"))
	float EscapeWindowSeconds = .12f;
};

UENUM(BlueprintType)
enum class ERhythmResponseType : uint8
{
	Dodge UMETA(DisplayName = "闪避"),
	Block UMETA(DisplayName = "格挡"),
	Counter UMETA(DisplayName = "反击"),
	HoldBlock UMETA(DisplayName = "持续格挡"),
	DodgeOrBlock UMETA(DisplayName = "闪避或格挡")
};

UENUM(BlueprintType)
enum class ERhythmHoldMarkerType : uint8
{
	None UMETA(DisplayName = "非长按"),
	Start UMETA(DisplayName = "长按开始"),
	Continue UMETA(DisplayName = "长按持续"),
	End UMETA(DisplayName = "长按结束")
};

USTRUCT(BlueprintType)

struct FImpactCinematicSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "关键慢放")
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "关键慢放",
	          meta = (ClampMin = "0.35", ClampMax = "1.0", EditCondition = "bEnabled"))
	float MusicAndBossRate = .88f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "关键慢放",
	          meta = (ClampMin = "0.0", ClampMax = "2.0", EditCondition = "bEnabled"))
	float LeadBeats = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "关键慢放",
	          meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnabled"))
	float HoldAfterImpactBeats = .16f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "关键慢放",
	          meta = (ClampMin = "0.02", ClampMax = "1.0", EditCondition = "bEnabled"))
	float RecoverySeconds = .14f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "关键慢放",
	          meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnabled"))
	float TriggerChance = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "关键慢放",
	          meta = (ClampMin = "0.25", ClampMax = "2.0", EditCondition = "bEnabled"))
	float CameraStrength = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "关键慢放",
	          meta = (ClampMin = "0.2", ClampMax = "1.0", EditCondition = "bEnabled"))
	float ClashActorRate = .48f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "关键慢放",
	          meta = (ClampMin = "0.03", ClampMax = "0.3", EditCondition = "bEnabled"))
	float ClashActorSlowSeconds = .11f;
};

// 一段独立攻击中的一次命中，BeatOffset 相对第一击，第一击填 0
USTRUCT(BlueprintType)

struct FAttackImpactMarker
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|识别信息")
	FAttackRecognitionInfo Recognition;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|伤害")
	EBossAttackDamageShape DamageShape = EBossAttackDamageShape::Blade;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|范围攻击",
	          meta = (EditCondition = "DamageShape == EBossAttackDamageShape::GroundBurst"))
	FGroundBurstSettings GroundBurst;
	// 源动画帧号用于查看，运行时仍以 AnimationTimeSeconds 为准
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|识别信息") int32 SourceFrame = -1;

	// 动画命中标记
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据", meta = (ClampMin = "0.0"))
	float AnimationTimeSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据", meta = (ClampMin = "0"))
	double BeatOffset = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|伤害", meta = (ClampMin = "0.0"))
	float Damage = 18.0f;

	// 理想按键比武器接触提前的秒数，评级以接触时刻减去此值为目标
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|闪避判定",
	          meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float IdealDodgeLeadSeconds = 0.14f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|伤害",
	          meta = (ClampMin = "0.01", ClampMax = "0.3"))
	// 命中拍点前开始收集刀刃接触，提前接触先记录，到拍点再结算
	float DamageWindowBeforeSeconds = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|伤害",
	          meta = (ClampMin = "0.01", ClampMax = "0.3"))
	// 拍点后继续检查的时长，持续格挡末尾还会扩展到松键评级窗口
	float DamageWindowAfterSeconds = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|防御指令")
	ERhythmResponseType RequiredResponse = ERhythmResponseType::Dodge;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|防御指令",
	          meta = (ClampMin = "0.0", ClampMax = "8.0"))
	float PromptLeadBeats = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|防御指令")
	bool bForceStrongAccent = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|防御指令")
	TObjectPtr<UTexture2D> PromptIconOverride = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|防御指令",
	          meta = (ClampMin = "0.4", ClampMax = "2.0", EditCondition = "PromptIconOverride != nullptr"))
	float PromptIconScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|防御指令",
	          meta = (EditCondition = "PromptIconOverride != nullptr"))
	FLinearColor PromptIconColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|持续格挡",
	          meta = (EditCondition = "RequiredResponse == ERhythmResponseType::HoldBlock"))
	// 同一段持续格挡的开始、维持和结束必须使用同一组名
	FName HoldGroup = TEXT("HoldA");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|持续格挡",
	          meta = (EditCondition = "RequiredResponse == ERhythmResponseType::HoldBlock"))
	ERhythmHoldMarkerType HoldMarker = ERhythmHoldMarkerType::None;

	// 单次命中的演出配置，命中仍使用同一套音乐时间安排
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|关键慢放")
	FImpactCinematicSettings Cinematic;
};

// 替换玩家需要执行的防御操作，命中时间和伤害仍使用原命中标记

USTRUCT(BlueprintType)

struct FAttackResponseInstruction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|随机指令")
	ERhythmResponseType RequiredResponse = ERhythmResponseType::Block;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|随机指令",
	          meta = (EditCondition = "RequiredResponse == ERhythmResponseType::HoldBlock"))
	ERhythmHoldMarkerType HoldMarker = ERhythmHoldMarkerType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|随机指令",
	          meta = (EditCondition = "RequiredResponse == ERhythmResponseType::HoldBlock"))
	FName HoldGroup = TEXT("HoldA");
};

// 攻击排队时只选择一次指令组合，让界面提示与伤害判定使用同一组指令

USTRUCT(BlueprintType)

struct FAttackResponsePattern
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|随机指令")
	FName PatternName = TEXT("Pattern");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|随机指令", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	// 从第几个命中标记开始替换，索引从零开始
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|随机指令", meta = (ClampMin = "0"))
	int32 FirstImpactIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|随机指令")
	TArray<FAttackResponseInstruction> Instructions;
};

// ============================================================
// Boss 攻击数据资产
// ============================================================
// 保存 Boss 攻击动画和动画原本的命中时间
// 本类只保存数据，不播放动画，不检测伤害
UCLASS(BlueprintType)

// 敌人攻击资源
class HEARBLADE_API UAttackDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

  public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "技能信息") FText SkillName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "技能信息", meta = (MultiLine = true))
	FText SkillDescription;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "技能信息", meta = (ClampMin = "1", ClampMax = "2"))
	int32 MinimumBossPhase = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "技能信息")
	FAttackRecognitionInfo DefaultRecognition;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "技能信息") float SourceFrameRate = 0.f;
	// 在攻击姿势上停留一段时间，为玩家离开危险区域留出时间
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "蓄力动作", meta = (ClampMin = "0"))
	float ChargePoseAnimationSeconds = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "蓄力动作", meta = (ClampMin = "0"))
	float ChargeHoldSeconds = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "蓄力动作") int32 ChargePoseSourceFrame = -1;
	float GetMotionElapsedAtAnimationTime(float AnimationTime) const;
	float GetMotionAnimationTimeAtElapsed(float ElapsedSeconds) const;
	// 行为树可独立调度的一段攻击动画
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据")
	TObjectPtr<UAnimMontage> AttackMontage = nullptr;

	// 独立动作的有效片段End=0 表示播放到 Montage 末尾，可去掉多余的收剑段
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据", meta = (ClampMin = "0.0"))
	float ClipStartSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据", meta = (ClampMin = "0.0"))
	float ClipEndSeconds = 0.0f;

	// 返回有效动画片段终点，未配置终点时使用蒙太奇长度
	float GetClipEndSeconds() const;

	// 动画开始到命中的时间
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据", meta = (ClampMin = "0.0"))
	float AuthoredImpactTime = 0.0f;

	// 按动画时间升序排列；第一个标记决定这一段动作的启动时间
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据")
	TArray<FAttackImpactMarker> ImpactMarkers;

	// 为空时使用原指令，否则按权重选择一组指令，不改变动画时间

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|随机指令")
	TArray<FAttackResponsePattern> ResponsePatterns;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据", meta = (ClampMin = "0.01"))
	float PlayRate = 1.0f;

	// 可选：起手→第一击、各相邻命中、最后一击→收招的逐段速度留空时整段使用 PlayRate
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据")
	TArray<float> SegmentPlayRates;

	// 每次命中前通过动作对齐修正移动目标
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|Motion Warping")
	bool bWarpToTarget = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|Motion Warping",
	          meta = (ClampMin = "0.05"))
	float WarpLeadAnimationSeconds = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|Motion Warping",
	          meta = (ClampMin = "0.0"))
	float WarpStopDistance = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|Motion Warping",
	          meta = (ClampMin = "0.0"))
	float MaxWarpTravelPerStrike = 250.0f;

	// 临近命中时停止追踪目标，让玩家能够侧移躲过已经出手的刀刃

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|Motion Warping")
	bool bCommitStrikeDirection = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|Motion Warping",
	          meta = (ClampMin = "0.1", ClampMax = "0.65", EditCondition = "bCommitStrikeDirection"))
	float StrikeCommitLeadSeconds = 0.45f;

	float GetRateAtAnimationTime(float AnimationTime) const;
	// 把动画位置换成实际播放时长，并计入蓄力停顿
	float GetElapsedAtAnimationTime(float AnimationTime) const;
	float GetAnimationTimeAtElapsed(float ElapsedSeconds) const;

	// 音乐拍线参数由编辑器写入，测试音乐时钟也从这里读取，保证预览与运行时一致
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "节拍编辑器", meta = (ClampMin = "1.0"))
	double ReferenceBPM = 104.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "节拍编辑器")
	double ReferenceBeatPhaseSeconds = 0.204;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "节拍编辑器", meta = (ClampMin = "0.0"))
	double ReferenceMusicStartSeconds = 4.0;

	// 零表示任意拍，其他值指定小节内拍号，运行时使用音乐时钟的小节拍数
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "节拍编辑器|调度", meta = (ClampMin = "0"))
	int32 FirstImpactBeatInBar = 1;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "节拍编辑器")
	FString MusicSourceWavPath;

	UPROPERTY(EditAnywhere, Category = "节拍编辑器", meta = (ClampMin = "0.01"))
	float MinimumPlayRate = 1.0f;

	UPROPERTY(EditAnywhere, Category = "节拍编辑器", meta = (ClampMin = "0.01"))
	float MaximumPlayRate = 1.15f;
#endif
};
