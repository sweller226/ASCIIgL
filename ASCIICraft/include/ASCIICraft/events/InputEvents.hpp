#pragma once

namespace events {

/// Emitted when the player presses the key bound to "toggle inventory" (e.g. E).
/// Emitted regardless of blocking so the GUI can close when the key is pressed again.
struct ToggleInventoryEvent {};

/// Emitted when the player presses the key bound to "quit" (e.g. Escape).
/// Only emitted when not blocking (so Escape closes GUI first when a panel is open).
struct QuitRequestedEvent {};

/// Emitted when the player presses the key bound to "jump" (e.g. Space).
/// Only emitted when not blocking. MovementSystem consumes this to set jump buffer.
struct JumpPressedEvent {};

/// Emitted when the player presses the key bound to "interact_left" / primary action (e.g. left click / Q).
/// Only emitted when not blocking. MiningSystem consumes for break block.
struct PrimaryActionPressedEvent {};

/// Emitted when the player presses the key bound to "interact_right" / secondary action (e.g. right click / R).
/// Only emitted when not blocking. PlacingSystem consumes for place block.
struct SecondaryActionPressedEvent {};

} // namespace events
