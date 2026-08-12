// Fill out your copyright notice in the Description page of Project Settings.


#include "Interaction/OBInteractableActor.h"

#include "Blueprint/UserWidget.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Pawn.h"
#include "Player/Controller/OBPlayerController.h"
#include "Components/MeshComponent.h"
#include "Components/WidgetComponent.h"
#include "UI/HUD/OBInteractPromptWidget.h"


AOBInteractableActor::AOBInteractableActor()
{
	PrimaryActorTick.bCanEverTick = false;
	
	Range = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	SetRootComponent(Range);
	Range->InitSphereRadius(InteractRadius);
	Range->SetCollisionProfileName(TEXT("Trigger"));
	
	PromptWidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("PromptWidget"));
	PromptWidgetComp->SetupAttachment(Range);

	// Screen: 항상 카메라를 향하고 거리와 무관하게 같은 크기로 보인다.
	PromptWidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
	PromptWidgetComp->SetDrawAtDesiredSize(true);

	// 트리거 구체와 겹쳐서 오버랩을 오염시키면 안 된다.
	PromptWidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PromptWidgetComp->SetGenerateOverlapEvents(false);

	// 다가오기 전에는 숨는다. 컨트롤러가 최근접 하나에만 켠다.
	PromptWidgetComp->SetVisibility(false);
}

void AOBInteractableActor::BeginPlay()
{
	Super::BeginPlay();
	
	RefreshPromptText();
	
	Range->SetSphereRadius(InteractRadius);
	Range->OnComponentBeginOverlap.AddDynamic(this, &AOBInteractableActor::OnRangeBeginOverlap);
	Range->OnComponentEndOverlap.AddDynamic(this, &AOBInteractableActor::OnRangeEndOverlap);

	// World Partition 맵에서는 플레이어가 이미 구체 안에 있는 상태로 셀이 스트림인될 수 있다.
	// 그때는 BeginOverlap이 영원히 오지 않으므로 지금 겹친 폰을 직접 훑는다.
	TArray<AActor*> Overlapping;
	Range->GetOverlappingActors(Overlapping, APawn::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		OnRangeBeginOverlap(Range, Actor, nullptr, 0, false, FHitResult());
	}
}

void AOBInteractableActor::OnRangeBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	AOBPlayerController* PC = Pawn ? Cast<AOBPlayerController>(Pawn->GetController()) : nullptr;
	
	// 목록에 넣기만 한다. 겹친 상자 중 무엇을 고를지는 컨트롤러가 거리로 판단한다.
	if (PC && PC->IsLocalController())
	{
		PC->AddNearbyInteractable(this);
	}
}

void AOBInteractableActor::OnRangeEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn) return;
	
	if (AOBPlayerController* PC = Cast<AOBPlayerController>(Pawn->GetController()))
	{
		if (PC->IsLocalController())
		{
			PC->RemoveNearbyInteractable(this);
		}
	}
}

void AOBInteractableActor::Interact_Implementation(AOBPlayerController* PC)
{
	// 기본: 지정 위젯을 컨트롤러가 오픈(커서/이동잠금 포함)
	if (PC && InteractWidgetClass)
	{
		PC->OpenInteractionWidget(InteractWidgetClass);
	}
}

// 파일 끝에 추가

FText AOBInteractableActor::GetInteractPromptText_Implementation() const
{
	return InteractPromptText.IsEmpty() ? NSLOCTEXT("OBInteraction", "DefaultPrompt", "상호작용") : InteractPromptText;
}

void AOBInteractableActor::SetHighlighted(bool bHighlighted)
{
	if (PromptWidgetComp)
	{
		PromptWidgetComp->SetVisibility(bHighlighted);
		if (bHighlighted)
		{
			RefreshPromptText();
		}
	}

	if (!HighlightOverlayMaterial) return;

	TArray<UMeshComponent*> Meshes;
	GetComponents<UMeshComponent>(Meshes);

	for (UMeshComponent* Mesh : Meshes)
	{
		if (Mesh)
		{
			Mesh->SetOverlayMaterial(bHighlighted ? HighlightOverlayMaterial : nullptr);
		}
	}
}

void AOBInteractableActor::RefreshPromptText()
{
	if (!PromptWidgetComp) return;

	// 위젯 클래스가 지정 안 됐거나 데디케이티드 서버면 null이다. 정상.
	if (UOBInteractPromptWidget* Prompt = Cast<UOBInteractPromptWidget>(PromptWidgetComp->GetUserWidgetObject()))
	{
		Prompt->SetPromptText(GetInteractPromptText());
	}
}
