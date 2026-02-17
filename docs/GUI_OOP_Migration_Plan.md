# GUI Migration Plan: ECS → Traditional OOP with ECS Integration

## Scope (what to build now vs later)

- **Implement now**: Only **hotbar** and **inventory** GUI. The OOP foundation (Widget, Panel, Slot, Screen, GUIManager, draw list, input blocking) is built so these two work.
- **Not implemented now** (architecture must allow adding later): main menu, settings, pause menu, health/hunger/armor bars, crafting GUI, chest/container GUI. The screen stack, blocking vs non-blocking, and widget types (Button, Label, ProgressBar, Slider, etc.) are designed so you can add these without reworking the core.

---

## 1. Current State Summary

### 1.1 What Exists (ECS-based GUI)

| Piece | Location | Role |
|-------|----------|------|
| **GUISystem** | `ecs/systems/GUISystem` | Layout (anchor/pivot/offset → screen pos), cursor (arrow keys + interact), panel open/close/toggle, hover detection, emits events |
| **Components** | `ecs/components/gui/` | `GUIElement`, `GUIPanel`, `GUISlot`, `GUIButton`, `GUICursor` — all backed by entities with `Transform` + `Renderable` |
| **Factories** | `ecs/factories/gui/` | `GUIFactory` (base), `InventoryGUIFactory` (9×4 panel), `HotbarGUIFactory` (9 slots) — create entities for panels/slots/buttons |
| **Rendering** | `RenderSystem` | Collects all `Transform`+`Renderable`; 2D items use `GUICamera` (optional). No dedicated GUI layer; GUI is “just more entities” |
| **Events** | `events/GUIEvents.hpp` | `SlotClickedEvent`, `ButtonClickedEvent`, `PanelOpenedEvent`, etc. (emit-only; no subscribers in codebase yet) |

### 1.2 Gaps for Your Goals

- **No main menu** — `GameState` is only `Playing` / `Exiting`; no menu or pause flow.
- **No settings screen** — no Minecraft-style options (graphics, sound, controls, etc.).
- **No crafting GUI** — only inventory panel is created; no crafting grid or result slot.
- **Hotbar not wired** — `HotbarGUIFactory` exists but is never instantiated in `Game::InitializeSystems()`.
- **GUI quad mesh** — `GUIFactory::SetSharedQuadMesh()` is never called, so panel/slot/button meshes may be null.
- **2D camera** — `SetActive2DCamera()` is never called, so 2D MVP may be identity (works by accident for pixel-space if view/proj are identity).

### 1.3 What to Preserve

- **ECS as source of truth for game data**: player/chest `Inventory`, `ItemIndex`, block/entity state.
- **EventBus** for GUI → game communication (e.g. slot click → move item in ECS).
- **Existing rendering pipeline**: 2D draw list, `GUICamera`, `guiMaterial`, inventory texture array, `GUIShaders`.
- **Screen/layout concepts**: anchor, pivot, offset, layers — reuse in OOP layout model.

---

## 2. Target Architecture: OOP GUI with ECS Read/Write

### 2.1 High-Level Structure

- **GUI layer is OOP**: widgets (panels, buttons, slots, labels) are C++ objects, not entities. They own or reference meshes/materials and submit draw commands (or build a draw list).
- **ECS is used when content comes from game state**: inventory slots, crafting grid, chest contents. The OOP GUI **reads** from `entt::registry` (e.g. `Inventory`, `ItemIndex`) and **writes** back via systems or events (e.g. slot click → emit event → inventory system moves items).
- **One central GUI manager** (e.g. `GUIManager` or `ScreenManager`) owns:
  - Current **screen** or screen stack (for now: play HUD with hotbar; inventory screen can be pushed when E is pressed). Later: main menu, settings, pause, crafting, chest.
  - Cursor state and input routing.
  - Push/pop or replace semantics so that e.g. Esc from inventory closes inventory; later, main menu can have its own flow.

### 2.2 Screen / Context Types

**Implement now:**

