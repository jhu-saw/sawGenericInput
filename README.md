# sawGenericInput

cisst/SAW input through `mtsEvdev`: Linux physical keyboards, pedals, joystick
buttons, axes, triggers, and hats via libevdev. All devices use the same JSON
configuration format and `core/share/saw-evdev.schema.json`.

HID support is deferred. dVRK's current HID head sensor remains in dVRK.
The repository is `sawGenericInput`, with the same core/ROS organization as
other cisst/SAW components:

```text
sawGenericInput/
  core/
    components/    # library: code/ and include/sawGenericInput/
    applications/  # optional Qt evdev configuration tool
    examples/      # optional Qt joystick example
    tests/         # hardware-independent tests
    share/         # configurations, schema, and udev rules
  ros/             # separate optional ROS frontend package
```

`core/colcon.pkg` defines one plain CMake package, `sawGenericInputCore`.
Colcon builds all of `core/` as a single CMake project, including the library,
examples, configuration files, optional applications, and tests. Its subdirectories
are not separate ROS packages. The shared library and CMake package are
`sawGenericInput`. Public headers use `sawGenericInput/` and configuration helpers
use the `sawGenericInput` namespace.

## Compilation and configuration

See [jhu-saw/vcs](https://github.com/jhu-saw/vcs) for download and build instructions.

Requires Linux, cisst 1.5 with JSON support, a C++17 compiler, CMake, pkg-config,
jsoncpp, and libevdev. The additional Ubuntu development packages are
`libevdev-dev` and `libjsoncpp-dev`.

Options:

| Option | Default | Purpose |
| --- | --- | --- |
| `sawGenericInput_WITH_QT` | OFF | Build `sawGenericInputConfigQt` and `sawGenericInputQtExample` |
| `BUILD_TESTING` | ON | Build hardware-independent tests |

The optional ROS frontend is a separate package, `saw_generic_input_ros`,
with a dependency on `sawGenericInputCore`.

The ROS executable is `generic_input`, installed under
`lib/saw_generic_input_ros`. Run it using `ros2 run saw_generic_input_ros generic_input`
(or `rosrun saw_generic_input_ros generic_input` for ROS 1).

## Qt evdev configuration tool

Enable the optional Qt tools with `-DsawGenericInput_WITH_QT=ON` when configuring
`sawGenericInputCore`. After sourcing your workspace, start the configurator:

```sh
sawGenericInputConfigQt
# Optionally preselect one input node; it does not start observing automatically:
sawGenericInputConfigQt --device /dev/input/event22
```

1. Choose the device and click **Start observing**. For a composite device such
   as the R1 ring, its keyboard, touch, and consumer-control nodes appear
   separately. Input access uses the same device-specific permissions as the
   runtime component. The tool never grabs a device or changes Bluetooth pairing.
2. Operate a control, then **Stop** to freeze the captured observations. Filter
   by event type or code name to isolate a control. The table groups each code
   and shows its latest value, observed range, counts, and last complete key-hold
   duration. The bounded log shows new events matching the current filter.
   **Clear observations** starts a fresh observation list without deleting mappings.
3. Select an event and enter its provided interface names, separated by commas,
   and quick-tap threshold for a button. A blank field includes the button only
   in aggregate `input_data`.
   For absolute axes, use raw values or enable calibration and set its input and
   output ranges. **Use observed range** and **Use latest as center** help fill
   calibration values. Axis names are their event codes; the runtime schema does
   not provide named axis interfaces. Click **Add / update mapping** to apply edits.
4. Reorder or remove mappings as needed. Order defines the corresponding digital
   or analog vector indices. Choose name/ID matching or an absolute device path
   (prefer a stable symlink), and choose whether the generated configuration
   should grab the device exclusively when used by the runtime component.
5. Copy the complete JSON or a mappings fragment. **Save full configuration**
   always writes a complete device configuration, regardless of preview mode.
   Output is checked with the actual sawGenericInput configuration loader before
   copy/save is enabled; duplicate/reserved names and invalid calibration fail
   validation. Copy a fragment into an existing configuration by merging its
   `buttons`/`axes` entries; a fragment alone is not a complete config file.

Observed events and mappings remain separate for each selected input node during
this session. No changes are written until you explicitly save. A disconnected
or overflowing device stops observation and retains the captured work. Refresh
and restart observing after reconnecting. Keyboard capture is limited to the
selected device while observing. Synchronization events are omitted; other event
types remain visible for diagnosis, but relative motion and multitouch axes
cannot be exported because the current runtime does not support those mappings.

## Evdev configuration and interfaces

See `core/share/pcsensor-pedals.json`, `core/share/joystick.json`, and
`core/share/saw-evdev.schema.json`. The existing pedal configuration remains valid.

- `device`: an absolute `/dev/input/eventN` path or stable symlink; alternatively
  an object with exact `name` and optional four-digit hexadecimal `vendor_id`
  and `product_id`. Every configured key and axis must be supported. Multiple
  matching nodes are rejected. Event numbers can change after reboot.
- `exclusive`: defaults to true. Grabs **all** input from the selected node,
  including unconfigured keys. Other nodes of a composite device are separate.
  Use false for a shared joystick or keyboard. Failed grabs remain disconnected
  and retry; capture does not silently fall back to shared input.
- `buttons`: ordered array of `key` entries (`KEY_A`, `BTN_TRIGGER`, or numeric
  EV_KEY codes). Array order defines `prmInputData.DigitalInputs()` indices.
  An optional `interface` exposes a named `Button` write event carrying
  `prmEventButton`. Use `interfaces` with a nonempty array of names to send the
  same events to multiple interfaces, e.g.
  `{"key": "BTN_TRIGGER", "interfaces": ["Clutch", "Camera"]}`.
  Specify either `interface` or `interfaces`. Names must be unique across all
  button mappings; `Device` and `Input` are reserved. Each physical button keeps
  one digital-input index regardless of its number of interfaces. All its
  interfaces share the quick-tap threshold and receive identical event payloads,
  including releases during disconnection and recovery.
  Omitting both interface fields includes the button only in aggregate
  `input_data`; unlisted buttons are ignored.
- `quick_tap_ms`: per-button nonnegative integer; zero disables click detection.
  Down emits PRESSED immediately, up emits RELEASED immediately, then CLICKED
  for holds at most the threshold. Repeats and duplicate transitions are ignored.
  Durations use kernel monotonic timestamps, independently of polling latency.
- `axes`: ordered array of `code` entries, e.g. `ABS_X`, `ABS_Z`, `ABS_HAT0X`.
  Array order defines `AnalogInputs()` indices. Values are raw by default.
  Axes appear only in aggregate `input_data`, with no named interfaces;
  unlisted axes are ignored. At least one button or axis is required.
  Multitouch axes are excluded.

Optional axis `calibration` specifies `minimum < center < maximum`, plus
`deadzone` in raw units (default zero). Values inside `center +/- deadzone` map
to `output_center`; each remaining side maps linearly, with endpoint clamping.
Output defaults are `output_minimum: -32767`, `output_center: 0`, and
`output_maximum: 32767`. Set all three explicitly for another range. Swapping
output endpoints permits inversion. Calibration is explicit and does not import
joydev/jscal corrections. Example input bounds in `joystick.json` must be adjusted
to measurements from your device.

`Input` provides an `input_data` read command and write event using `prmInputData`.
Changed aggregate snapshots are published at `SYN_REPORT` boundaries. Unfinished
reports never appear in the read command, even if an event budget splits a
report across task iterations. Named button events preserve each transition,
including a press/release pair within one aggregate report.

`Device` provides `connected`, `period_statistics`, Status/Warning/Error message
events, `set_device` (absolute path), `open_device`, and `close_device`. Closing
stops retries; opening resumes them. Setting a new device while configured closes
and reconnects. A path supplied with `SetDevice` before Configure overrides JSON.

C++ applications can configure the same mappings directly, without a JSON file:

```cpp
sawGenericInput::Configuration config;
config.Path = "/dev/input/by-id/YOUR-KEYBOARD-event-kbd";
config.Exclusive = true;
config.Buttons = {{KEY_SPACE, {"Keyboard"}, 0}, {KEY_Q, {"Quit"}, 0}};
mtsEvdev keyboard("Keyboard");
keyboard.Configure(config);
```

Include `<linux/input-event-codes.h>` for the key codes. Programmatic and JSON
configuration use the same validation and can each be applied only once.

The default task period is 2 ms, with at most 256 events read without blocking
per iteration. Missing devices are retried once per second. Disconnection and
queue overflow release buttons and publish zeroed, invalid aggregate data.
Connection/recovery publishes an initial valid snapshot with current axes.
Held buttons remain logically released in both named events and aggregate data
until physically released and pressed again. No recovery action emits CLICKED.
Cleanup releases buttons, invalidates data, and drops capture.

## Examples and frontends

```sh
sawGenericInputQtExample -j core/share/joystick.json
ros2 run saw_generic_input_ros generic_input -j core/share/joystick.json
```

Both frontends instantiate `mtsEvdev` and provide input and system-monitor
widgets. The Qt monitor executable is `sawGenericInputQtExample`.
The ROS executable and node are named `generic_input`; `/joystick/input_data`
and the device-control topics retain their existing namespace. Their cisst
connections use the `Input` and `Device` interfaces.
The `-j` evdev configuration is required;
`-d` optionally overrides the device node. Numeric joydev button/axis indices and
`/dev/input/jsN` paths are not accepted as substitutes for evdev mappings.

## Device access and dVRK

`core/share/udev/70-saw-evdev-pcsensor.rules` grants the active local seat user access to
PCsensor USB 3553:b001 and creates `/dev/input/saw-pcsensor-pedals`:

```sh
sudo install -m 0644 core/share/udev/70-saw-evdev-pcsensor.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
```

Unplug/replug afterward. Rules are never activated automatically during package
installation. Headless/remote use may require administrator-managed permissions.
For identical devices, use individual by-id/by-path nodes instead of the shared
convenience symlink. Exclusive capture is released when the descriptor closes.

Use `core/share/manager-pedals.json` in your component manager. It loads
`sawGenericInput` and creates an `mtsEvdev` component. For pedals, dVRK
`input_type: COMPONENTS` terminals point to component `Pedals`, interfaces
`Clutch`, `Camera`, `OperatorPresent`.
