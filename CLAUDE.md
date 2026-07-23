\# UE 5.7 C++ Project Rules (Manual Control Mode)



\## 1. Core Workflow Restrictions

\- \*\*No Engine Automation\*\*: 너에게는 에디터 제어나 빌드 실행 권한이 없다. 절대 `Build.bat`, `UAT` 등의 셸 스크립트 실행을 시도하지 말 것.

\- \*\*No Plan Files\*\*: `writing-plans` 플러그인을 사용하지 않는다. 계획은 파일로 저장하지 말고, 채팅창 내에서 텍스트로만 제안하고 합의할 것.

\- \*\*Manual Application\*\*: 코드는 내가 Rider에서 복사해서 직접 붙여넣기 좋게 \*\*완전한 파일 단위\*\* 또는 \*\*명확한 함수 단위\*\*의 완성된 코드 블록으로 출력할 것.



\## 2. Planning \& Review (Superpowers + Ponytail)

\- \*\*Plan Phase\*\*: 복잡한 구현 전에는 반드시 \*\*Superpowers Brainstorming\*\*을 통해 아키텍처와 논리적 오류(가비지 컬렉션, 스레드 안전성)를 먼저 검토받을 것.

\- \*\*Review Phase\*\*: 코드를 작성한 후에는 스스로 \*\*Ponytail\*\* 렌즈로 검토하여 과도한 추상화나 불필요한 바퀴를 다시 발명하는 일(Overengineering)을 방지할 것.



\## 3. Rider Environment Integration

\- \*\*Project Structure\*\*: Rider는 .uproject 또는 .sln 구조를 직접 파싱하므로 compile\_commands.json 생성 요청을 절대 하지 말 것.

\- \*\*Code Context\*\*: 헤더 인스턴스화, 리플렉션 매크로(GENERATED\_BODY), 언리얼 스마트 포인터(TSharedPtr, TWeakObjectPtr) 패턴을 Rider 가이드라인에 맞춰 정확히 인식하고 코드를 작성할 것.



\## 4. Unreal 5.7 C++ Constraints

\- \*\*Containers\*\*: STL(`std::`) 사용을 완전히 금지함. 무조건 언리얼 컨테이너(`TArray`, `TMap`, `TSet`, `FString`)만 사용할 것.

\- \*\*Memory Management\*\*: 가비지 컬렉션(GC) 크래시 방지를 위해, 모든 `UObject` 파생 클래스 포인터 멤버 변수는 반드시 `UPROPERTY()` 스마트 매크로를 누락 없이 붙일 것.

\- \*\*Optimization\*\*: 가급적 액터 틱(`AActor::Tick`) 기능을 끄고(`bCanEverTick = false`), 델리게이트나 타이머(`FTimerManager`) 위주로 설계할 것.



\## 5. Interaction Style

\- 설명은 최소화하고, 코드는 정확하게 작성할 것.

\- 코드를 출력할 때 어느 파일(`Header` vs `Cpp`)의 어느 위치(`BeginPlay` 내부 등)에 들어가야 하는지 주석으로 명시할 것.



