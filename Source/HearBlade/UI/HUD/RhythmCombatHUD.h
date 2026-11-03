// 创建并连接战斗界面，保留旧绘制路径的兼容入口
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Combat/Player/PlayerDefenseComponent.h"
#include "RhythmCombatHUD.generated.h"

class UTexture2D;
class UBeatSyncedComboComponent;
class URhythmResponseVisualData;
class URhythmWidgetController;
class URhythmCombatWidget;
class UUserWidget;

// 创建界面控件与显示控制器，连接数据通知
// 蓝图界面不可用或启用旧版显示时，保留画布绘制入口
UCLASS()

class HEARBLADE_API ARhythmCombatHUD : public AHUD
{
	GENERATED_BODY()

  public:
	ARhythmCombatHUD();
	// 优先更新蓝图界面，需要旧版显示时再绘制画布内容
	virtual void DrawHUD() override;
	// 清理界面与音频分析，释放控制器引用
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	URhythmWidgetController* GetWidgetController();

  private:
	void UpdateTouchControls();
	bool bTouchControlsChecked = false;
	UPROPERTY(Transient) TObjectPtr<UUserWidget> TouchControls;
	bool UpdateBlueprintHUD();
	UPROPERTY(Transient) TObjectPtr<URhythmWidgetController> MVCController;
	UPROPERTY(Transient) TObjectPtr<URhythmCombatWidget> MVCWidget;
	bool bMVCLoadAttempted = false;
	bool bMVCVerifyCaptured = false;
	float PhaseHUDOpacity = 1.f;
	// 绘制一条画布线段
	void DrawLine(float X1, float Y1, float X2, float Y2, FLinearColor Color, float Thickness = 0.f);

	struct FJudgmentFeedEntry
	{
		ERhythmDodgeJudgment Grade = ERhythmDodgeJudgment::None;
		float ErrorMs = 0.0f;
		double TimeSeconds = 0.0;
		int32 Combo = 0;
		float StartSlot = 5.0f;
		float TargetSlot = 4.0f;
	};

	// 绘制玩家生命、共鸣和攻击状态
	void DrawPlayerPanel(const UPlayerDefenseComponent* State, float Scale, double Now);
	// 按音乐拍点绘制玩家攻击节奏提示
	void DrawPlayerAttackPulse(const UPlayerDefenseComponent* State, float Scale, double Now);
	// 绘制命中点的闪光、速度线与文字
	void DrawImpactComic(float Scale, double Now);
	// 首次游玩的教程卡片和短时提示，推进逻辑在教程组件中
	void DrawTutorial(float Scale, double Now);
	UPROPERTY() TMap<FName, TObjectPtr<UTexture2D>> TutorialArt;
	// 绘制最近一次评级与时间偏差
	void DrawJudgment(const UPlayerDefenseComponent* State, float Scale, double Now);
	// 绘制失误或漏判的短时提示
	void DrawFailureFeedback(float Scale, double Now);
	// 把即将发生的攻击转换为轨道上的指令位置
	void DrawAttackTimingCue(const UPlayerDefenseComponent* State, float Scale);
	// 把反击提示投影到敌人附近
	void DrawCounterWorldCue(float Scale);
	// 刀形节拍轨道分别显示敌人防御指令、玩家攻击拍点和追击破绽


