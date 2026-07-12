# Windows Macro Pollution Audit

Date: 2026-07-12

Project: `C:\UEProject\OutBreak`

## 1. 오류의 전처리 단계 해석

이번 오류는 C++ 이름 조회 문제가 아니라 C/C++ 전처리기 단계의 매크로 치환 문제다.

`std::numeric_limits<uint16>::max()`는 토큰 기준으로 `max` 뒤에 `(`가 오므로, `max(a,b)` 함수형 매크로가 살아 있으면 전처리기가 앞의 `std::numeric_limits<uint16>::`를 고려하지 않고 `max()`를 매크로 호출로 해석한다. 그래서 인수 2개를 기대하는 Windows `max(a,b)` 매크로에 인수 0개가 전달된 것으로 보고 C4003이 발생한다.

확인된 오류 지점:

```text
C:\Program Files\Epic Games\UE_5.7\Engine\Source\Runtime\FieldNotification\Public\FieldNotificationDelegate.h(153,55)
uint16 AddedEmplaceAt = std::numeric_limits<uint16>::max();
```

## 2. `std::numeric_limits<uint16>::max()`가 왜 매크로 충돌을 일으키는지

Windows SDK의 `minwindef.h`는 `NOMINMAX`가 없으면 다음 매크로를 정의한다.

```cpp
#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))
```

전처리기는 범위 지정 연산자 `::`를 보고 멤버 함수인지 판단하지 않는다. 따라서 `std::numeric_limits<uint16>::max()` 안의 소문자 `max`가 그대로 매크로 확장 대상이 된다.

## 3. `FMath::Max`가 직접 충돌하지 않는 이유

Windows 매크로 이름은 소문자 `max`이고, `FMath::Max`는 대문자 `Max`다. 매크로 이름은 대소문자를 구분하므로 `FMath::Max(...)` 자체는 Windows `max(a,b)` 매크로와 직접 충돌하지 않는다.

프로젝트 내 `FMath::Max`/`FMath::Min` 사용처는 다수 있었지만, 이번 C4003의 직접 원인은 아니었다.

## 4. 발견된 Windows 관련 include 전체 목록

프로젝트 `Source`/`Config`에서 현재 남아 있는 직접 Windows SDK include는 없다.

수정 전 실제 유입 지점은 다음 한 줄이었다.

```cpp
// Source/OutBreak/Private/FlowField/Subsystem/HordeProxySubsystem.cpp:20, HEAD 기준
#include "../../../../../../../Program Files/Epic Games/UE_5.7/Engine/Plugins/Media/BlackmagicMedia/Source/ThirdParty/Build/Include/DeckLinkAPI_h.h"
```

이 파일의 첫 부분은 다음처럼 Unreal wrapper 없이 Windows RPC 헤더를 직접 포함한다.

```cpp
#include "rpc.h"
#include "rpcndr.h"
```

`/showIncludes`로 확인한 실제 유입 chain:

```text
Module.OutBreak.6.cpp
-> Source/OutBreak/Private/FlowField/Subsystem/HordeProxySubsystem.cpp
-> Engine/Plugins/Media/BlackmagicMedia/Source/ThirdParty/Build/Include/DeckLinkAPI_h.h
-> Windows Kits/10/include/10.0.22621.0/shared/rpc.h
-> Windows Kits/10/include/10.0.22621.0/um/windows.h
-> Windows Kits/10/include/10.0.22621.0/shared/minwindef.h
```

## 5. 발견된 `max`/`min`/`Interlocked` 관련 매크로 목록

Windows SDK 쪽 정의:

```text
minwindef.h:
  #define max(a,b)
  #define min(a,b)

winnt.h:
  #define InterlockedIncrement _InterlockedIncrement
  #define InterlockedDecrement _InterlockedDecrement
  #define InterlockedExchange _InterlockedExchange
  #define InterlockedCompareExchange _InterlockedCompareExchange
  #define InterlockedCompareExchange64 _InterlockedCompareExchange64
  #define InterlockedCompareExchangePointer _InterlockedCompareExchangePointer
```

