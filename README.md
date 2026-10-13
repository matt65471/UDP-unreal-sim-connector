# UDP Unreal Simulation Connector

A dependency-free C++17 receiver for the JSON telemetry emitted by
`URocketUdpBridge` in the Unreal Space Program project.

The desktop receiver is the first HIL endpoint: it proves that live simulation
data crosses a physical Ethernet connection. The telemetry model and parser are
kept separate from the POSIX socket code so they can later be reused behind an
ESP32, STM32/lwIP, W5500, or other embedded network adapter.

## Network layout

For a direct Ethernet cable without a router:

| Device | Ethernet IPv4 address | Mask |
|---|---:|---:|
| Windows simulation PC | `192.168.50.1` | `255.255.255.0` |
| Mac receiver | `192.168.50.2` | `255.255.255.0` |

On `BP_Rocket`, select `UdpBridge` and configure:

```text
bAutoStart = true
TelemetryAddress = 192.168.50.2
TelemetryPort = 5762
TelemetryRateHz = 30
```

The receiver binds `0.0.0.0:5762`, so it accepts packets arriving through the
Mac's Ethernet interface.

## Build on macOS

Install the Xcode command-line tools and CMake, then run:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Receive telemetry

Start the receiver before pressing Play in Unreal:

```sh
./build/udp_receiver
```

Useful options:

```sh
./build/udp_receiver --help
./build/udp_receiver --raw
./build/udp_receiver --log flight-001.jsonl
./build/udp_receiver --bind 192.168.50.2 --port 5762
```

## Build on Windows

Install **Visual Studio 2022** with the **Desktop development with C++** workload.
Make sure the workload includes its CMake tools, then open **Developer PowerShell
for VS 2022** in this repository and run:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Start the receiver with:

```powershell
.\build\Release\udp_receiver.exe
```

Or bind it specifically to the Windows receiver's Ethernet address:

```powershell
.\build\Release\udp_receiver.exe --bind 192.168.50.2 --port 5762
```

The first time it runs, Windows Defender Firewall may ask for network access.
Allow it on **Private networks**. If no prompt appears and packets do not arrive,
open PowerShell as Administrator and add a narrow inbound rule:

```powershell
New-NetFirewallRule -DisplayName "Unreal Rocket UDP Telemetry" -Direction Inbound -Protocol UDP -LocalPort 5762 -Action Allow -Profile Private
```

If the simulation is also running on Windows, it must be on a different PC or
use a different local test setup for a physical-Ethernet demonstration. For two
Windows PCs connected directly, give the simulator `192.168.50.1/24`, give the
receiver `192.168.50.2/24`, and set `BP_Rocket`'s `TelemetryAddress` to
`192.168.50.2` exactly as in the network table above.

Windows builds use Winsock 2 directly and link the system `ws2_32` library; no
third-party networking package is required.

Expected output resembles:

```text
#000001 from=192.168.50.1 phase=PRELAUNCH        met=    0.03s agl=       0.0ft vs=     +0.0ft/s thrust=    0.0lbf
#000061 from=192.168.50.1 phase=POWERED_ASCENT   met=    2.03s agl=      12.4ft vs=    +38.7ft/s thrust=  438.1lbf
```

By default, every valid datagram is also appended in its original JSON form to
`telemetry.jsonl`. Capture files are ignored by Git.

## Troubleshooting

- Confirm both Ethernet interfaces are on `192.168.50.0/24` and can ping each other.
- Confirm Unreal logs `telemetry -> 192.168.50.2:5762` when Play begins.
- Allow inbound UDP port 5762 through the macOS firewall if prompted.
- Make sure another receiver is not already bound to port 5762.
- To prove traffic is using the cable, disable Wi-Fi temporarily and capture the
  Ethernet interface in Wireshark with the filter `udp.port == 5762`.

## Embedded direction

The current Unreal protocol is newline-terminated JSON over UDP. A capable
microcontroller can parse it, but the socket setup will be platform-specific.
Keep `Telemetry` and the packet-validation behavior consistent, replace the
POSIX `socket`/`bind`/`recvfrom` layer with the selected board's Ethernet stack,
and consider a versioned binary packet if timing or memory becomes constrained.
