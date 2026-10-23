# Controls handoff for the website (Garage)

For the separate website session (the `recarbon-website` repository on the M1), which is
not changed from here. Generated from ReCarbon's source on 2026-10-24: the key defaults from
`runtime/user/settings.cpp`, the presets from `runtime/hid/profiles.h`, and the controller
from `runtime/hid/pad_map.h`. Re-derive from those files after any change; the site's
drift check should read the same files.

## Status words (use these, and nothing stronger)
- **Verified in play:** seen working by the user in hand tests 1, 3 and 4, on the Default keys
  and the keyboard.
- **Binding tested:** the key or button reaches the right Xbox 360 control. Unit tests pin it
  (`settings_keys_test`, `pad_map_test`, `profiles_test`). Says nothing about what the game
  does with the control.
- **Unverified:** no evidence of the game action. Show the Xbox control name only.

## Keyboard: Default and the two presets
| `[keys]` row | Settings label | Default | WASD | Arrows | Game action |
|---|---|---|---|---|---|
| `right_trigger` | Throttle | w | w | up | Throttle — verified in play |
| `left_trigger` | Brake | s | s | down | Brake — verified in play (reverse not seen) |
| `left_stick_left` | Steering Left | a, left | a | left | Steering left — verified in play |
| `left_stick_right` | Steering Right | d, right | d | right | Steering right — verified in play |
| `left_stick_up` | Left Stick Up | up | none | none | unverified (hand test 5) |
| `left_stick_down` | Left Stick Down | down | none | none | unverified (hand test 5) |
| `a` | A | space | space | space | unverified (hand test 5) |
| `b` | B | backspace | backspace | backspace | unverified (hand test 5) |
| `x` | X | left_shift | left_shift | left_shift | unverified (hand test 5) |
| `y` | Y | left_ctrl | left_ctrl | left_ctrl | unverified (hand test 5) |
| `lb` | LB | q | q | q | unverified (hand test 5) |
| `rb` | RB | e | e | e | unverified (hand test 5) |
| `dpad_up` | D-pad Up | up | up | w | unverified (hand test 5) |
| `dpad_down` | D-pad Down | down | down | s | unverified (hand test 5) |
| `dpad_left` | D-pad Left | left | left | a | unverified (hand test 5) |
| `dpad_right` | D-pad Right | right | right | d | unverified (hand test 5) |
| `start` | Start | return | return | return | unverified (hand test 5) |
| `back` | Back | escape | escape | escape | unverified (hand test 5) |
| `right_stick_up` | Right Stick Up | o | o | o | unverified (hand test 5) |
| `right_stick_down` | Right Stick Down | l | l | l | unverified (hand test 5) |
| `right_stick_left` | Right Stick Left | k | k | k | unverified (hand test 5) |
| `right_stick_right` | Right Stick Right | semicolon | semicolon | semicolon | unverified (hand test 5) |
| `left_stick_click` | Left Stick Click | z | z | z | unverified (hand test 5) |
| `right_stick_click` | Right Stick Click | c | c | c | unverified (hand test 5) |

- **The same in all three:** every row not shown above as different.
- **The presets** (user-approved, 2026-10-23): no key drives two controls. Their keys are
  **binding tested** (the input layer resolves them, `profiles_test`), not verified in play.
  Menu navigation with a real key under each preset is still pending (M8.11).
- **Labels:** use the Settings label column: **Throttle**, **Brake**, **Steering Left**,
  **Steering Right**, and the Xbox names for the rest.
  - Not "Gas": "Throttle" is the agreed term.
  - Not "Brake / Reverse": reversing hasn't been seen in play.
- **Rebinding:** players can rebind any row (Settings > Controls, stored as scancodes). The
  site shows defaults, not a player's keys.
- **Layouts:** a key whose name on the active keyboard layout isn't ASCII (Myanmar, Thai) can't
  be bound, and its label shows as '?'. English layouts are the supported case.

## Controller (Default mapping; buttons only)
| Xbox button | Controller button (by position) |
|---|---|
| A | South |
| B | East |
| X | West |
| Y | North |
| Back | Back |
| Start | Start |
| Left Stick Click | Left stick (press) |
| Right Stick Click | Right stick (press) |
| LB | Left shoulder |
| RB | Right shoulder |
| D-pad | D-pad |

- **The triggers and sticks** map straight through. They can't be rebound in v1, and neither
  can the deadzones or the paddles. Paddles aren't mapped at all: show them dimmed.
- **Rebinding** (Settings > Controller) swaps buttons. Back + Start always opens and closes
  ReCarbon's menu, whatever the mapping.
- **The physical controller is Deferred: not verified.** The mapping is binding tested with a
  virtual controller only. Make no claim that any physical controller, 8BitDo included, works.

## Unverified racing actions: don't label them
Handbrake, Nitrous, Speedbreaker, Change Camera and Look Back have no evidence. No source, test
or play ties them to a control, and other games' layouts (NFSU2, Most Wanted) aren't evidence
for Carbon. Keep them out of the diagrams' labels until hand test 5 maps them.

## The site's current data (read on 2026-10-24, not changed)
`src/data/controls.actions.json` uses:
- "Gas": change it to "Throttle";
- "Brake / reverse": change it to "Brake";
- "Steer left/right": change them to "Steering Left/Right".

It marks the other actions unverified, which is right. Its status for the four driving rows
("pending") can become "verified in play" for the Default keys only.