	void DrawBladeScore(const UPlayerDefenseComponent* State, UBeatSyncedComboComponent* Combo,
	                    const TArray<struct FUpcomingImpactCue>& Cues, float Scale);
	// 加载刀形轨道所需资源，资源不齐时使用原轨道
	bool LoadBladeScoreArt();
	UPROPERTY() TMap<FName, TObjectPtr<UTexture2D>> BladeArt;
	double NextBladeArtLoadWorld = 0.0;
	bool bBladeScoreDrawn = false;
	FVector2D BladeAttackTextAnchor = FVector2D::ZeroVector;
	// 根据输入偏差绘制判定框反馈
	void DrawTimingGate(const UPlayerDefenseComponent* State, float JudgeX, float MidY, float Scale,
	                    float FadeIn, float AbsInputError);
	// 绘制二阶段指令轨道与持续格挡范围
	void DrawPhaseTwoTimingRail(const UPlayerDefenseComponent* State, UBeatSyncedComboComponent* Boss,
	                            float Scale);
	double PhaseTwoRailFirstBeat = -1.;
	// 在锁定目标头部附近绘制标记
	void DrawLockMarker();
	// 绘制 Boss 血量与延迟变化条
	void DrawBossHealth(float Scale);
	// 读取频谱并绘制音乐波形
	void DrawAudioVisualizer(float Scale);
	UPROPERTY() TObjectPtr<UTexture2D> BossHealthFrameTexture;
	float BossHealthTrail = 1.f;
	TWeakObjectPtr<UBeatSyncedComboComponent> HealthBarTarget;
	// 查找当前用于指令提示的 Boss 连招组件
	UBeatSyncedComboComponent* FindCueCombo();
	// 按评级选择显示颜色
	static FLinearColor GetJudgmentColor(ERhythmDodgeJudgment Grade);
	// 按评级返回显示文字
	static FString GetJudgmentText(ERhythmDodgeJudgment Grade);
	// 按评级选择文字贴图
	UTexture2D* GetJudgmentWordTexture(ERhythmDodgeJudgment Grade) const;

	UPROPERTY() TObjectPtr<UTexture2D> PlayerFrameTexture;
	UPROPERTY() TObjectPtr<UTexture2D> PlayerBeatRingTexture;
	UPROPERTY() TObjectPtr<UTexture2D> JudgmentBurstTexture;
	UPROPERTY() TObjectPtr<UTexture2D> ComboSlashTexture;
	UPROPERTY() TObjectPtr<UTexture2D> JudgmentRailTexture;
	UPROPERTY() TObjectPtr<UTexture2D> JudgmentRailBackboneTexture;
	UPROPERTY() TObjectPtr<UTexture2D> JudgmentCardTexture;
	UPROPERTY() TObjectPtr<UTexture2D> ComboPanelTexture;
	UPROPERTY() TObjectPtr<UTexture2D> PerfectWordTexture;
	UPROPERTY() TObjectPtr<UTexture2D> GoodWordTexture;
	UPROPERTY() TObjectPtr<UTexture2D> BadWordTexture;
	UPROPERTY() TObjectPtr<UTexture2D> MissWordTexture;
	UPROPERTY() TObjectPtr<UTexture2D> ComboWordTexture;
	UPROPERTY() TObjectPtr<UTexture2D> AttackTimingRailTexture;
	UPROPERTY() TObjectPtr<UTexture2D> PhaseTwoNoteAtlas;
	UPROPERTY() TObjectPtr<UTexture2D> AttackSlashTexture;
	UPROPERTY() TObjectPtr<UTexture2D> DodgeResponseTexture;
	UPROPERTY() TObjectPtr<UTexture2D> BlockResponseTexture;
	UPROPERTY() TObjectPtr<UTexture2D> CounterResponseTexture;
	UPROPERTY() TObjectPtr<UTexture2D> HoldResponseTexture;
	UPROPERTY() TObjectPtr<URhythmResponseVisualData> ResponseVisuals;
	TArray<FJudgmentFeedEntry> JudgmentFeed;
	FJudgmentFeedEntry OutgoingJudgment;
	bool bHasOutgoingJudgment = false;
	double QueueAnimationStartSeconds = -1000.0;
	double ComboPunchStartSeconds = -1000.0;
	int32 LastComboCount = INDEX_NONE;
	int32 LastComboTier = 0;
	double ComboTierUpWorldSeconds = -1000.0;
	double LastCapturedJudgmentSeconds = -1000.0;
	double FailureFeedbackStartSeconds = -1000.0;
	ERhythmDodgeJudgment FailureFeedbackGrade = ERhythmDodgeJudgment::None;
	float FailureFeedbackErrorMs = 0.0f;
	double LastPromptedImpactAudioSeconds = -1000.0;
	TWeakObjectPtr<UBeatSyncedComboComponent> CachedCueCombo;
	bool bSpectrumAnalysisStarted = false;
	bool bAreaTutorialSeen = false;
	bool bAreaWarningWasVisible = false;
	TArray<float> SmoothedSpectrum;
	float SpectrumAutoPeak = 0.01f;
};
