# car-can-lab

An ESP32 + ELM327-over-BLE CAN reverse-engineering toolkit. Its first hardware target is the
yellow deck (an ESP32-2432S028R "Cheap Yellow Display") and its first car a Honda City. A sniff
firmware watches the bus in silent monitor mode, shows it live, and records what changed; PC tools
turn a recording into suspects.

Build:

    pio run -e sniff                     # build
    pio run -e sniff_ota -t upload       # update the deck over WiFi (PC on the phone hotspot)
    powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1   # host tests