Unreal wrapper가 정상 경로로 사용될 때 정리하는 매크로:

```text
Windows/PostWindowsApi.h:
  #undef InterlockedIncrement
  #undef InterlockedDecrement
  #undef InterlockedExchange
  #undef InterlockedCompareExchange
  #undef InterlockedCompareExchange64
  ...

Windows/HideWindowsPlatformAtomics.h:
  #undef InterlockedIncrement
  #undef InterlockedDecrement
  #undef InterlockedExchange
  #undef InterlockedCompareExchange
  ...
```

## 6. Public 헤더에서 외부로 노출되는 위험 include

현재 프로젝트 `Source\OutBreak\Public`에서는 `Windows.h`, `DeckLinkAPI_h.h`, `PBDRigidsSOAs.h`, `AllowWindowsPlatformTypes.h`, `AllowWindowsPlatformAtomics.h` 계열 직접 include를 발견하지 못했다.

따라서 이번 매크로 오염은 Public 헤더 전파가 아니라 Unity Translation Unit 안의 `.cpp` include 순서 전파였다.

## 7. Unity Translation Unit별 원본 `.cpp` 구성

오류가 난 TU는 다음 파일이다.

```text
C:\UEProject\OutBreak\Intermediate\Build\Win64\x64\UnrealEditor\Development\OutBreak\Module.OutBreak.6.cpp
```

해당 TU의 관련 순서:

```text
... generated cpp files
Source/OutBreak/OutBreak.cpp
Source/OutBreak/Private/Ability/...
Source/OutBreak/Private/FlowField/Subsystem/HordeProxySubsystem.cpp
  -> DeckLinkAPI_h.h
  -> rpc.h
  -> windows.h
  -> minwindef.h / winnt.h
Source/OutBreak/Private/FlowField/Subsystem/HordeStatusSubsystem.cpp
...
Source/OutBreak/Private/UI/HUD/OBHUD.cpp
  -> OBHealthViewModel.h
  -> MVVMViewModelBase.h
  -> MVVMFieldNotificationDelegates.h
  -> FieldNotificationDelegate.h
  -> C4003 at std::numeric_limits<uint16>::max()
```

`Module.OutBreak.1.cpp`부터 `Module.OutBreak.5.cpp`, `Module.OutBreak.7.cpp`는 같은 실패를 재현하지 않았다. 실제 실패 전파는 `Module.OutBreak.6.cpp` 내부 순서에 종속됐다.

## 8. 최초 유입 지점의 정확한 파일과 줄

Primary 유입 지점:

```text
C:\UEProject\OutBreak\Source\OutBreak\Private\FlowField\Subsystem\HordeProxySubsystem.cpp:20
```

수정 전 코드:

```cpp
#include "../../../../../../../Program Files/Epic Games/UE_5.7/Engine/Plugins/Media/BlackmagicMedia/Source/ThirdParty/Build/Include/DeckLinkAPI_h.h"
```

`git blame HEAD` 기준 유입 커밋:

```text
c95038d7 UPDATE: Horde Proxy SubSystem
2026-07-12 04:04:08 +0900
```

보조 위험 지점:

```text
C:\UEProject\OutBreak\Source\OutBreak\Private\UI\Widgets\Lobby\LoadoutWidget\LoadoutSelectionView.cpp:6
#include "Chaos/PBDRigidsSOAs.h"
```

이 줄은 감사 시작 시점의 working tree에서 이미 제거되어 있었다. `git blame HEAD` 기준 유입 커밋은 `84b01f28 UPDATE: MISC`다. 이 include는 `PBDRigidsSOAs.h` 오류를 직접 노출시키는 혼동 요인이지만, 제거된 상태에서도 `FieldNotificationDelegate.h`의 `max` 오류가 남았으므로 primary root cause는 아니었다.

## 9. 최초 유입 지점에서 엔진 오류 지점까지의 include chain

