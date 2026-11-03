// 生成初始战斗界面蓝图并保存，保留已有布局资产
#pragma once
#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "HearBladeBuildUICommandlet.generated.h"

// 首次生成战斗界面蓝图并保存，已有资源时拒绝覆盖设计器改动
UCLASS()

class UHearBladeBuildUICommandlet : public UCommandlet
{
	GENERATED_BODY()
  public:
	UHearBladeBuildUICommandlet();
	// 首次生成战斗界面蓝图，已有资源时拒绝覆盖，编译通过后保存
	virtual int32 Main(const FString& Params) override;
};