| Screen | OOP role | ECS usage |
|--------|----------|-----------|
| **Play HUD** | Hotbar only (9 slots). Non-blocking. | **Read**: player `Inventory` (hotbar slots 0–8), `ItemIndex` for icons/counts. |
| **Inventory screen** | Panel with 9×4 slot grid. Blocking when open (E to toggle). | **Read**: player `Inventory` + `ItemIndex`. **Write**: emit `SlotClickedEvent` → inventory system handles move/swap. |

**Add later (architecture already supports):**

| Screen / Context | OOP role | ECS usage |
|------------------|----------|-----------|
| **Main menu** | Buttons (Singleplayer, Options, Quit), logo. | None |
| **Settings (Options)** | Sliders, toggles, key bindings. | Optional: config / file |
| **Pause menu** | Resume, Options, Save and Quit. | None |
| **Play HUD (extended)** | Crosshair, health bar, hunger bar, armor, experience. | **Read**: e.g. `Health`, `Hunger` components |
| **Crafting GUI** | 2×2 or 3×3 grid + result slot. | **Read**/ **Write**: crafting slots + events |
| **Chest / container** | Two panels (player + container). | **Read**/ **Write**: both inventories + events |

### 2.3 HUDs: Health, Hunger (future)

When you add them later, the same OOP layer supports health/hunger/armor/exp:

- Same widget system and layout; add e.g. `ProgressBar` or `IconStrip` to the Play HUD.
- Non-blocking (HUD never steals movement/action input).
- Read from registry each frame (e.g. `Health`, `Hunger` on player). No implementation needed now.

### 2.4 Widget Hierarchy (OOP)

**Implement now (minimal set for hotbar + inventory):**

- **Widget** (base): layout (anchor, pivot, offset, size, layer), visibility, enabled, hit-test, `Layout(screenSize)`, `Update(dt)`, `Draw(...)`.
- **Panel** : Widget — background quad, child widgets. Used for inventory panel (and hotbar background if desired).
- **Slot** : Widget — **ECS binding** `(entity, slotIndex)`. In draw, read `Inventory` + `ItemIndex`; on click, emit `SlotClickedEvent`. Used for hotbar and inventory grid.

**Add later (same hierarchy, no rework):**

- **Button**, **Label** — for main menu, pause, settings.
- **ProgressBar** — for health, hunger, experience.
- **Slider / Toggle** — for settings.

Containers (e.g. `Screen`) hold a tree of widgets; the manager sets the active screen and forwards input/layout/draw.

### 2.5 Input and Cursor

- **Single logical cursor**: position (mouse or simulated with arrow keys), hovered widget, click (left/right).
- **Input blocking is input-only, not game pause**:  
  - When a **blocking** screen is open (inventory, crafting, chest, settings, pause): cursor visible, **non-GUI input** is blocked (movement, look, mine, place, etc.). **Game systems keep running** (physics, world update, block updates, etc.) — only the systems that consume player input do not receive it.  
  - So: “blocking” = “this screen consumes input; do not pass movement/action input to game logic.”  
- **Non-blocking GUI**: Some GUI never blocks game input. Examples: **hotbar**, **health bar**, **hunger bar**, **crosshair**, **experience bar**. They are drawn and may react to keys (e.g. scroll hotbar slot) but movement and actions still go to the game. The manager only blocks when the *current* screen (or top of the screen stack) is marked blocking.
- You can keep a “virtual cursor” entity for **rendering** the cursor sprite when a blocking screen is open; when only HUD is visible, cursor can be hidden and hotbar/keyboard still handled by GUI (e.g. slot selection, E to open inventory).

### 2.6 ECS Integration Points (inventory, crafting, containers)

- **Read**  
  - `registry.get<Inventory>(player)` for slot contents.  
  - `ItemIndex` (or item prototype) for icon mesh/layer and stack size.  
  - Optionally a “container” entity for chests (e.g. block entity with `Inventory`).  
  - For HUDs: any player component that drives bars (e.g. `Health`, `Hunger` / `FoodLevel`, `Experience`) — read each frame in the widget’s `Update`/`Draw`.
