# 04. 굼바 스택과 Goombette

## 1. 시스템 개요

굼바 스택은 여러 `AGoombaCharacter`를 하나의 조작 단위처럼 만드는 구조다. 별도 Stack Actor를 만들지 않고 각 굼바가 위·아래 굼바의 약한 참조를 가진 연결 리스트로 구현했다.

```text
Top Goomba
  Below ──> Middle Goomba
               Below ──> Root Goomba

Root.Above   = Middle
Middle.Above = Top
```

- Root: `Below == nullptr`
- Top: `Above == nullptr`
- 스택 수: Root부터 Above를 순회하거나 현재 노드에서 연결을 따라 계산
- 캡처 대상: 스택 중 하나가 아니라 항상 Top을 반환

## 2. 왜 Top을 캡처하는가

마리오가 모자를 던졌을 때 가장 위의 굼바가 시각적으로 접근 가능하고, 스택 전체의 플레이어 대표가 되기 쉽다. 그러나 실제 이동은 Root가 담당한다.

```text
PlayerController Possess → Captured Top
입력 수신             → Captured Top
실제 AddMovement/Jump  → Stack Root
나머지 위치 정렬       → Root Tick / 체인 정렬
```

이 분리는 캡처 인터페이스의 대상과 물리적으로 바닥에 닿는 Pawn이 다르다는 문제를 해결한다.

## 3. 스택 생성 조건

스택은 일반 AI 굼바끼리 자동으로 쌓이지 않는다. 캡처된 굼바를 포함한 위쪽 스택이 낙하해 다른 스택의 Top에 닿을 때 결합한다.

조건의 핵심은 다음과 같다.

- 위쪽 스택 어딘가에 현재 캡처된 굼바가 있어야 함
- 아래쪽 대상은 다른 스택의 Top이어야 함
- 자기 스택과의 순환 연결을 만들면 안 됨
- 결합 후 아래 스택의 Top.Above와 위 스택의 Root.Below를 연결

현재 명시적인 최대 스택 높이는 없다. `GoombetteActor`의 보상 요구치는 기본 4지만 스택 자체는 그 이상도 가능하다.

## 4. 물리와 충돌 역할 분리

스택이 만들어지면 Root만 CharacterMovement로 실제 이동한다. 위의 Follower들은 다음처럼 바뀐다.

- Root에 계층적으로 붙는 효과를 내도록 매 Tick 위치/회전 정렬
- CharacterMovement 비활성화
- 캡슐은 Query/Overlap 중심으로 조정해 서로 Block하며 튀는 현상 방지
- 접촉 피해는 AI 상태의 Root만 담당

Root가 바뀌거나 스택이 분리될 수 있으므로 각 굼바는 자신의 Root와 Top을 반복 탐색하는 유틸리티를 가진다. WeakObjectPtr를 써 파괴된 노드가 강한 참조로 남는 것을 피한다.

## 5. 입력 전달

캡처된 굼바가 받는 Move/Run/Jump는 Root에 적용된다.

| 기능 | 적용 대상 | 값의 출처 |
|---|---|---|
| 이동 방향 | Root | 캡처된 굼바 입력 |
| 속도 | Root CharacterMovement | 캡처된 굼바의 걷기/달리기 설정 |
| 점프 | Root | 캡처된 굼바의 Jump 설정 |
| 카메라 | 숨겨진 Mario | 공통 캡처 Look 전달 |
| 해제 | CaptureComponent | 캡처된 굼바의 Crouch |

Root Tick은 스택 안에 플레이어가 캡처한 멤버가 있는지 검사해 전체 스택을 PlayerControlled 모드로 취급한다. 캡처 해제 시 AI/충돌/이동 상태를 다시 정리하고 일정 시간 Stunned 상태를 둔다.

## 6. 피해 전달

스택의 어느 굼바가 맞아도 플레이어가 캡처한 멤버가 있으면 그 멤버로 `ApplyDamage`를 다시 전달한다. 목적은 스택 아래쪽을 공격해도 현재 캡처 Pawn의 피격 이벤트가 발생하게 만드는 것이다.

그 결과 `UCaptureComponent`가 해당 Pawn의 `OnTakeAnyDamage`를 받고 `OnCapturedPawnDamaged`를 호출한다. 굼바 구현은 다음 반응을 Root에 적용한다.

- 입력 잠금
- 피해 방향 반대쪽 평면 넉백
- 상방 넉백
- 타이머 후 입력 복구

단, 현재 CaptureComponent가 마리오 HP를 차감하지 않으므로 결과적으로 스택 피격도 HP 감소 없이 리액션만 발생하는 것으로 확인된다.

## 7. Goombette 보상

`AGoombetteActor`는 “플레이어가 조종 중인 Pawn”이 굼바인지 검사한다. 일반 마리오나 AI 굼바의 Overlap은 무시한다.

```text
Trigger Overlap
  → GetPlayerPawn(0)와 OtherActor가 같은가
  → AGoombaCharacter인가
  → GetStackCount() >= RequiredStackCount(기본 4)인가
  → RewardMoonClass 스폰
  → Goombette Destroy
```

보상 슈퍼문은 기본적으로 `ASuperMoonPickup`이고 Goombette 위치 위 80 UU에 생성한다. 실제 MoonId는 배치 인스턴스에 지정하지 않으면 경로명으로 자동 생성된다.

## 8. 경계 조건

반드시 테스트해야 할 경우:

- 스택 중간 굼바가 Destroy될 때 위/아래 연결 복구
- Root가 파괴되거나 NavMesh 밖으로 떨어졌을 때
- 캡처 Top이 강제 해제된 직후 스택의 충돌/AI 복구
- 높은 스택이 낮은 천장이나 좁은 통로에 들어갈 때
- 두 스택을 연속으로 빠르게 합칠 때 순환 링크 방지
- 스택 어느 층에 캐피가 맞아도 Top 캡처로 일관되는지
- Goombette 보상 MoonId가 재방문 시 중복 생성되지 않는지

## 9. 구조 평가

별도 관리자 없이 각 노드가 연결 정보를 갖는 방식은 소규모 스택에는 단순하고 효과적이다. 다만 연결 갱신, 물리 대표, 캡처 대표, 피해 대표가 서로 다른 객체이므로 파괴/분리 같은 예외 경로가 늘어난다. 향후 스택 분해나 개별 사망을 추가한다면 `UGoombaStackComponent` 또는 전용 Stack Actor로 소유권을 모으는 편이 안전하다.
