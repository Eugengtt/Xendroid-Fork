// SPDX-License-Identifier: WTFPL
#ifndef XENIA_GAME_QUIRKS_H_
#define XENIA_GAME_QUIRKS_H_

#include <cstddef>
#include <cstdint>

namespace xe {
namespace game_quirks {

// Applies per-title cvar overrides at the game-config priority (above the
// global config, below the title's own config file). Returns count applied.
size_t Apply(uint32_t title_id);

}  // namespace game_quirks
}  // namespace xe

#endif  // XENIA_GAME_QUIRKS_H_