- **Write**  
  - **Preferred**: emit events (`SlotClickedEvent`, `CraftResultTakenEvent`, etc.); let an **InventorySystem** or **CraftingSystem** (or existing logic in a system) mutate the registry.  
  - **Alternative**: GUI holds a reference to `entt::registry&` and a small “GUI action” API (e.g. `TryMoveSlot(owner, fromSlot, toSlot)`) that is implemented by a system or a dedicated service to keep ECS rules in one place.

### 2.7 GUI Rendering Strategy

**How the OOP GUI gets on screen**

Two approaches:

1. **GUI fills a draw list; one 2D render path consumes it (recommended)**  
   - Each frame the widget tree (Panel, Slot, etc.) produces a list of **draw items**: position, size/scale, mesh (e.g. shared quad or item icon), layer, optional texture/UV.  
   - A **single 2D render path** (inside or next to your existing `RenderSystem`) takes that list, optionally **merges** it with any 2D entities from the registry (e.g. a cursor entity if you keep it), sorts by layer, batches by mesh, then draws with the same 2D camera and material you use today.  
   - So: OOP GUI is the **source of truth** for what’s drawn; no ECS entities for panels/slots. “The rest of the GUI” (everything that isn’t an entity) is rendered by this list.  
   - **Optional**: If you still want the cursor (or any other 2D thing) to live as an entity, the 2D path can treat “registry ELEM_2D” and “GUI draw list” as two inputs, merge into one list, then one sort/batch/draw. That way one pipeline handles all 2D and you can keep or drop ECS 2D later.

2. **OOP GUI syncs into ECS entities; existing 2D path draws them**  
   - Each widget creates/updates an entity with `Transform` + `Renderable` (and mesh) each frame. Your current `RenderSystem` already iterates all such entities and draws 2D with `ELEM_2D`.  
   - **Pros**: No new render code; everything flows through the same system.  
   - **Cons**: Two sources of truth (OOP layout vs ECS transform/mesh); you must keep them in sync every frame. For ~50 widgets (hotbar + inventory + panels) the cost is small, but the design is messier and any bug shows up as “GUI and draw state disagree.”

**Recommendation: draw list, not entities, for GUI**

- Use a **unified 2D draw list** that the OOP GUI fills. The existing 2D pipeline (in `RenderSystem` or a small `GuiRenderer`) is extended to accept this list: either it’s the only input for 2D, or it’s merged with `registry.view<Transform, Renderable>` for `ELEM_2D` before sort/batch/draw.  
- **Rendering the rest of the GUI** = everything that isn’t an ECS entity: panels, slots, slot backgrounds, item icons, cursor (if you draw it from OOP). All of that is “widget calls `DrawList::AddQuad(...)` (or similar)”; the list is submitted once per frame and the 2D pass renders it.  
- **Is “entities flowing through ECS for 2D” best?** For **game** 2D (e.g. sprites, effects) it can be. For **GUI**, keeping draw data in a list owned by the GUI layer is simpler: one source of truth, no sync, same batching benefits (sort by layer, batch by mesh). So: use the ECS 2D path for game-world 2D if you have it; use the **draw list** for GUI. If you merge both into one 2D pass, you still get one sort and one batch for the whole frame.

**Concrete steps**

- **Draw list**: e.g. `struct GuiDrawItem { glm::vec2 position; glm::vec2 size; int layer; std::shared_ptr<Mesh> mesh; /* optional: texture layer / UV */ };`. `GUIManager::Draw(drawList)` (or each `Widget::Draw`) only **appends** to the list — no RendererGPU or draw calls in the GUI layer.  
- **Consumer**: **RenderSystem** performs the actual draw calls. Before or inside `RenderSystem::Render()`, call `guiManager.Draw(drawList)` to fill the list; then RenderSystem merges that list with ECS `ELEM_2D` if desired, sorts, batches, and does all **BindMaterial** / **SetMatrix4** / **DrawMesh** (or equivalent). So: GUI produces data; all GPU draw calls happen in RenderSystem.  
- **Meshes**: Shared quad for panels/slots; item icons from `ItemIndex` (or existing quad-per-layer) for slot contents. No need for GUI to create entities.

**Drawing item stacks in slots (icons + count)**