확정된 chain:

```text
Module.OutBreak.6.cpp
-> HordeProxySubsystem.cpp
-> DeckLinkAPI_h.h
-> rpc.h
-> windows.h
-> minwindef.h defines max/min
-> winnt.h defines Interlocked* macros
-> later in same Unity TU:
-> OBHUD.cpp
-> OBHealthViewModel.h
-> MVVMViewModelBase.h
-> MVVMFieldNotificationDelegates.h
-> FieldNotificationDelegate.h
-> std::numeric_limits<uint16>::max() is parsed as macro call
```

## 10. 가능성이 가장 높은 근본 원인

`HordeProxySubsystem.cpp`가 BlackmagicMedia 플러그인의 MIDL 생성 헤더 `DeckLinkAPI_h.h`를 절대 경로성 상대 경로로 직접 include했다. 이 헤더는 Unreal의 Windows wrapper 없이 `rpc.h`/`windows.h` 계열을 끌어오며, 결과적으로 `max`, `min`, `Interlocked*` 매크로가 Unity TU 안에 남았다.

이후 같은 Unity TU에서 MVVM/FieldNotification 헤더가 포함되면서 엔진 코드의 정상적인 `std::numeric_limits<uint16>::max()`가 깨졌다.

## 11. 근거와 신뢰도

근거:

```text
rg:
  프로젝트 Source에서 DeckLinkAPI_h.h 직접 include는 HordeProxySubsystem.cpp 한 곳뿐.

/showIncludes:
  HordeProxySubsystem.cpp -> DeckLinkAPI_h.h -> rpc.h -> windows.h -> minwindef.h
  이후 OBHUD.cpp -> OBHealthViewModel.h -> MVVMFieldNotificationDelegates.h -> FieldNotificationDelegate.h에서 C4003 발생.

사용 여부 검색:
  DeckLink/IDeckLink/BMD 관련 타입은 프로젝트 소스에서 해당 include 줄 외에 사용되지 않음.

검증:
  DeckLinkAPI_h.h include 제거 후 일반 빌드, 강제 Unity 빌드, Non-Unity 빌드 모두 성공.
```

신뢰도: 높음.

## 12. 최소 수정안

사용되지 않는 include였으므로 제거가 최소 수정이다.

수정 파일:

```text
C:\UEProject\OutBreak\Source\OutBreak\Private\FlowField\Subsystem\HordeProxySubsystem.cpp
```

변경:

```diff
-#include "../../../../../../../Program Files/Epic Games/UE_5.7/Engine/Plugins/Media/BlackmagicMedia/Source/ThirdParty/Build/Include/DeckLinkAPI_h.h"
```

## 13. 구조적으로 올바른 장기 수정안

DeckLink API가 실제로 필요하다면 현재처럼 Engine 설치 경로를 프로젝트 `.cpp`에서 직접 상대 경로로 include하면 안 된다.

권장 방향:

```cpp
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/AllowWindowsPlatformAtomics.h"

THIRD_PARTY_INCLUDES_START
#include "DeckLinkAPI_h.h"
THIRD_PARTY_INCLUDES_END

#include "Windows/HideWindowsPlatformAtomics.h"
#include "Windows/HideWindowsPlatformTypes.h"
```

또는 BlackmagicMedia 플러그인의 기존 패턴을 따라 모듈 의존성과 include path를 정식으로 추가하고, 필요한 구현 파일의 private 영역에만 격리한다. 실제 엔진 플러그인 `BlackmagicCore\Private\Common.h`는 이 wrapper 패턴을 이미 사용하고 있다.

DeckLink 타입을 Public 헤더에 노출해야 한다면 forward declaration 또는 PImpl로 숨기고, Windows/COM 헤더는 Private `.cpp`에만 둬야 한다.

## 14. 수정 후 영향받을 파일 목록

직접 수정:

```text
C:\UEProject\OutBreak\Source\OutBreak\Private\FlowField\Subsystem\HordeProxySubsystem.cpp
```

