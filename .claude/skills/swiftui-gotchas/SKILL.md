---
name: swiftui-gotchas
description: Reference catalog of SwiftUI, Swift Concurrency (Swift 6 / MainActor-default), SwiftData, WidgetKit, and App Intents antipatterns. Consult BEFORE writing non-trivial SwiftUI code, when a build hits a concurrency or isolation diagnostic, when an animation or transition isn't behaving, when a widget reads stale data, or when reaching for `TimelineView` / `withAnimation` / `matchedGeometryEffect` / custom gesture composition. Use proactively the moment a task touches one of: SwiftUI animation, SF Symbols, navigation, scrolling, SwiftData fetching, App Intents, widget timelines, `@MainActor` boundaries, `Sendable` errors, or `Task { }` lifecycle.
---

# SwiftUI / iOS gotchas

Rules distilled from real mistakes across iOS projects using Swift 6 concurrency. Each is general enough to apply to any project on a modern iOS toolchain.

## When to consult

- Starting a non-trivial SwiftUI / SwiftData / WidgetKit / App Intent change → skim the relevant category before writing code.
- Build emits a Swift 6 isolation, Sendable, or `@MainActor` diagnostic → check **Concurrency**.
- Animation flashes, neighbor cells re-render, or a transition is missing → check **Animation & transactions**.
- About to write `TimelineView`, `Timer`, or a manual countdown → check **SF Symbols & Text**.
- Widget reads stale or default data; an AppEntity won't select → check **SwiftData & Widgets**.

## Principles

1. **Write the smallest code that achieves the outcome.** Prefer one framework call over a bespoke wrapper around it. Prefer absence of code over presence. Don't add scaffolding "in case we need it later" — the framework or the codebase will tell you when the need arrives. This principle is the parent of most of the rules below: framework-first, `.default` curves, threading `ModelContext` instead of capturing it. And when debugging: only **remove**, never add (principle #6).
2. **Reach for the SwiftUI / Foundation API first.** Countdowns (`Text(date, style:)`), content morphs (`.contentTransition(.numericText)`), haptics (`.sensoryFeedback`), symbol variants (`.symbolVariant(.fill)`), periodic effects (`.phaseAnimator`) all have native one-liners. If you wouldn't be surprised the framework already has it, look it up before writing — the `apple-docs` MCP exposes `search_apple_docs`, `search_framework_symbols`, and `search_wwdc_content` for exactly this. WWDC 22–25 covers most of the modern SwiftUI / Concurrency surface.
3. **Animation scope matters.** `withAnimation` opens a *global* transaction that propagates through every visible view that re-renders. `.animation(_:value:)` scopes to a single view's value change. `TimelineView(.animation)` re-emits the body at the display refresh rate and breaks transitions inside its closure. Pick the right one.
4. **Under `SWIFT_DEFAULT_ACTOR_ISOLATION = MainActor`, every unannotated type is implicitly `@MainActor`.** Service classes doing I/O MUST be explicitly `Sendable` with `nonisolated let` properties and `nonisolated func ... async` methods, or they will block the main thread. Verify the build setting before reasoning about isolation.
5. **Pass `ModelContext` as a parameter; don't capture it.** The context from `@Environment(\.modelContext)` is main-actor-bound. Storing it in a singleton pins the whole class to MainActor. Thread `using modelContext: ModelContext` per call.
6. **"Done" means user-visible behavior, not build success.** Invoke `/verify-ios` on UI work before declaring done. When stuck for 2 attempts, switch from reasoning to bisection — remove the smallest plausible cause, build, observe. Only remove, never add.

---

## Animation & transactions

### A1. `withAnimation` broadcasts across siblings — scope with `.animation(_:value:)`
**Wrong:** wrap a state mutation in `withAnimation { state.toggle() }` to drive a single child's animation when the parent contains many similar children (lazy stacks, `ForEach` lists, grids).
**Right:** the transaction propagates through every sibling that re-renders, pulling neighbors into the animation alongside the one you meant to change. Drop the global `withAnimation` and put `.animation(.default, value: state)` on the affected child so only that view animates on its own value change.