The **data** for a stack (itemId, count) already lives in ECS (`Inventory::slots[i]`, `ItemIndex` for appearance). The question is how to **draw** that stack in the GUI.

- **Recommended: draw list, not entities.**  
  The Slot widget already reads from the registry to know what to show. In `Slot::Draw()` it: (1) reads `registry.get<Inventory>(owner).slots[slotIndex]` and `ItemIndex` for the icon mesh/texture layer; (2) pushes draw items to the **same GUI draw list** — e.g. slot background quad, then icon quad (and later a count label if you have text). The existing 2D pass then draws everything (panels, slot backgrounds, **stack icons**) in one go. No ECS entities are created for stack visuals; no need for stack drawing to flow through the ECS render system.  
  So: stack data is ECS; stack **rendering** is “Slot reads ECS → pushes draw items” → same 2D pipeline as the rest of the GUI.

- **Alternative: stack visuals as entities.**  
  You could create one entity per visible slot content (Transform + Renderable with item icon mesh), update their positions and meshes each frame from the Slot/layout, and let them flow through the existing `RenderSystem` as `ELEM_2D`. That reuses the ECS 2D path for those quads but adds sync logic (create/destroy/update entities when inventory or layout changes) and mixes “GUI owns layout” with “ECS owns draw.” The plan recommends **not** doing this: keep stack drawing in the draw list for a single source of truth and one 2D pipeline.

**Summary:** Item stacks are already ECS **data**; drawing them in the GUI should **not** flow through the ECS render system as entities. The Slot reads ECS and submits icon (and count) as draw list items; the same 2D pass renders the rest of the GUI and the stack visuals together.

**Should you remove 2D support from RenderSystem?**

No — **keep it and refactor the input.** Right now the 2D path is: collect `ELEM_2D` entities from the registry into a list → sort by layer → batch by mesh → draw with 2D camera. After the GUI migration, nothing (or almost nothing) will be `ELEM_2D` in the registry, so that collection step would produce an empty list. The right move is to **feed the same 2D list from the GUI** instead of (or in addition to) the registry:

- **Option A (recommended):** RenderSystem stays the single place that does all 2D. Add a way to submit the GUI draw list into the 2D pipeline (e.g. `Submit2DDrawList(std::vector<GuiDrawItem>)` or pass the GUI list into `Render()`). Each frame: clear `m_drawList2D`, fill it from the GUI list (and optionally merge in any `ELEM_2D` entities from the registry, e.g. for a cursor entity or future world 2D). Then same sort → batch → draw with `m_active2DCamera`. So 2D support is not removed; its **source** changes from “registry only” to “GUI draw list (+ optional registry).”
- **Option B:** Remove 2D from RenderSystem and add a separate “GUI render pass” that only draws the GUI list (sort, batch, draw with 2D camera). Then you have two places that know how to do 2D (one for GUI, none for ECS 2D). That duplicates the sort/batch/draw logic unless you extract it into a shared helper.

So: **don’t get rid of 2D support.** Refactor so the 2D draw list is filled from the OOP GUI (and optionally from the registry). The only user of 2D is the GUI for now; the RenderSystem’s 2D path is the single pipeline that draws it. Later, if you add world 2D (sprites, cursor entity, etc.), you can merge those into the same list.

---

## 3. Implementation Plan (Phased)

### Phase 1: Core OOP foundation + draw path

1. **Create GUI module** (e.g. `ASCIICraft/gui/`).
2. **Widget base** — layout (anchor, pivot, offset, size, layer), `Layout(screenSize)`, `ContainsPoint(glm::vec2)`, visibility, enabled, `Update`, `Draw`.
3. **Panel** — background quad, child widgets (enough for inventory panel and hotbar strip).
4. **Screen** — root (e.g. full-screen panel), list of children, `blocksInput` flag. `OnUpdate`, `OnLayout`, `OnDraw`, `OnKey`, `OnCursorMove`, `OnClick`.
5. **GUIManager** — current screen or stack (for now a single “play” screen that can show inventory on top). Cursor state, input routing. Each frame: `Layout` → `Update` → hit-test → `Draw`. Esc closes top blocking screen (e.g. inventory).
6. **Rendering** — GUI builds a draw list (see §2.7): each widget appends draw items (position, size, layer, mesh); one 2D pass consumes that list (and optionally merges with ECS `ELEM_2D`), then sort/batch/draw with existing `GUICamera` and `guiMaterial`. No ECS entities for GUI panels/slots.
7. **Wire into Game** — Create `GUIManager` in `Game::Initialize`, set screen size. In `Update`, call `guiManager.Update()`; when the top screen is **blocking**, withhold non-GUI input from game (game systems still run). In `Render`, run GUI draw with 2D camera set.

