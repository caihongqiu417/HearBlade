// 生成初始战斗界面蓝图并保存，保留已有布局资产
#pragma once
#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "HearBladeBuildUICommandlet.generated.h"

UCLASS()

class UHearBladeBuildUICommandlet : public UCommandlet
{
	GENERATED_BODY()
  public:
	UHearBladeBuildUICommandlet();
	virtual int32 Main(const FString& Params) override;
};