감사 시작 전 이미 working tree에서 수정되어 있던 파일:

```text
C:\UEProject\OutBreak\Source\OutBreak\Private\UI\Widgets\Lobby\LoadoutWidget\LoadoutSelectionView.cpp
```

이 파일은 `#include "Chaos/PBDRigidsSOAs.h"` 제거 상태였다. 이번 감사에서 primary 원인으로 확정된 수정은 `HordeProxySubsystem.cpp`의 DeckLink include 제거다.

## 15. 엔진 파일을 수정하면 안 되는 이유

`FieldNotificationDelegate.h`, `PBDRigidsSOAs.h`, `WindowsPlatformAtomics.h`는 증상이 드러난 위치이지 최초 유입 지점이 아니다.

엔진 파일에서 `std::numeric_limits<uint16>::max()`를 `(std::numeric_limits<uint16>::max)()`로 바꾸거나, 전역 `#undef max`를 추가하는 것은 원인을 가리는 임시 우회다. 문제는 프로젝트 `.cpp`가 Unreal wrapper 없이 Windows/COM SDK 헤더를 Unity TU에 누출한 것이므로, 프로젝트 include 경계를 고치는 것이 맞다.

## 결론

```text
Primary root cause:
HordeProxySubsystem.cpp가 Blackmagic DeckLink MIDL 헤더를 Unreal Windows wrapper 없이 직접 include하여 Windows SDK max/min/Interlocked 매크로를 Module.OutBreak.6.cpp Unity TU에 누출함.

최초 매크로 유입 파일:
C:\UEProject\OutBreak\Source\OutBreak\Private\FlowField\Subsystem\HordeProxySubsystem.cpp

최초 매크로 유입 줄:
수정 전 HEAD 기준 20번 줄

유입된 매크로:
max, min, InterlockedIncrement, InterlockedDecrement, InterlockedExchange, InterlockedCompareExchange, InterlockedCompareExchange64, InterlockedCompareExchangePointer

영향받은 Translation Unit:
C:\UEProject\OutBreak\Intermediate\Build\Win64\x64\UnrealEditor\Development\OutBreak\Module.OutBreak.6.cpp

오염 전파 경로:
HordeProxySubsystem.cpp -> DeckLinkAPI_h.h -> rpc.h -> windows.h -> minwindef.h/winnt.h -> later OBHUD.cpp -> OBHealthViewModel.h -> MVVMFieldNotificationDelegates.h -> FieldNotificationDelegate.h

권장 최소 수정:
사용되지 않는 DeckLinkAPI_h.h 직접 include 제거.

권장 구조 수정:
DeckLink API가 필요할 때는 정식 module dependency/include path를 사용하고, private 구현부에서 Windows/AllowWindowsPlatformTypes.h, Windows/AllowWindowsPlatformAtomics.h, THIRD_PARTY_INCLUDES_START/END, Windows/HideWindowsPlatformAtomics.h, Windows/HideWindowsPlatformTypes.h로 최소 범위 격리.

진단 신뢰도:
높음. /showIncludes로 최초 Windows SDK 유입과 오류 발생 지점을 확인했고, include 제거 후 일반/강제 Unity/Non-Unity 빌드가 모두 성공함.
```

## 검증 결과

성공:

```text
OutBreakEditor Win64 Development
Build.bat OutBreakEditor Win64 Development -Project=C:\UEProject\OutBreak\OutBreak.uproject -WaitMutex
Result: Succeeded
```

성공:

```text
Force Unity
Build.bat OutBreakEditor Win64 Development -Project=C:\UEProject\OutBreak\OutBreak.uproject -WaitMutex -ForceUnity -DisableAdaptiveUnity
Result: Succeeded
```

성공:

```text
Non-Unity
Build.bat OutBreakEditor Win64 Development -Project=C:\UEProject\OutBreak\OutBreak.uproject -WaitMutex -DisableUnity
Result: Succeeded
```