### A2. Conditional `AnyShapeStyle` gets interpolated by spring animations
**Wrong:** `.foregroundStyle(isDone ? AnyShapeStyle(.tint) : AnyShapeStyle(.tint.tertiary))` inside an animated view.
**Right:** a type-erased ternary creates a new instance per render; SwiftUI can't equate them, and a bouncy spring will interpolate between two styles — overshoot flashes through unexpected colors. Keep one constant `.foregroundStyle(.tint)` and animate `.opacity` (a single `Double`, clean interpolation).

### A3. `matchedGeometryEffect` silently no-ops across UIKit-hosted regions and async state publishers
**Wrong:** use `matchedGeometryEffect` to fly a view into a `ToolbarItem`, gated on an `@AppStorage` flag toggled inside `withAnimation`.
**Right:** `ToolbarItem` is hosted by `UINavigationBar` — `@Namespace` doesn't propagate across that boundary. `@AppStorage` publishes asynchronously via Combine, so the spring transaction is over before the view re-evaluates. Build a manual flight overlay: capture source/dest frames with `PreferenceKey`, render copies in a top-level `ZStack`, drive with a single `progress: @State`, hide endpoints during flight.
**Console signal:** *"Multiple inserted views in matched geometry group ... have isSource: true, results are undefined"*.

### A4. `TimelineView(.animation)` re-emits at the display refresh rate and breaks `.transition` inside its body
**Wrong:** wrap a whole view in `TimelineView(.animation)` to drive a per-frame effect, leaving `.id()` / `.transition` children inside.
**Right:** SwiftUI's transition state tracking is unreliable when the surrounding tree re-emits 60–120 times per second. Invert the relationship — push `TimelineView` *inside* each leaf that needs a ticker, so the parent tree stays stable and transitions get clean start/end states.

### A5. `.phaseAnimator` over `TimelineView` + math for periodic effects
**Wrong:** drive a mesh gradient or oscillating offset with `TimelineView(.animation)` and trig math.
**Right:** `.phaseAnimator` (iOS 17.2+) expresses periodic state phases declaratively, no manual `Date` math, no per-frame body re-emit. Reserve `TimelineView` for cases that genuinely need wall-clock time (countdowns, scheduled UI).

### A6. Default to `.default` — don't pick a curve unprompted
**Wrong:** reach for `.bouncy`, `.spring(response: 0.4, dampingFraction: 0.6)`, or `.easeInOut(duration: 0.25)` because it's "more fun."
**Right:** `.default` is what Apple uses across the system; it's the right baseline unless either (a) the surrounding codebase has established a non-default curve for this kind of interaction, or (b) the user has explicitly asked for a specific feel. Custom curves are taste decisions; making one unprompted is noise. (And bouncy curves actively cause bugs around interpolation — see [[A2]].)

---

## SF Symbols & Text

### S1. Don't hand-roll countdowns — use `Text(_:style:)`
**Wrong:** `TimelineView(.periodic(from: .now, by: 60))` + manual `Date` math + a formatter helper to render "Locks in 3h 24m".
**Right:** `Text(date, style: .relative)` (or `.timer` / `.offset`) auto-updates. One line. No timer plumbing.

> *"You're doing too much work for the countdown ;) SwiftUI's Text has built-in countdowns/timers etc with Text(date, style: ...)"*

### S2. `.symbolVariant(.fill)`, not `"name.fill"` string concatenation
**Wrong:** build the name as `isDone ? "app.fill" : "app"` and pass to `Image(systemName:)`.
**Right:** render one symbol; apply `.symbolVariant(.fill)` conditionally. Composes with other modifiers and preserves symbol identity for `.contentTransition(.symbolEffect(.replace))`.

### S3. `.contentTransition(.numericText)` over hand-rolled streaming text
**Wrong:** custom typewriter/streaming text mechanism (with a `.task` mutating state per character) for animated number changes.
**Right:** `.contentTransition(.numericText)` produces the visual effect with zero state machinery. Side benefit: heavy `.task`-driven streaming inside a view body can crash Xcode previews during preview reload.

