# Unreal Editor 실행 크래시 원인 분석 보고서

## 1. 문제 요약

에디터 실행 중 `UHordeProxySubsystem::Initialize()` 단계에서 다음 Fatal Error가 발생한다.

```text
Fatal error:
No object initializer found during construction.
```

콜스택의 핵심 구간은 다음과 같다.

```text
UObject::CreateDefaultSubobject()
UHordeProxySubsystem::Initialize()
FSubsystemCollectionBase::AddAndInitializeSubsystem()
UWorld::InitWorld()
```

즉, 월드 초기화 과정에서 `UHordeProxySubsystem`이 초기화되다가 `CreateDefaultSubobject()` 호출로 크래시가 발생하고 있다.

## 2. 직접 원인

문제가 되는 코드는 다음 위치에 있다.

```cpp
// Source/OutBreak/Private/FlowField/Subsystem/HordeProxySubsystem.cpp

void UHordeProxySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	MovementSubsystem = Collection.InitializeDependency<UHordeMovementSubsystem>();

	const UFlowFieldSettings* Settings = GetDefault<UFlowFieldSettings>();
	USkeletalMesh* SkeletalMesh = Settings->GetProxySkeletalMesh();

	HordeProxy = CreateDefaultSubobject<UInstancedSkinnedMeshComponent>(TEXT("HordeProxy"));
}
```

`CreateDefaultSubobject()`는 UObject 또는 Actor의 생성자에서만 사용해야 한다.  
하지만 현재 코드는 `UWorldSubsystem`의 `Initialize()` 함수 안에서 호출하고 있다.

이 시점에는 Unreal의 `FObjectInitializer`가 존재하지 않기 때문에 엔진 내부에서 다음 오류가 발생한다.

```text
No object initializer found during construction.
```

## 3. Unreal Engine 동작 관점에서의 설명

`CreateDefaultSubobject()`는 클래스 기본 객체, 생성자, 리플렉션 기반 기본 서브오브젝트 구성 흐름과 연결되어 있다.

정상 사용 예시는 다음과 같은 Actor 생성자 내부 호출이다.

```cpp
AModularSkeletalMeshProxyActor::AModularSkeletalMeshProxyActor()
{
	ABAHead = CreateDefaultSubobject<USkeletalMeshComponentBudgeted>(TEXT("ABA Head"));
	SetRootComponent(ABAHead);
}
```

반면 `UHordeProxySubsystem::Initialize()`는 생성자가 아니다.  
서브시스템 객체가 이미 생성된 뒤, 월드 초기화 과정에서 호출되는 일반 초기화 함수다.

따라서 `Initialize()` 내부에서 default subobject를 만들면 안 된다.

## 4. 현재 코드의 구조적 문제

`UHordeProxySubsystem`은 `UWorldSubsystem`이다.

```cpp
UCLASS()
class OUTBREAK_API UHordeProxySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

	virtual auto Initialize(FSubsystemCollectionBase& Collection) -> void override;

private:
	UPROPERTY(Transient)
	TObjectPtr<class UInstancedSkinnedMeshComponent> HordeProxy;
};
```

그런데 `UInstancedSkinnedMeshComponent`는 씬 컴포넌트 계열로, 일반적으로 Actor에 붙어서 월드에 등록되어야 한다.  
서브시스템이 직접 `CreateDefaultSubobject()`로 컴포넌트를 소유하려는 구조는 Unreal의 컴포넌트 생명주기와 맞지 않는다.

## 5. 권장 수정 방향

### 방향 A: Proxy Actor를 만들고 생성자에서 컴포넌트 생성

가장 Unreal다운 방식이다.

1. `AHordeProxyActor` 같은 Actor 클래스를 만든다.
2. Actor 생성자에서 `CreateDefaultSubobject<UInstancedSkinnedMeshComponent>()`를 호출한다.
3. `UHordeProxySubsystem::Initialize()`에서는 해당 Actor를 월드에 Spawn한다.
4. 서브시스템은 Actor 또는 Component 참조만 보관한다.

예상 구조:

```cpp
AHordeProxyActor::AHordeProxyActor()
{
	HordeProxy = CreateDefaultSubobject<UInstancedSkinnedMeshComponent>(TEXT("HordeProxy"));
	SetRootComponent(HordeProxy);
}
```

```cpp
void UHordeProxySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	MovementSubsystem = Collection.InitializeDependency<UHordeMovementSubsystem>();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ProxyActor = World->SpawnActor<AHordeProxyActor>();
}
```

### 방향 B: Initialize에서 NewObject로 동적 컴포넌트 생성

서브시스템 초기화 시점에 반드시 컴포넌트를 만들어야 한다면 `CreateDefaultSubobject()`가 아니라 `NewObject()`를 사용해야 한다.

다만 이 경우에도 컴포넌트는 월드에 존재하는 Actor를 Outer로 삼고, 적절히 등록해야 한다.

예상 구조:

```cpp
HordeProxy = NewObject<UInstancedSkinnedMeshComponent>(OwnerActor, TEXT("HordeProxy"));
HordeProxy->RegisterComponent();
```

이 방식은 Owner Actor, Register, Attach, Destroy 시점 관리를 직접 챙겨야 하므로 방향 A보다 실수 여지가 크다.

## 6. 결론

이번 에디터 실행 크래시의 직접 원인은 다음 코드다.

```cpp
HordeProxy = CreateDefaultSubobject<UInstancedSkinnedMeshComponent>(TEXT("HordeProxy"));
```

이 호출이 `UHordeProxySubsystem::Initialize()` 안에 있기 때문에 Unreal이 필요한 object initializer를 찾지 못하고 크래시가 발생한다.

수정은 `CreateDefaultSubobject()` 호출을 생성자 내부로 옮기거나, 런타임 생성이 필요한 경우 `NewObject()` 기반으로 변경해야 한다.  
현재 구조에서는 별도 Proxy Actor를 만들고 그 Actor가 `UInstancedSkinnedMeshComponent`를 소유하게 하는 방향이 가장 적합하다.
