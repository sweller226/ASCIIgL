#pragma once

namespace events {

/// Emitted when the player presses the key bound to "quit" (e.g. Escape).
/// Only emitted when not blocking (so Escape closes GUI first when a panel is open).
struct QuitRequestedEvent {};

/// Emitted when the player presses the key bound to "interact_left" / primary action (e.g. F).
/// Only emitted when not blocking. MiningSystem consumes for break block.
struct PrimaryActionPressedEvent {};

/// Emitted when the player presses the key bound to "interact_right" / secondary action (e.g. right click / R).
/// Only emitted when not blocking. PlacingSystem consumes for place block.
struct SecondaryActionPressedEvent {};

/// Emitted when the player presses the key bound to toggling game mode (currently hard-wired to P).
/// MovementSystem consumes this to toggle between Survival and Spectator.
struct SwitchGameModeEvent {};

} // namespace events
