// 验证行走速度与混合空间半径的换算，以及超速和缺少快跑圈的边界
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/Player/PlayerLocomotionComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearBladeLocomotionTest, "HearBlade.Combat.LocomotionSpeed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHearBladeLocomotionTest::RunTest(const FString&)
{
    auto* Locomotion = NewObject<UPlayerLocomotionComponent>();
    // 先验证正常范围内可互相还原，再检查越界速度与缺失快跑圈
    for (float Speed : {0.f, 85.f, 170.f, 275.f, 380.f, 470.f, 560.f})
    {
        const float Radius = Locomotion->RadiusForSpeed(Speed);
        TestTrue(TEXT("采样半径与动画速度可以互相还原"),
            FMath::IsNearlyEqual(Locomotion->SpeedForRadius(Radius), Speed, .01f));
    }
    TestEqual(TEXT("超过快跑速度不越出混合空间"), Locomotion->RadiusForSpeed(900.f), Locomotion->FastRadius);
    Locomotion->FastRadius = Locomotion->RunRadius;
    TestEqual(TEXT("没有快跑圈时停在跑步圈"), Locomotion->RadiusForSpeed(560.f), Locomotion->RunRadius);
    return true;
}
#endif