Deliverable: GUIManager + Widget/Panel/Screen + draw list integrated with Game; no screens with real content yet.

### Phase 2: Hotbar and Inventory (only implemented GUI)

1. **Slot widget** — ECS binding `(entt::entity owner, int slotIndex)`. In `Draw`, read `registry.get<Inventory>(owner).slots[slotIndex]` and `ItemIndex` for icon; draw slot background + icon + count. On click, emit `SlotClickedEvent`. No Button/Label needed yet.
2. **Play HUD screen (non-blocking)**  
   - Contains hotbar: 9 slots bound to player, layout at bottom center.  
   - Optional: hotbar selection index (scroll wheel or number keys) for highlight; can live in screen state or a small ECS component.
3. **Inventory screen (blocking)**  
   - Panel with 9×4 grid of same Slot widget, bound to player `Inventory`.  
   - Open/close: key (E) pushes/pops this screen; when open, `blocksInput == true` so movement/mine/place don’t run; game systems (physics, world, etc.) still run.  
   - Cursor visible when inventory is open; arrow keys + interact for slot click (or mouse when you add it).
4. **Replace ECS GUI for hotbar + inventory**  
   - Stop creating entities with `InventoryGUIFactory` / `HotbarGUIFactory`.  
   - Remove or bypass GUISystem’s layout/slots for these (keep only what you need for cursor when a blocking screen is open, if still using ECS cursor).  
   - Slot content logic lives in OOP Slot’s draw/update (read from registry; emit events on click).  
   - Add or use an **InventorySystem** (or handler in Game) that reacts to `SlotClickedEvent` and performs pick/place/swap.

Deliverable: **Hotbar** (always visible, non-blocking) and **Inventory** (E to open/close, blocking input only), both ECS-backed; architecture ready for main menu, settings, health/hunger, crafting, chest later.

### Future phases (when you need them)

- **Main menu / Settings / Pause** — Add `Button`, `Label`, `Slider`, `Toggle`; add `MainMenuScreen`, `SettingsScreen`, `PauseScreen`; start game by pushing Play HUD from main menu. No change to core.
- **Health / Hunger / etc.** — Add `ProgressBar` (or similar), add to Play HUD, read `Health`/`Hunger` from registry in widget. Non-blocking.
- **Crafting / Chest** — New screens reusing `Panel` + `Slot`; new events or same `SlotClickedEvent` with context (e.g. container entity). Same ECS read/emit pattern.
- **Cleanup** — Remove old ECS GUI entities/factories and GUISystem usage for panels/slots; keep EventBus and `GUIEvents.hpp`. Ensure 2D camera and shared quad are set once.

---

## 4. File / Module Layout Suggestion

**Implement now:**

```
ASCIICraft/
  gui/
    GuiManager.hpp / .cpp        # Screen stack, cursor, input routing
    Screen.hpp / .cpp            # Base screen (blocksInput, lifecycle)
    Widget.hpp / .cpp            # Layout, hit-test, Update, Draw
    Panel.hpp / .cpp
    Slot.hpp / .cpp              # ECS-bound slot (entity + slotIndex)
    DrawList.hpp / .cpp          # Collect draw calls for 2D pass
  gui/screens/
    PlayHUDScreen.hpp / .cpp     # Hotbar only
    InventoryScreen.hpp / .cpp   # 9×4 panel, E to toggle
```