### S4. `.symbolEffect(.replace.magic)` for paired-symbol morphs
**Wrong:** `.replace.byLayer` to morph between paired symbols (e.g. `app` → `checkmark.app`).
**Right:** `.replace.magic` is what produces the draw-on morph for paired symbols; `.byLayer` just snaps. Pair with a non-bouncy curve so overshoot doesn't cause scale artifacts.

### S5. `.symbolRenderingMode(.hierarchical)` over `.opacity` to convey "disabled"
**Wrong:** dim past/disabled tiles with `.opacity(0.5)`.
**Right:** `.symbolRenderingMode(.hierarchical)` reads as "secondary" without making the tile look greyed out or broken.

---

## State, navigation, gestures

### G1. Don't construct heavy `@State` objects in the View init autoclosure
**Wrong:** `@State private var state = OnboardingFlowState()` where the initializer touches SwiftData / audio / Core Haptics.
**Right:** the autoclosure can fire during preview reload before SwiftUI mounts the view, blowing up framework init. Make the state optional and construct it inside `.task { state = OnboardingFlowState() }` so the heavy work happens once, after mount.

### G2. `@State` survives data swaps when SwiftUI reuses view identity — reset explicitly
**Wrong:** assume `@State scrollPosition = maxScroll` in `init` will re-run when the parent passes new data.
**Right:** `@State` init only fires on first creation. When a view is reused with new data (same identity, different payload), the old state persists while derived layout changes. Use `.onChange(of: <stable-data-signal>) { scrollPosition = maxScroll }`.

### G3. Don't put `@AppStorage` (or any independent subscription) in a child the parent is animating
**Wrong:** move `@AppStorage("show24Hour")` into the leaf view that consumes it, for "encapsulation."
**Right:** `@AppStorage` creates an independent UserDefaults subscription. When updates from that subscription and updates driven by the parent's animation context (e.g. `.animation(.default, value: date)`) converge on the same frame, SwiftUI may merge or discard the parent's animation transaction — the symptom is "the animation stopped working for no clear reason." Subscribe once in the (single) parent and pass the value to the leaf as a plain parameter. Same caution applies to `@Environment(\.scenePhase)`, `@ObservedObject`, and any other independently-publishing source.

### G4. Register `navigationDestination(for:)` once at the `NavigationStack` root
**Wrong:** add `.navigationDestination(for: Foo.self)` inside a detail view that can push another instance of itself (recursive navigation).
**Right:** each push re-registers the destination, producing console spam and unreliable resolution. Destinations live with the stack, not with pushed views — declare every `navigationDestination(for:)` on the parent that *owns* the `NavigationStack`.
**Console signal:** *"A navigationDestination for X was declared earlier on the stack. Only the destination declared closest to the root view of the stack will be used."*

### G5. `Button` on iOS can't host tap AND long-press cleanly
**Wrong:** attach `.onLongPressGesture` to a `Button` to get press-and-hold-to-repeat alongside tap.
**Right:** `Button`'s gesture priority fights long-press on iOS. Build a custom view with `onTapGesture` + `onLongPressGesture`, or use a single `DragGesture(minimumDistance: 0)` with `@GestureState` and own the press/release transitions. Otherwise the two callbacks fire in unpredictable order on release and double-count or miss the tap.

### G6. `DragGesture(minimumDistance: 0)` blocks ancestor `ScrollView` pans
**Wrong:** put a zero-min-distance drag on a tile and rely on `.simultaneousGesture` to let the ScrollView win for vertical pans.
**Right:** zero-min-distance gestures consume touches at the leaf, blocking the ancestor pan. Either let the parent handle the gesture, or use a state machine inside `DragGesture` that waits ~0.3s of stillness before claiming the touch as a drag.

