# H4. 보스 다중 FSM 오케스트레이션

## 1. 세 층의 상태기

Attrenashin 전투는 하나가 아니라 세 층의 상태기가 협력한다.

```text
Arena Lifecycle FSM
  └─ Boss Phase/Flow FSM
       ├─ Left Fist FSM + Slam Sub-FSM
       └─ Right Fist FSM + Slam Sub-FSM
```

| 층 | 시간 범위 | 책임 |
|---|---|---|
| Arena | 조우 전~승리/패배 | 컷신, 스폰, 정리, 카메라, BGM |
| Boss | 전투 전체/페이즈 | 손 선택, 패턴 순서, 머리 타격, 카운터 |
| Fist | 수 초 공격 단위 | 추적, 예고, 급강하, 기절, 캡처, 복귀 |

## 2. 소유권 방향

정상 명령 방향은 위에서 아래다.

```text
Arena → Boss 생성/파괴
Boss → Fist 패턴 시작/강제 복귀
Fist → Boss에 캡처/해제/얼음비 Impact 알림
Boss → Arena는 직접 알리지 않고 HeadHitCount를 노출
Arena → Tick Poll로 승리 감지
```

Fist가 Boss를 WeakPtr로 갖고, Boss도 두 Fist를 WeakPtr로 갖는다. Actor 수명은 World가 관리하고 순환 강참조를 피한다.

## 3. 주먹의 중첩 상태

일반 Slam은 FistState와 SlamSeq 두 상태가 동시에 존재한다.

```text
FistState = SlamSequence
SlamSeq   = MoveAbove | FollowAbove | PreSlamPause | SlamDown | PostImpactWait
```

State가 바뀔 때 하위 상태를 None으로 정리하지 않으면 AnimInstance와 충돌 판정이 이전 공격으로 오인할 수 있다. 실제 코드가 캡처/Abort/IceRain/복귀 진입 때 SlamSeq와 Timer를 반복 초기화하는 이유다.

## 4. 보스 Flow와 양손 동기화

Phase2/3의 ShardRain은 단순 시간 경과로 시작하지 않는다.

```text
ForceFistsReturnToAnchor
→ AreBothFistsIdle 대기
→ StartIceRainByBothFists
→ 두 손 중 하나라도 IceRainSlam이면 대기
→ 두 손 모두 Idle 확인
→ AlternatingSlamLoop
```

이렇게 실제 손 상태를 Barrier로 사용하면 충돌/캡처/복귀 지연으로 Animation 시간이 달라져도 다음 패턴이 겹치지 않는다.

반면 손이 Destroy되거나 한 손이 영구적으로 Idle에 못 들어가면 `AreBothFistsIdle()`가 계속 false라 Flow가 멈춘다. Invalid Fist를 재생성하거나 Timeout 후 복구하는 Watchdog은 없다.

## 5. 캡처가 끼어드는 비동기 이벤트

AlternatingSlamLoop 도중 손이 캡처되면 정기 패턴보다 캡처 대응이 우선한다.

```text
CapturedNow 발견
  → Fear = true
  → Retreat 시작
  → 다른 손 Abort + Barrage 시작
  → 일반 Slam Timer 갱신 중단
```

해제되면 Barrage/Retreat를 끄고 Phase1에서는 3초 후 중앙 복귀 Timer를 건다. 그 사이 다시 캡처되면 이 Timer를 취소한다.

즉 Timer callback과 Tick 상태 전이가 경쟁할 수 있으므로 `CancelPhase1CaptureFailReturnToCenter()`가 상위 페이즈 진입과 재캡처 양쪽에서 호출된다.

## 6. 머리 타격 트랜잭션

한 번의 머리 Overlap은 다음 변경을 묶는다.

1. HeadHitCount 증가
2. 현재 Phase에 따라 Phase2/3 초기화
3. Retreat/Barrage/복귀 Timer 정리
4. Captured Fist 강제 해제
5. Mario를 Arena 중앙 방향으로 Launch
6. 다음 Tick에 Arena가 Count를 읽어 승리 여부 확인

세 번째 타격에서도 Boss가 즉시 Destroy되지 않고 Arena Tick이 승리를 처리한다. 이 짧은 사이에 Fist Release 콜백이나 Phase 로직이 실행될 수 있다. `bBossDefeated`는 Arena 쪽에만 있으므로 향후 승리 연출을 넣을 때는 Boss의 공격을 즉시 정지시키는 명시적 Defeated 상태가 더 안전하다.

## 7. Phase3 박수의 좌표계

박수는 마리오 Actor의 RightVector가 아니라 다음으로 좌우축을 구한다.

```text
Dir   = Normalize2D(Player - BossHead)
Right = Normalize((-Dir.Y, Dir.X, 0))
OpenL = Player - Right * SideOffset
OpenR = Player + Right * SideOffset
```

장점:

- 마리오가 어느 방향을 보든 보스 관점 좌우가 유지
- 카메라 회전과 독립
- 원형 아레나에서 일관된 포위 패턴

Tracking이 끝나면 현재 손 위치를 Open 지점으로 Freeze하고 중앙 Close 지점을 다시 계산한다. 그 뒤 Hold 예고 → 빠른 Close → Open을 실행한다. 추적과 공격을 분리해 플레이어가 마지막 회피 방향을 선택할 수 있게 한 패턴 설계다.

## 8. 정리 순서와 재도전

플레이어 사망은 아래에서 위로 상태를 해제하기보다 Arena가 월드의 실제 Actor들을 일괄 Destroy한다.

```text
Sequence Stop
→ Boss Destroy
→ Fist 전체 Destroy
→ IceShard/Tile 전체 Destroy
→ Arena flags reset
→ Proxy restore/show
→ Camera Mario
```

장점은 중간 FSM 상태와 Timer를 일일이 되돌리지 않아도 새 Boss가 깨끗하게 시작한다는 점이다. 단점은 전역 타입 정리와 컷신 프록시/실제 Actor의 이중 관리다.

## 9. 개선 설계안

- Arena: `EEncounterState`와 `EnterEncounterState()`로 bool 조합 통제
- Boss: `EAttrenashinPhase`와 FlowState를 하나의 전이 표로 문서화/검증
- Fist: 모든 상태 진입을 `EnterState`로 통일하고 직접 `State =` 대입 제거
- Barrier Timeout: 손 복귀/Idle 대기에 최대 시간과 복구 경로 추가
- Defeated: 세 번째 타격 순간 Boss 패턴 정지, Arena에 Delegate 알림
- Ownership: Spawn한 Fist/Shard/Tile을 배열로 추적해 자기 전투만 정리
- 공간 기준: ArenaRoot Local Space로 모든 중심/높이/반대편 좌표 계산

이 변경은 패턴 자체보다 “불가능한 상태 조합과 영구 대기”를 제거하는 데 목적이 있다.
