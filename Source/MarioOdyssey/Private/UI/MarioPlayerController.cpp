#include "UI/MarioPlayerController.h"

#include "UI/MarioHUDWidget.h"
#include "Progress/MarioGameInstance.h"
#include "UObject/ConstructorHelpers.h"

AMarioPlayerController::AMarioPlayerController()
{
	// 기본 HUD 위젯: /Game/_BP/UI/WBP_HUD
	static ConstructorHelpers::FClassFinder<UMarioHUDWidget> HudBP(TEXT("/Game/_BP/UI/WBP_HUD"));
	if (HudBP.Succeeded())
	{
		HUDWidgetClass = HudBP.Class;
	}
}

void AMarioPlayerController::BeginPlay()
{
	Super::BeginPlay();

	CreateHUDIfNeeded();

	if (UMarioGameInstance* GI = GetGameInstance<UMarioGameInstance>())
	{
		BindToGameInstance(GI);
		PushAllToHUD(GI);
	}
}

void AMarioPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	CreateHUDIfNeeded();

	if (UMarioGameInstance* GI = GetGameInstance<UMarioGameInstance>())
	{
		BindToGameInstance(GI);
		PushAllToHUD(GI);
	}
}

void AMarioPlayerController::OnUnPossess()
{
	// 캡쳐/전환 등으로 Pawn이 바뀌어도 HUD는 GI 기준으로 유지한다.
	Super::OnUnPossess();
}

void AMarioPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromGameInstance();
	Super::EndPlay(EndPlayReason);
}

void AMarioPlayerController::CreateHUDIfNeeded()
{
	if (HUDWidget)
	{
		return;
	}
	if (!HUDWidgetClass)
	{
		return;
	}

	HUDWidget = CreateWidget<UMarioHUDWidget>(this, HUDWidgetClass);
	if (HUDWidget)
	{
		HUDWidget->AddToViewport();
	}
}

void AMarioPlayerController::BindToGameInstance(UMarioGameInstance* GI)
{
	if (!GI) return;
	if (BoundGI.Get() == GI) return;

	UnbindFromGameInstance();
	BoundGI = GI;

	GI->OnHPChanged.AddDynamic(this, &AMarioPlayerController::HandleHPChanged);
	GI->OnCoinsChanged.AddDynamic(this, &AMarioPlayerController::HandleCoinsChanged);
	GI->OnSuperMoonCountChanged.AddDynamic(this, &AMarioPlayerController::HandleSuperMoonCountChanged);
	GI->OnSuperMoonCollected.AddDynamic(this, &AMarioPlayerController::HandleSuperMoonCollected);
}

void AMarioPlayerController::UnbindFromGameInstance()
{
	if (UMarioGameInstance* GI = BoundGI.Get())
	{
		GI->OnHPChanged.RemoveDynamic(this, &AMarioPlayerController::HandleHPChanged);
		GI->OnCoinsChanged.RemoveDynamic(this, &AMarioPlayerController::HandleCoinsChanged);
		GI->OnSuperMoonCountChanged.RemoveDynamic(this, &AMarioPlayerController::HandleSuperMoonCountChanged);
		GI->OnSuperMoonCollected.RemoveDynamic(this, &AMarioPlayerController::HandleSuperMoonCollected);
	}
	BoundGI.Reset();
}

void AMarioPlayerController::PushAllToHUD(UMarioGameInstance* GI)
{
	if (!HUDWidget || !GI)
	{
		return;
	}

	const float MaxHP = GI->GetMaxHP();
	const float CurrentHP = GI->HasSavedHP() ? GI->GetCurrentHP() : MaxHP;

	HUDWidget->BP_OnHPChanged(CurrentHP, MaxHP);
	HUDWidget->BP_OnCoinsChanged(GI->GetCoins());
	HUDWidget->BP_OnSuperMoonCountChanged(GI->GetSuperMoonCount());
}

void AMarioPlayerController::HandleHPChanged(float CurrentHP, float MaxHP)
{
	if (HUDWidget)
	{
		HUDWidget->BP_OnHPChanged(CurrentHP, MaxHP);
	}
}

void AMarioPlayerController::HandleCoinsChanged(int32 Coins)
{
	if (HUDWidget)
	{
		HUDWidget->BP_OnCoinsChanged(Coins);
	}
}

void AMarioPlayerController::HandleSuperMoonCountChanged(int32 SuperMoons)
{
	if (HUDWidget)
	{
		HUDWidget->BP_OnSuperMoonCountChanged(SuperMoons);
	}
}

void AMarioPlayerController::HandleSuperMoonCollected(FName MoonId, int32 TotalSuperMoons)
{
	if (HUDWidget)
	{
		HUDWidget->BP_OnSuperMoonCollected(MoonId, TotalSuperMoons);
	}
}
