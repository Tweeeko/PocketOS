# Device controls

Every device opens its own control screen on PocketOS. Anything with a level
(brightness, temperature, volume, speed, position) gets a **dial**: a 270° glass arc
around the edge of the round screen. On/off-only things get one **big glass button**.

The prototype of these screens is `design/device-prototype.html`.

## Interaction (same on every control screen)

| Input | Action |
|---|---|
| Drag along the arc | Set the level; sent when the finger lifts |
| − / + buttons | One step down / up (heating: the thermostat's own step, usually 0.5°; lights and volume 5 %; fans and blinds 10 %) |
| Tap centre button | Main action (on/off, play/pause, heat on/off, lock/unlock) |
| Swipe right | Back |
| BOOT double press | Straight back to the companion |

## Honest updates

- While a change is being sent, the screen says **Sending…** and pulses.
- The new value is shown as confirmed **only when Home Assistant reports the new state**
  (WebSocket `state_changed` event for that entity), never just because PocketOS sent it.
- If Home Assistant is unreachable the change is refused, the companion looks worried,
  and the dial snaps back to the real value.
- While a control screen is open, state changes made elsewhere (the app, a wall switch,
  an automation, the thermostat warming the room) update the screen live.

## Home Assistant mapping

Controls are only shown when the entity actually supports them. PocketOS reads the
entity's `supported_features` and attributes at discovery time.

| Entity domain | Dial | Centre button | Extras | Home Assistant actions | Shown only if |
|---|---|---|---|---|---|
| `light` | Brightness 1–100 % | On / off | — | `light.turn_on` with `brightness_pct`, `light.turn_off` | brightness: `supported_color_modes` is not just `onoff` |
| `climate` | Target temperature, 0.5° steps, between the entity's `min_temp` and `max_temp` | Heating on / off | − / + buttons; shows `current_temperature` | `climate.set_temperature` (`temperature`), `climate.set_hvac_mode` (`heat` / `off`) | target temperature feature supported |
| `media_player` | Volume 0–100 | Play / pause | Previous / next track; shows media title | `media_player.volume_set` (`volume_level` 0–1), `media_player.media_play_pause`, `media_player.media_previous_track`, `media_player.media_next_track` | each control only if its feature is supported |
| `fan` | Speed 0–100 % | On / off | — | `fan.set_percentage`, `fan.turn_on`, `fan.turn_off` | speed: set-speed feature supported |
| `cover` (blinds, curtains) | Position 0–100 % | Open / close | Close / stop / open buttons; moves live | `cover.set_cover_position`, `cover.open_cover`, `cover.close_cover`, `cover.stop_cover` | position: set-position feature; otherwise buttons only |
| `lock` | — | Big lock / unlock button | Shows Locking… / Unlocking… until confirmed | `lock.lock`, `lock.unlock` | — |
| `switch` (plugs) | — | Big on / off button | — | `switch.toggle` | — |
| `sensor` (temperature etc.) | Read-only gauge | — | Live reading | none (read-only) | numeric state |

Feature flag values were checked against the Home Assistant core source
(`homeassistant/components/*/const.py`): media player PAUSE 1, VOLUME_SET 4, PREVIOUS_TRACK 16,
NEXT_TRACK 32, PLAY 16384; climate TARGET_TEMPERATURE 1; fan SET_SPEED 1; cover OPEN 1, CLOSE 2,
SET_POSITION 4, STOP 8.

## Where it lives in the code

- `firmware/main/core/model.c`: the device table, capabilities from `supported_features`,
  status text, moods. `core/ha_proto.c` builds every `call_service` message (with clamping and
  step snapping) and parses replies. Both are unit-tested on a PC (`make -C firmware/host test`).
- `firmware/main/ui/ui_control.c`: the control screen (`lv_arc`, 270°, value label, up to three
  glass buttons). Dragging only updates the label; one action is sent on release.
- A WebSocket subscription to `state_changed` keeps the table live; the open screen redraws from it.
  A change shows **Sending…** until Home Assistant confirms it, and "No response" after 6 s.

## Side buttons (whole device)

| Button | Action |
|---|---|
| BOOT press | Show or hide the Settings code (wakes the screen if it's off). During a website reset request: confirms the reset |
| BOOT press twice | Back to the companion |
| BOOT hold | Talk to Home Assistant (push-to-talk), release to send |
| PWR press | Screen off / on |
| PWR hold | Hardware power-off by the AXP2101 (PocketOS shows a hint first) |