### G7. Bottom bars: `ToolbarItem(.bottomBar)` by default, `.safeAreaBar` as escape hatch
**Wrong:** reach for `.safeAreaBar` (or `.safeAreaInset`) the moment you need a bottom bar — bypassing the system styling without reason.
**Right:** `ToolbarItem(placement: .bottomBar)` is the default. The system handles spacing, blur, and nav-stack integration for free, and the path works on watchOS too. Use `.safeAreaBar(edge: .bottom)` with a regular `View` only when the toolbar's styling or layout constraints actively fight what you're building (custom button shapes, non-standard arrangements, content the toolbar won't accept). When you do need an escape hatch, prefer `.safeAreaBar` over `.safeAreaInset` — the former preserves scroll edge effects (blur transitions at the content edge); the latter doesn't.

---

## SwiftData

### D1. Don't use generic SwiftData keypaths on a protocol — crashes at runtime
**Wrong:** centralize `static func all(in:)` / `find(stableID:)` on a `Tracker` protocol with `SortDescriptor(\.sortOrder)` / `#Predicate { ... }` over `Self`.
**Right:** when the keypath roots in the protocol rather than the concrete `@Model`, SwiftData's internal `graph_keyPathToString` can't resolve it against the schema and trips `EXC_BREAKPOINT` at runtime — no compile error. Keep these helpers concrete per-model, or fetch unfiltered with `FetchDescriptor<Self>()` and sort/filter in Swift (fine for small datasets).

---

## Widgets

Widget extensions are not "small SwiftUI views." They run in a separate process, under a tight memory budget, on a system-controlled refresh schedule, with a curated subset of the SwiftUI animation surface. Rules that hold inside the app often break inside a widget extension.

### W1. Timeline budget is a system hint, not a contract — design for "refresh might not happen"
**Wrong:** rely on `Timeline(entries:, policy: .atEnd)` or `.after(Date)` to refresh exactly on time, and on `WidgetCenter.shared.reloadAllTimelines()` to take effect immediately.
**Right:** the system decides when (or whether) to honor a reload request based on widget visibility, charge state, foreground activity, and the overall daily budget. Design each entry so it stays *plausible* across a range, not a single instant. If you need behavior change at known future moments, generate a series of entries that cover the window — the system swaps between them without re-running your provider.

### W2. `Date.now` baked into an entry is the *generation* time, not display time
**Wrong:** stamp `Date.now` into the entry payload and render it as a static `Text`, expecting it to stay current.
**Right:** the timeline is built once and the widget may render that snapshot hours later. For live time content, use system-rendered `Text` templates — `Text(date, style: .relative)`, `.timer`, `.offset`, `Text(timerInterval:)` — which update *in place* without re-running your provider. For non-time content that should change at a specific future moment, emit one entry per state with the `date:` you want it to appear, and let the system swap between them.

### W3. `TimelineView(.animation)` and `TimelineView(.everyMinute)` don't run inside widgets
**Wrong:** reach for `TimelineView(.animation)` for per-frame effects or `TimelineView(.everyMinute)` for live minute text, expecting WidgetKit to drive them.
**Right:** widget rendering is snapshot-based — `TimelineView` only refreshes when a new entry is delivered, regardless of the schedule type. The supported animation surface is narrow: `.contentTransition(.numericText)` between entries, supported `.transition(...)` modifiers driven by entry diffing, and system-rendered `Text` time templates (W2) which are the *only* mechanism for autonomous sub-entry time updates. Anything richer comes from the *structure* of the timeline (multiple entries) — or accept it won't animate.

> *"The only way to show updating time-based text in widgets is using Text(_, style:)."*

### W4. Widget extensions run under a tight memory budget — fetch small, image small
**Wrong:** reuse the main app's full SwiftData fetch (with relationships eagerly loaded) inside the widget provider; load full-resolution images via `Image(uiImage:)`.
**Right:** widget extensions get tens of MB of memory, not hundreds. Cross the line and the system kills the extension silently — no crash report, just a placeholder tile. Use lightweight `FetchDescriptor`s scoped to what the view actually shows, downsample images via `CGImageSource` + `kCGImageSourceCreateThumbnailFromImageAlways` (or `ImageRenderer` for SwiftUI), and don't retain models beyond the timeline entry that needs them.

### W5. `getSnapshot(in:)` is the gallery preview — return fast, never block on the network
**Wrong:** issue an async network request inside `getSnapshot` and `await` the response.
**Right:** `getSnapshot` runs in the widget gallery and configuration UI; it must return sub-second with plausible-looking placeholder data. Real network or heavy SwiftData work belongs in `getTimeline(in:)` — and even there, run against the budget (W1) and a cache. Treat `getSnapshot` like SwiftUI's preview: synthetic data, immediate return.

### W6. Don't put SwiftData model objects in timeline entries — use `Codable` value types keyed by `UUID`
**Wrong:** put `@Model` instances (or their `persistentModelID`) directly in timeline entries, or use `model.persistentModelID` as the `id` on an `AppEntity`.
**Right:** `PersistentIdentifier` doesn't reliably round-trip across the AppIntent/widget configuration boundary; configured selection silently falls back to a default. The pattern that holds: define a `Codable, Hashable, Identifiable` value-type wrapper (e.g. `EntryLocation`) keyed off `model.uuid: UUID`, and use `UUID` as the `AppEntity` identifier with `entities(for identifiers: [UUID])`. Backfill `stableID: UUID?` on existing models if migrating (optional for additive CloudKit migration).

### W7. Widget targets need an App Group + `ModelConfiguration(groupContainer: .identifier(...))`
**Wrong:** `try ModelContainer(for: Foo.self)` in shared code and expect widgets to read the same store as the app.
**Right:** widget extension and main app see different sandbox containers by default. Declare an App Group entitlement, then:
```swift
ModelConfiguration("User Data", schema: schema,
    isStoredInMemoryOnly: inMemory,
    groupContainer: inMemory ? .none : .identifier(appGroupID),
    cloudKitDatabase: .none)
```
Validation happens at runtime — a build pass doesn't prove the wiring works.

### W8. Interactive widgets render as live SwiftUI views — `Text(_, style: .timer)` instances multiply re-evaluations
**Wrong:** assume "system-rendered" `Text(_, style: .timer)` is free in any widget — drop several copies into a configurable interactive widget body without thought.
**Right:** in a `StaticConfiguration` widget, the timer text is a system overlay (cheap). The moment you add a `Button(intent:)`, the widget renders as a live SwiftUI view; every `Text(_, style: .timer)` then drives ~25Hz body re-evaluation, and each pass re-prepares every `Button(intent:)`. N instances multiply (3 buttons × 3 locations × 2 layers = 12+). Symptoms: log spam of "Prepared widgetID/widgetKind", timeline render requests taking many seconds. Share label instances; never render `style: .timer` inside a mask or hidden layer; use static `Text(date, style: .time)` for non-second-precision needs.

### W9. `.invalidatableContent()` is a loading-state marker, not "self-updating live content"
**Wrong:** apply `.invalidatableContent()` so the view "refreshes itself" between timeline entries.
**Right:** the modifier marks a view to render in the invalidated/redacted state from the moment the user taps an interactive widget control until a new timeline entry arrives. It's a *loading-state* signal, not an animation/update mechanism. To suppress automatic redaction on parts of the widget, use `.unredacted()` on the body.

### W10. Interactive widgets need `AppIntentConfiguration` + `AppIntentTimelineProvider` — not `StaticConfiguration`
**Wrong:** add `Button(intent:)` to a widget declared with `StaticConfiguration` + `TimelineProvider` and assume taps refresh the timeline.
**Right:** `Button(intent:)` reliably integrates only with `AppIntentConfiguration` + `AppIntentTimelineProvider`. Under `StaticConfiguration`, taps fire the intent but `WidgetCenter.shared.reloadTimelines(ofKind:)` doesn't reliably re-drive `getTimeline` on iOS 17/18 — the widget appears stuck in an invalidated state. Migration: define an empty `WidgetConfigurationIntent` (if no real configuration is needed), swap to `AppIntentConfiguration`, make timeline methods async.

### W11. `GeometryReader` / async size measurement is too late for the snapshot pass
**Wrong:** capture a child's size with `GeometryReader` + `.onAppear` / `.task(id:)` and feed it into a sibling layout via `@State`.
**Right:** widget snapshots are a one-shot render; `.onAppear` fires *after* the snapshot is captured, so dependent geometry reads as `.zero` on first paint. For widgets, compute size synchronously from font metrics / known constants and pass it as a parameter — don't rely on async measurement.

### W12. Per-widget-instance state needs an explicit widget ID, not the widget kind
**Wrong:** key per-tile state (e.g., a time-travel offset) on the widget *kind* and use `WidgetCenter.reloadTimelines(ofKind:)` to refresh "the right tile."
**Right:** `reloadTimelines(ofKind:)` reloads *all* tiles of that kind — correct for cross-instance fanout, wrong for per-instance state, which would mean tapping +1h on one tile changes all of them. Resolve a stable per-instance widget ID (an optional `widgetID` parameter on the configuration intent, falling back to a hash of selected entities, then a default) and key persisted state on `"key_\(widgetID)"`. The intent still tells `WidgetCenter` what `ofKind` to reload — separate concerns.

---

## Swift 6 concurrency

These rules assume `SWIFT_DEFAULT_ACTOR_ISOLATION = MainActor` (a common Swift 6 strict-mode setting) and approachable-concurrency enabled. Verify with `xcodebuild -showBuildSettings | grep -E 'SWIFT_(DEFAULT|APPROACHABLE|STRICT)'`. If the project is not on that mode, the rules still apply but the failure modes shift from "compiler errors" to "data races at runtime."

### C1. Service classes need explicit `Sendable` + `nonisolated let` + `nonisolated func ... async`
**Wrong:** `@MainActor final class APIClient` so it "just works" with view models.
**Right:** under the MainActor default, a `@MainActor` service blocks the main thread on every network call. Make it `final class APIClient: Sendable` (or `@unchecked Sendable` if it has mutable state behind a lock). Mark every stored property `nonisolated let`, every method `nonisolated func ... async throws`. Now network requests run off-main without `await MainActor.run { }` ping-pong.

```swift
final class APIClient: @unchecked Sendable {
    nonisolated let baseURL: URL
    nonisolated private let session: URLSession
    nonisolated func fetch() async throws -> Response { /* ... */ }
}
```

### C2. Bare `Task { }` inherits no isolation — annotate it
**Wrong:** spawn `Task { await thing() }` from `init()`, from a `TimelineProvider` callback, or from `task.expirationHandler`, expecting it to inherit the surrounding context's isolation.
**Right:** in non-isolated contexts, `Task { }` runs unspecified-isolation. If the body touches main-actor state (`@Binding` writes, SwiftData fetches via the main context, `@MainActor`-bound stores), write `Task { @MainActor in ... }`. Inside `init()` specifically: `Task { }` also captures `self` strongly and can't be cancelled by the owner — prefer `.task { }` on the first view instead.

### C3. Static properties on `@MainActor` types can't be touched from `actor` / nonisolated context
**Wrong:** reach for `SomeService.shared` (a `@MainActor` static) from inside an `actor`.
**Right:** `await` it, move the static to nonisolated context, or import the offending module with `@preconcurrency`.
**Compiler signal:** *"Main actor-isolated static property X can not be referenced on a nonisolated actor instance."*

### C4. `@preconcurrency import` for SDKs that haven't audited their isolation
**Wrong:** retroactively wrap third-party SDK types to satisfy `Sendable`, generating a cascade of warnings.
**Right:** `@preconcurrency import SomeSDK` at the import site downgrades Sendable/isolation errors from that module to warnings. Use sparingly and only on dependencies you don't own.

### C5. `LayoutValueKey` (and other framework-callback) conformances must be `nonisolated`
**Wrong:** declare a `LayoutValueKey`-conforming struct with no isolation annotation under MainActor default.
**Right:** SwiftUI invokes `LayoutValueKey.defaultValue` from non-isolated layout machinery. Mark the static `defaultValue` `nonisolated` (or annotate the whole struct). Same pattern applies to other "framework calls into your type from no-particular-actor" boundaries — when the diagnostic says "X-isolated conformance cannot be used in nonisolated context," the fix is `nonisolated`, not adding more `@MainActor`.

### C6. Shared mutable state accessed across targets needs explicit isolation — MainActor default isn't enough
**Wrong:** declare `final class Cache` in shared code and trust the project-wide MainActor default for a singleton accessed from both the app and a widget extension.
**Right:** a class accessed across module/target boundaries (especially via `WidgetCenter.shared.reloadAllTimelines()`, which may run off-main) is NOT auto-MainActor. Annotate explicitly: `@MainActor final class Cache`, or rewrite as `actor Cache`. The MainActor default applies within a target's source, not across them.

### C7. Off-main entry points must hop to MainActor before touching MainActor-bound state
Applies to `BGAppRefreshTask` / `BGProcessingTask` handlers, `AppIntent.perform()`, AppEntityQuery methods, Foundation Models tool `call()` methods — any closure the system invokes from a non-isolated context.

**Wrong:** call `mainContext.fetch(...)`, mutate a `@MainActor`-isolated store, or write to a `@Binding` directly inside one of these closures.
**Right:** wrap in `Task { @MainActor in ... }` (and keep the `Task` token so `expirationHandler` can `.cancel()` it). For AppIntents specifically, annotate `perform()` `@MainActor` when it touches SwiftData or the main store.

```swift
static func handleBackgroundRefresh(task: BGAppRefreshTask) {
    let work = Task { @MainActor in
        applyAll()
        task.setTaskCompleted(success: true)
    }
    task.expirationHandler = {
        work.cancel()
        task.setTaskCompleted(success: false)
    }
}
```

### C8. `ModelContext` is MainActor-bound — thread it as a parameter
**Wrong:** store a `ModelContext` in a service singleton; share it freely across async functions.
**Right:** `@Environment(\.modelContext)` is main-actor-bound. Don't capture it in a stored property (that pins the whole class to MainActor). Pass `using modelContext: ModelContext` per call. For cross-context work, use `ModelActor` with its own background context — don't share the main one.

```swift
@MainActor
func fetchItems(using modelContext: ModelContext) -> [Item] {
    (try? modelContext.fetch(FetchDescriptor<Item>())) ?? []
}
```

---

## Modern API swaps

### M1. `.sensoryFeedback(.impact(...), trigger:)` over `UIImpactFeedbackGenerator`
**Wrong:** `UIImpactFeedbackGenerator(style: .medium).impactOccurred()` from inside SwiftUI button actions.
**Right:** `.sensoryFeedback(.impact(.medium), trigger: state)` — declarative, auto-managed lifecycle, no manual generator instances, no `#if os(iOS)` gate needed for watchOS variants.

---

## Reusable templates

```swift
// Stateless service — auto-Sendable, unpinned from MainActor
final class KeychainManager: Sendable {
    nonisolated func read(_ key: String) async throws -> String? { /* ... */ }
}

// Service with immutable config — explicit Sendable, all-let properties
final class APIClient: @unchecked Sendable {
    nonisolated let baseURL: URL
    nonisolated private let session: URLSession
    nonisolated func fetch() async throws -> Response { /* ... */ }
}

// Onboarding async work into a MainActor-bound view model
.onAppear {
    Task { @MainActor in await viewModel.load() }
}

// Per-call ModelContext threading — never captured
@MainActor
func scan(using modelContext: ModelContext) async { /* ... */ }
```

## When this catalog doesn't have your gotcha

1. Search the codebase — there's usually an existing pattern; reuse it.
2. Check Apple docs via the `apple-docs` MCP (`search_apple_docs`, `search_wwdc_content`). WWDC 22–25 covers most modern SwiftUI / Concurrency content.
3. If stuck for 2 attempts: bisect. Remove the smallest plausible cause, build, observe. Only remove — never add — while debugging.
4. After resolving: if the lesson is generalizable, add it here (or a related skill) so the next session benefits.
