# EasyRTCSdk Instrumentation Tests Design

Date: 2026-04-21
Module: `android/easyrtc`
Test type: Android instrumentation tests (`androidTest`)

## Goal

Add a practical first batch of instrumentation tests for `EasyRTCSdk` lifecycle and callback behavior, focused on JNI-backed state management and main-thread callback dispatch safety.

## Why Instrumentation (Not Local Unit Tests)

`EasyRTCSdk` behavior depends on Android runtime and native bindings:

- JNI/native path: `EasyRTCSdk.connection()` calls `peerconnection.create(...)` and `MediaSession.create(...)`, which require Android-packaged native libs and device runtime.
- Looper/Handler path: callback APIs (`onSDPCallback`, `onTransceiverCallback`, `onDataChannelCallback`) enqueue through `ExecutorService` + `Handler(Looper.getMainLooper())`; this must run against a real main looper.
- State is singleton and process-level (`object EasyRTCSdk`), so realistic lifecycle teardown/recreate behavior is best validated in device/emulator process conditions.

Because of these constraints, local JVM unit tests would be either unreliable or heavily mocked, and would not verify the real failure surfaces we care about.

## Scope

This batch validates:

- peer/media session creation on connection
- release semantics and idempotency
- state recreation after reconnect
- null-listener callback safety
- listener dispatch behavior when callback `userPtr` matches SDK `mUserPtr`

This batch does **not** validate signaling protocol correctness, server interactions, or media content/frame correctness.

## Target Files

### Create

- `android/easyrtc/src/androidTest/java/cn/easyrtc/EasyRTCSdkInstrumentationTest.kt`
  - Main instrumentation test class with the 5 first-batch tests.
- `android/easyrtc/src/androidTest/java/cn/easyrtc/TestListener.kt`
  - Minimal thread-safe listener used for callback assertions.
- `android/easyrtc/src/androidTest/java/cn/easyrtc/TestReflection.kt`
  - Small helper for controlled introspection of `EasyRTCSdk` private fields (`mUserPtr`, `mPeerConnection`, `mediaSession`) when required for deterministic assertions.

### Modify

- `android/easyrtc/build.gradle.kts`
  - Ensure instrumentation dependencies are sufficient (`androidx.test:core-ktx`, `androidx.test.ext:junit-ktx`, optional `androidx.test:rules`).
  - Keep `testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"`.

## Test Design

All tests run in one process, serially, with strict cleanup:

- `@Before`: call `EasyRTCSdk.release()` and `EasyRTCSdk.setEventListener(null)` to normalize singleton state.
- `@After`: repeat cleanup to prevent inter-test contamination.
- Use fixed non-network STUN/TURN placeholders (empty strings are acceptable where native API allows).
- Use polling with timeout for async callback assertions (no arbitrary long sleeps).

## First-Batch Test Cases (Exact)

### 1) `connection()` creates peer + media session

Intent: verify lifecycle initialization creates required runtime state.

Steps:

1. Call `EasyRTCSdk.connection(...)` with deterministic `userPtr`.
2. Read `EasyRTCSdk.getPeerConnectionHandle()`.
3. Read `EasyRTCSdk.getMediaSession()`.

Assertions:

- Peer handle is non-zero.
- `getMediaSession()` returns non-null object.

### 2) `release()` resets peer handle and is idempotent

Intent: verify repeated cleanup is safe and leaves a known baseline.

Steps:

1. Connect once.
2. Call `EasyRTCSdk.release()`.
3. Call `EasyRTCSdk.release()` again.

Assertions:

- After first release, `getPeerConnectionHandle() == 0L`.
- Second release does not throw.
- Internal media session reference is cleared (via reflection helper).

### 3) connection-release-connection recreates state

Intent: ensure singleton can recover after teardown and create fresh native session state.

Steps:

1. Connect and capture first peer handle.
2. Release.
3. Connect again and capture second peer handle.

Assertions:

- First handle non-zero.
- Second handle non-zero.
- SDK is operational after second connect (media session present).
- Do not require handle inequality (native allocator may reuse addresses).

### 4) callbacks do not crash without listener

Intent: verify callback entry points are null-safe and scheduler-safe.

Steps:

1. Ensure listener is null.
2. Call:
   - `EasyRTCSdk.connectionStateChange(...)`
   - `EasyRTCSdk.onSDPCallback(...)`
   - `EasyRTCSdk.onTransceiverCallback(...)`
   - `EasyRTCSdk.onDataChannelCallback(...)`

Assertions:

- No exception thrown by any callback call path.
- Methods return `0` as expected.

### 5) callbacks dispatch when listener is set and `userPtr` matches

Intent: verify guarded callback routing reaches listener for matching user context.

Steps:

1. Connect with known `userPtr` (e.g. `1001L`).
2. Attach `TestListener` with latch/counters.
3. Invoke callback methods using matching `userPtr` for guarded callbacks.

Assertions:

- `connectionStateChange` reaches listener once for matching pointer.
- `onSDPCallback` reaches listener once for matching pointer.
- `onTransceiverCallback` reaches listener once for matching pointer.
- Event payloads observed by listener match invocation values.

Notes:

- This batch checks positive dispatch for matching `userPtr`.
- Negative mismatch filtering can be added in a follow-up batch to avoid over-expanding scope.

## Intentionally Not Tested in This Batch

- Network signaling correctness (offer/answer/ICE exchange correctness over server).
- Server dependency and backend availability behavior.
- Media frame correctness (bitstream quality, decode correctness, A/V sync, bitrate/FPS validation).

These require integration/end-to-end scenarios and are intentionally separated from lifecycle/callback safety tests.

## Execution Commands

From `android/`:

```bash
./gradlew :easyrtc:connectedDebugAndroidTest
```

Run only this class:

```bash
./gradlew :easyrtc:connectedDebugAndroidTest \
  -Pandroid.testInstrumentationRunnerArguments.class=cn.easyrtc.EasyRTCSdkInstrumentationTest
```

Optional build-only sanity before device run:

```bash
./gradlew :easyrtc:assembleDebug :easyrtc:assembleDebugAndroidTest
```

## Risks and Mitigations

### Flakiness from async callback dispatch

- Risk: `ExecutorService` + main-thread `Handler` causes timing-sensitive asserts.
- Mitigation: use latch-based waiting with bounded timeout; avoid fixed sleeps; keep tests single-threaded/serial.

### Emulator/device runtime constraints

- Risk: instrumentation requires connected Android runtime and compatible ABI.
- Mitigation: run on arm64 emulator/device (module is configured with `arm64-v8a`); verify one online device before test execution.

### Singleton state leakage across tests

- Risk: `object EasyRTCSdk` keeps process-level state, leading to order-dependent failures.
- Mitigation: strict `@Before/@After` cleanup (`release()`, clear listener) and avoid parallel instrumentation execution.

## Acceptance Criteria

- All 5 first-batch tests implemented in `androidTest` and passing on at least one arm64 emulator/device.
- No backend/server requirement for pass.
- Failures are deterministic and tied to lifecycle/callback contract regressions.
