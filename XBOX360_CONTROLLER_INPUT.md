# Xbox 360 Controller Input — GDScript Reference

## Device Index
All functions use device index `0` (first controller). Xbox 360 supports up to 4 controllers (indices 0–3), but this port currently only polls controller 0.

## Buttons

| Godot Constant | Xbox 360 Button | GDScript Check |
|---|---|---|
| `JOY_XBOX_A` | A | `Input.is_joy_button_pressed(0, JOY_XBOX_A)` |
| `JOY_XBOX_B` | B | `Input.is_joy_button_pressed(0, JOY_XBOX_B)` |
| `JOY_XBOX_X` | X | `Input.is_joy_button_pressed(0, JOY_XBOX_X)` |
| `JOY_XBOX_Y` | Y | `Input.is_joy_button_pressed(0, JOY_XBOX_Y)` |
| `JOY_L` | Left Bumper (LB) | `Input.is_joy_button_pressed(0, JOY_L)` |
| `JOY_R` | Right Bumper (RB) | `Input.is_joy_button_pressed(0, JOY_R)` |
| `JOY_L2` | Left Trigger (digital) | `Input.is_joy_button_pressed(0, JOY_L2)` |
| `JOY_R2` | Right Trigger (digital) | `Input.is_joy_button_pressed(0, JOY_R2)` |
| `JOY_L3` | Left Stick Click | `Input.is_joy_button_pressed(0, JOY_L3)` |
| `JOY_R3` | Right Stick Click | `Input.is_joy_button_pressed(0, JOY_R3)` |
| `JOY_START` | Start | `Input.is_joy_button_pressed(0, JOY_START)` |
| `JOY_SELECT` | Back | `Input.is_joy_button_pressed(0, JOY_SELECT)` |
| `JOY_GUIDE` | Xbox/Guide Button | `Input.is_joy_button_pressed(0, JOY_GUIDE)` |
| `JOY_DPAD_UP` | D-Pad Up | `Input.is_joy_button_pressed(0, JOY_DPAD_UP)` |
| `JOY_DPAD_DOWN` | D-Pad Down | `Input.is_joy_button_pressed(0, JOY_DPAD_DOWN)` |
| `JOY_DPAD_LEFT` | D-Pad Left | `Input.is_joy_button_pressed(0, JOY_DPAD_LEFT)` |
| `JOY_DPAD_RIGHT` | D-Pad Right | `Input.is_joy_button_pressed(0, JOY_DPAD_RIGHT)` |

## Analog Axes

| Godot Constant | Xbox 360 Control | Range | GDScript Read |
|---|---|---|---|
| `JOY_ANALOG_LX` | Left Stick X | -1.0 to 1.0 | `Input.get_joy_axis(0, JOY_ANALOG_LX)` |
| `JOY_ANALOG_LY` | Left Stick Y | -1.0 to 1.0 | `Input.get_joy_axis(0, JOY_ANALOG_LY)` |
| `JOY_ANALOG_RX` | Right Stick X | -1.0 to 1.0 | `Input.get_joy_axis(0, JOY_ANALOG_RX)` |
| `JOY_ANALOG_RY` | Right Stick Y | -1.0 to 1.0 | `Input.get_joy_axis(0, JOY_ANALOG_RY)` |
| `JOY_ANALOG_L2` | Left Trigger | 0.0 to 1.0 | `Input.get_joy_axis(0, JOY_ANALOG_L2)` |
| `JOY_ANALOG_R2` | Right Trigger | 0.0 to 1.0 | `Input.get_joy_axis(0, JOY_ANALOG_R2)` |

## D-Pad as Hat
The D-pad is also available as a hat value via `Input.get_joy_hat(0)`:
- Returns a bitmask: Up=1, Right=2, Down=4, Left=8
- Example: Up+Right = 3, Down+Left = 12

## Input Actions (UI Navigation)
You can map controller buttons to Godot input actions in **Project → Project Settings → Input Map**:

| Action | Recommended Button |
|---|---|
| `ui_up` | D-Pad Up / Left Stick Up |
| `ui_down` | D-Pad Down / Left Stick Down |
| `ui_left` | D-Pad Left / Left Stick Left |
| `ui_right` | D-Pad Right / Left Stick Right |
| `ui_accept` | A button |
| `ui_cancel` | B button |

Or check directly in GDScript:
```gdscript
if Input.is_action_pressed("ui_right"):
    # move right
if Input.is_action_just_pressed("ui_accept"):
    # confirm selection
```

## Example: Reading Controller in GDScript
```gdscript
extends Node

func _process(delta):
    var lx = Input.get_joy_axis(0, JOY_ANALOG_LX)
    var ly = Input.get_joy_axis(0, JOY_ANALOG_LY)
    
    if lx > 0.5:
        print("Moving right")
    elif lx < -0.5:
        print("Moving left")
    
    if Input.is_joy_button_pressed(0, JOY_XBOX_A):
        print("A pressed!")
    
    if Input.is_joy_button_pressed(0, JOY_DPAD_UP):
        print("D-pad up")
```

## Notes
- Controller data is polled **every frame** in the `run()` loop
- Only **controller 0** is polled (Xbox 360 supports up to 4, but this port only reads the first)
- Stick values are normalized from raw signed 16-bit (-32768 to 32767) to float (-1.0 to 1.0)
- Trigger values are normalized from 0-255 to 0.0-1.0
- No deadzone is applied to raw `get_joy_axis()` values — you may want to add a small deadzone in your GDScript code. Input Map actions have their own deadzone setting (the demo uses `0.75`).