**Add later (same gui/ tree):** `Button.hpp`, `Label.hpp`, `ProgressBar.hpp`, `Slider.hpp`, `MainMenuScreen`, `SettingsScreen`, `PauseScreen`, `CraftingScreen`, `ContainerScreen`. No change to GuiManager/Screen/Widget contract.

Keep ECS in `ecs/`. The `gui/` layer takes `entt::registry&` and `EventBus&` where it reads inventory and emits slot events.

---

## 5. Refinements for Minecraft

These keep the current scope (hotbar + inventory) but align the architecture with Minecraft so you don’t have to rework it later.

**Carried stack (cursor item)**  
When the player “picks up” from a slot, Minecraft shows that stack on the cursor until it’s placed or split. Model this in **ECS**, not only in the GUI: e.g. a `CarriedStack` (or “cursor slot”) on the player (or a small component the inventory system owns). The inventory system moves stack data into/out of this on click; the GUI **reads** it and, when non-empty, **draws the carried stack at the cursor position** (same draw list, one more quad per frame). That way one source of truth and no desync between “what the game thinks is carried” and “what the GUI shows.”

**Left vs right click semantics**  
Minecraft: left = take full stack / place one (depending on context), right = take half / place one. Keep **SlotClickedEvent** carrying a button (0 = left, 1 = right) and have the **inventory system** implement these rules (not the GUI). The plan already does this; just ensure the handler is designed for take-all / take-half / place-one from the start.

**Hotbar selection in ECS**  
The selected hotbar slot (highlight and “item in hand”) should live in **ECS** (e.g. `PlayerInput::selectedHotbarSlot` or a dedicated component) so both the GUI (draw highlight) and game systems (mining, placing) read the same value. GUI updates it on scroll wheel and number keys 1–9; no need for a separate “screen state” that game logic can’t see.

**GuiDrawItem: atlas / UV from the start**  
Minecraft GUIs use a texture atlas (slot background, panel, item icons). Define **GuiDrawItem** with texture layer or UV (or both) so you can draw from one atlas without adding it later. That avoids a second “GUI texture” path and matches how your inventory texture array likely works.

**Modifiers in slot events**  
Add a **shift** flag (and optionally others) to **SlotClickedEvent** (e.g. `bool shift`) so the inventory system can implement shift-click (move stack to/from hotbar or to other panel). You can add it when you implement the handler; the type should allow it so you don’t break the API later.

**Screen resize**  
When the window or logical size changes, call **GUIManager::SetScreenSize()** (or equivalent) and ensure the current screen is re-laid out so anchors/offsets stay correct. One place in the app (e.g. where you handle resize) does this; no extra design needed beyond “layout uses screen size.”

**Future without rework (short notes)**  
- **Tooltips**: Slot (or Widget) can expose “tooltip text” (e.g. item name from ItemIndex); GUIManager or a small tooltip widget draws it near the cursor when hovered. No change to draw list or events.  
- **Slot filters**: Later, slots can have a “role” or “allowed items” so the inventory system can reject moves (e.g. armor slots). Same event; handler logic changes.  
- **Composite screens**: Crafting/chest are one screen that contains multiple panels (crafting grid + inventory, or container + inventory). Screens can own multiple root panels; no need for “one screen = one panel.”  
- **Double-click**: Gather same item from inventory (Minecraft behavior) can be added later as a second event or a flag; inventory system implements it.

---

## 6. Input Centralization (Do Before GUI Migration)

Right now **MovementSystem**, **CameraSystem**, **MiningSystem**, **PlacingSystem**, **GUISystem**, and **Game** all poll `InputManager::GetInst()` directly. That makes "block input when GUI is open" a matter of **skipping** those systems (your current approach). A cleaner approach is to **centralize input** so one layer can gate game input when a blocking screen is open, without touching every system.

**Recommendation: do input centralization before migrating the GUI.**

**Why centralize**

