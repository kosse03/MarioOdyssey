# H2. Possess 기반 캡처 전환

## 1. 세 객체의 역할 분리

캡처 중에는 “조작”, “카메라”, “플레이어 데이터”가 서로 다른 객체에 있다.

| 관심사 | 소유 객체 |
|---|---|
| 입력을 받는 Pawn | 캡처된 Monster Pawn |
| 카메라 ViewTarget | 숨겨진 MarioCharacter |
| HP/코인/슈퍼문 | MarioGameInstance |
| HUD | MarioPlayerController |
| 전환 조정 | CaptureComponent |

이 분리가 캡처 시스템의 핵심이다. HUD와 진행도를 Mario Pawn에 붙였다면 Possess가 바뀔 때 모두 끊겼을 것이다.

## 2. 진입 트랜잭션

캡처는 중간 실패가 위험한 다단계 전환이다.

```text
검증
  CanBeCaptured
  CapturePawn 유효
  PlayerController 유효

대상 준비
  데미지 Delegate 바인딩
  Target.OnCaptured

마리오 정지
  Mario.OnCaptureBegin
  Hidden = true
  Collision = false
  MovementMode = None
  Target에 Attach

권한/카메라 이동
  AutoManageActiveCameraTarget = false
  PC.Possess(Target)
  PC.SetViewTarget(Mario)
```

검증이 끝나기 전에 마리오를 숨기거나 Controller를 넘기면 실패 시 복구가 복잡해진다. 현재 구현은 주요 유효성 검사를 먼저 끝낸 뒤 상태를 변경한다.

## 3. 왜 ViewTarget을 Mario로 유지하는가

대안은 각 캡처 Pawn마다 Camera/SpringArm을 갖는 것이다. 그러나 그러면 몬스터마다 다음을 맞춰야 한다.

- 카메라 거리와 충돌
- 현재 ControlRotation 연속성
- 캡처 순간 Blend
- Look 감도
- BGM Listener 위치

현재 방식은 Mario의 카메라 Rig 하나를 재사용한다. Mario를 캡처 Pawn에 Attach하므로 카메라 원점도 자연스럽게 따라간다. Monster의 Look 입력만 Mario로 전달하면 된다.

단, Mario Capsule 충돌과 Mesh가 꺼져도 Actor Transform과 Camera Component는 계속 Tick 가능해야 한다.

## 4. Controller 자동 카메라와의 경쟁

Unreal의 PlayerController는 Possess할 때 새 Pawn을 자동 ViewTarget으로 선택할 수 있다. 캡처는 이를 원하지 않으므로:

- `bAutoManageActiveCameraTarget = false`
- Possess 직후 `SetViewTarget(Mario)`
- CaptureComponent Tick에서도 ViewTarget이 바뀌었으면 다시 Mario로 설정

세 단계 방어를 사용한다. Level Sequence나 다른 시스템이 ViewTarget을 바꾸는 경우 CaptureComponent가 다음 Tick에 되돌릴 수 있으므로, 캡처 중 컷신을 재생하려면 카메라 소유권 우선순위를 별도로 정해야 한다.

## 5. 해제 위치 문제

Monster Pawn 중심에서 바로 Mario Collision을 켜면 다음 문제가 생긴다.

- 몬스터 Capsule과 겹침
- 벽 내부
- 낮은 천장
- 굼바 스택 중간
- NavMesh 밖 또는 공중

현재 해제 알고리즘은 전방+상방 후보의 캡슐 공간 검사 → NavMesh Projection → Z +200 fallback 순이다.

개선한다면 여러 방사 방향 후보를 점수화하고 바닥 Trace, 낙하 높이, 카메라 가시성까지 평가할 수 있다. 보스 주먹처럼 고정 공중 Z에서 해제되는 대상은 NavMesh만으로 충분하지 않을 수 있다.

## 6. 해제 트랜잭션

```text
OnTakeAnyDamage 해제
→ 출구 위치 계산
→ Mario Detach/표시/Collision 복구
→ PC.Possess(Mario)
→ Auto camera 복구
→ ViewTarget Mario
→ Mario.OnCaptureEnd
→ Target AIController 복구
→ Target.OnReleased
→ 1초 PostCaptureInvulnerability
```

Monster의 `OnReleased`보다 마리오 Possess 복구가 먼저다. 파생 몬스터가 해제 콜백에서 Destroy되거나 Controller를 바꿔도 플레이어가 고립되지 않게 하는 순서다.

## 7. 실패/강제 종료 경로

| 경로 | ReleaseReason | 주의점 |
|---|---|---|
| 플레이어 Crouch | Manual | 정상 출구 계산 |
| 보스 카운터 3회 | Manual | 주먹 고정 Z에서 복귀 |
| 마리오 HP 0 | GameOver | 사망 시퀀스와 입력 잠금 순서 |
| Captured Pawn 무효/파괴 | InvalidPawn | 참조가 사라져도 Mario 복구 |
| BulletBill 충돌 | Manual 후 Destroy | Release가 Destroy보다 먼저 |

모든 경로가 같은 `ReleaseCapture`로 모이는 것이 중요하다. 파생 Pawn이 Controller를 직접 Possess 변경하거나 Mario를 직접 표시하면 전환 불변식이 깨진다.

## 8. 피해 라우팅의 올바른 경계

피해 이벤트는 Captured Pawn에서 발생하지만 생명 자원은 Mario에 있다. 안전한 설계는 다음처럼 역할을 나누는 것이다.

```text
CapturedPawn.TakeDamage
  → Pawn 고유 피격 리액션
  → CaptureComponent.RouteDamageToMario
       → 중복/무적/사망 검사
       → Mario의 캡처 전용 HP 감소 API
```

현재 첫 번째 리액션까지만 있고 두 번째 HP 전달이 빠져 있다. 일반 `Mario.TakeDamage`는 캡처 중 피해를 의도적으로 무시하므로 그대로 호출하는 것만으로는 해결되지 않는다.

## 9. 네트워크 관점

프로젝트는 현재 로컬 싱글 플레이를 전제로 작성됐다. 첫 PlayerController, PlayerCharacter index 0, 전역 GameInstance를 직접 사용한다.

멀티플레이로 확장하려면 캡처 상태/대상/소유권을 서버 권한으로 전환하고, ViewTarget/HUD는 owning client에서만 처리해야 한다. 현재 구조를 그대로 복제하면 다른 플레이어의 Pawn이나 index 0 Controller를 바꿀 수 있다.