- **Single gate for blocking:** When inventory (or any blocking screen) is open, you want movement, look, mine, place, jump, etc. to be "off" for the game. If all game code reads from an **InputSystem** (or input facade) that knows "is GUI blocking?", you can apply that in one place: e.g. `InputSystem::IsActionHeld("move_forward")` returns `false` when blocking. Then you can **run** MovementSystem every frame (no skip) and it just gets neutral input when the GUI is open. Same for camera, mining, placing. No need for each system to know about GUI.
- **Discrete vs continuous:**  
  - **Discrete** (key/action pressed once): expose as **events** (e.g. `ActionPressedEvent{"open_inventory"}`, `ActionPressedEvent{"quit"}`). GUI and game can subscribe; one place emits when InputManager reports a press.  
  - **Continuous** (held, axes): expose as **direct API** (e.g. `InputSystem::IsActionHeld("move_forward")`, `GetMoveAxis()`, `GetLookDelta()`). Systems read these each frame; InputSystem applies blocking before returning.
- **One binding surface:** Key/controller bindings live in one place (InputManager is still fine); InputSystem wraps it and adds blocking + optional events. Hotbar selection (e.g. scroll, keys 1–9) can be written into ECS by InputSystem when not blocking.

**Concrete shape**

- **InputSystem** (or equivalent) runs once per frame, early in Update:
  1. Call `InputManager::Update()`.
  2. Set **blocking** from GUIManager (e.g. `SetBlocking(guiManager.IsBlockingInput())` or pass a query lambda). So InputSystem doesn't own GUI; Game passes blocking in.
  3. For discrete actions: if pressed this frame, emit `ActionPressedEvent{actionId}` (or similar) on EventBus.
  4. For continuous: cache or forward from InputManager; when systems call `IsActionHeld("move_forward")` etc., return false if blocking (for game actions), else InputManager's value.
- **MovementSystem**, **CameraSystem**, **MiningSystem**, **PlacingSystem** stop using `InputManager::GetInst()` and instead use **InputSystem** (or a getter that goes through the same layer). So they don't reference GUI at all.
- **GUI** continues to read input for its own use (cursor, slot click). When blocking, GUI has focus: it can read raw InputManager or the same InputSystem with "ignore blocking" for GUI-only actions (E to close, arrows, click). So either (a) GUI reads InputManager directly for its keys, or (b) InputSystem exposes "GUI context" where blocking doesn't suppress. (a) is simpler: when blocking, only GUI reads; game reads from InputSystem and gets gated input.)

**Order: input first, then GUI**

1. **Before GUI migration:** Introduce InputSystem as the single reader of InputManager. Move all game-side polling (movement, camera, mining, place, jump, quit) to InputSystem: continuous via `IsActionHeld` / axes, discrete via events. InputSystem does **not** apply blocking yet (no GUIManager). Run all systems every frame; they get input from InputSystem.  
2. **When you add OOP GUI:** Game calls `InputSystem.SetBlocking(guiManager.IsBlockingInput())` (or equivalent) each frame before systems run. InputSystem gates game actions when blocking. Optionally "open inventory" becomes an event from InputSystem that GUIManager subscribes to (so E key is handled in one place).  
3. Result: No need to skip Movement/Camera/Mining/Placing when blocking; they just get neutral input. GUI migration doesn't require each system to check `IsBlockingInput()`.

---

## 7. Risk and Ordering Notes

- **Rendering first**: Decide early whether GUI draws via its own draw list (recommended) or by spawning ECS entities. If you keep ECS entities for GUI, you’ll have two parallel systems (old ECS GUI + new OOP) during migration, which is more confusing.
- **EventBus**: Keep event types and emit from OOP GUI; add one place (e.g. `Game::Update` or an `InventorySystem`) that subscribes and performs slot moves, craft result take, etc.
- **GameState**: For now you can keep `Playing` / `Exiting`. When you add main menu or pause later, extend to e.g. `MainMenu`, `Playing`, `Paused` so Update/Render branch on active screen.
- **Back-compat**: You can keep the old GUISystem and factories until Phase 2 is done and inventory/hotbar are fully on the new GUI; then remove or stub the old path.
- **Input before GUI**: Centralize input (InputSystem with blocking and events) before starting the OOP GUI migration so blocking is a single gate and systems don't need to know about GUI.

This plan delivers an OOP GUI with **hotbar and inventory only** for now; the same architecture allows adding main menu, settings, health/hunger, crafting, and chest later without reworking the core.
