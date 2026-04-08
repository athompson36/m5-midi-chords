#!/usr/bin/env python3
"""
CrowPanel 1.28" Firmware Flasher
Automatically detects ESP32S3 and flashes all necessary components.
"""

import subprocess
import sys
import os
import platform
import time

try:
    from serial.tools import list_ports as serial_list_ports
except Exception:
    serial_list_ports = None

# Firmware component offsets and filenames
FIRMWARE_COMPONENTS = [
    (0x0,      "bootloader.bin",  "Bootloader"),
    (0x8000,   "partitions.bin",  "Partition Table"),
    (0x10000,  "firmware.bin",    "Application Firmware"),
]

BAUD_RATE = 921600
BAUD_RATE_FALLBACK = 115200

def has_esptool():
    """Check if esptool is available as a Python module in this environment."""
    try:
        result = subprocess.run(
            [sys.executable, "-m", "esptool", "version"],
            capture_output=True,
            text=True,
        )
        return result.returncode == 0
    except Exception:
        return False

def list_ports():
    """List available serial ports"""
    if serial_list_ports is not None:
        ports = list(serial_list_ports.comports())
        if not ports:
            return "No serial ports found."
        lines = [f"{port.device} - {port.description}" for port in ports]
        return "\n".join(lines)

    # Fallback for older environments where pyserial is not directly importable.
    try:
        for cmd in (["list-ports"], ["list_ports"]):
            result = subprocess.run(
                [sys.executable, "-m", "esptool"] + cmd,
                capture_output=True,
                text=True,
            )
            if result.returncode == 0 and result.stdout.strip():
                return result.stdout
        return "No serial ports found."
    except Exception as e:
        print(f"❌ Error listing ports: {e}")
        return None

def detect_esp32_port():
    """Auto-detect ESP32S3 port"""
    if serial_list_ports is not None:
        ports = list(serial_list_ports.comports())
        if len(ports) == 1:
            port = ports[0].device
            print(f"✅ Auto-detected port: {port}")
            return port
        elif len(ports) > 1:
            print("📋 Multiple ports found:")
            for port_info in ports:
                print(f"{port_info.device} - {port_info.description}")
            port = input("📌 Enter port (e.g., COM3 or /dev/ttyUSB0): ").strip()
            return port if port else None

    try:
        port_text = list_ports()
        if port_text and port_text.strip() and port_text.strip() != "No serial ports found.":
            lines = [line.strip() for line in port_text.splitlines() if line.strip()]
            if len(lines) == 1:
                port = lines[0].split()[0]
                print(f"✅ Auto-detected port: {port}")
                return port
            if len(lines) > 1:
                print("📋 Multiple ports found:")
                print(port_text)
                port = input("📌 Enter port (e.g., COM3 or /dev/ttyUSB0): ").strip()
                return port if port else None
    except Exception as e:
        print(f"⚠️ Could not auto-detect port: {e}")
    
    return None

def verify_files():
    """Check if all firmware files exist"""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    for offset, filename, label in FIRMWARE_COMPONENTS:
        filepath = os.path.join(script_dir, filename)
        if not os.path.exists(filepath):
            print(f"❌ Missing file: {filename}")
            print(f"   Expected at: {filepath}")
            return False
        print(f"✅ Found: {filename} ({os.path.getsize(filepath)} bytes)")
    return True

def flash_firmware(port, baud_rate=BAUD_RATE):
    """Flash all firmware components"""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    
    # Build flash command arguments
    flash_args = [
        "--port", port,
        "--baud", str(baud_rate),
        "--before", "default_reset",
        "--after", "hard_reset",
        "write_flash"
    ]
    
    # Add each firmware component
    for offset, filename, label in FIRMWARE_COMPONENTS:
        filepath = os.path.join(script_dir, filename)
        flash_args.append(hex(offset))
        flash_args.append(filepath)
    
    print(f"\n🔧 Flashing with baud {baud_rate}...")
    print(f"   Command: esptool.py {' '.join([str(x) for x in flash_args])}")
    print()
    
    try:
        result = subprocess.run(
            [sys.executable, "-m", "esptool"] + flash_args,
            timeout=60
        )
        return result.returncode == 0
    except subprocess.TimeoutExpired:
        print("❌ Flash operation timed out")
        return False
    except Exception as e:
        print(f"❌ Flash error: {e}")
        return False

def erase_flash(port):
    """Erase entire flash (destructive, optional)"""
    print("\n⚠️  Erasing entire flash memory...")
    try:
        result = subprocess.run(
            [sys.executable, "-m", "esptool", "--port", port, "erase_flash"],
            timeout=30
        )
        return result.returncode == 0
    except Exception as e:
        print(f"❌ Erase error: {e}")
        return False

def main():
    print("=" * 60)
    print("🎵 CrowPanel Chord Suggester - Firmware Flasher")
    print("=" * 60)
    
    # Check esptool availability
    print("\n📡 Checking esptool.py...")
    if not has_esptool():
        print("❌ esptool.py not found!")
        print("\n💡 Install with: pip install esptool")
        sys.exit(1)
    print("✅ esptool.py is available")
    
    # Verify firmware files
    print("\n📦 Checking firmware files...")
    if not verify_files():
        sys.exit(1)
    
    # Detect port
    print("\n🔌 Detecting ESP32S3 port...")
    port = detect_esp32_port()
    
    if not port:
        print("\n💡 Can't auto-detect? Check:")
        print("   • USB cable is connected (DATA pins, not just power!)")
        print("   • Press Boot button, then hold Power for 1 sec (enters download mode)")
        print("   • Windows: Install CH34x driver")
        print("\n📋 Available ports:")
        print(list_ports())
        port = input("📌 Enter port manually: ").strip()
        if not port:
            print("❌ No port specified")
            sys.exit(1)
    
    # Optional erase
    erase = input("\n🗑️  Erase entire flash first? (y/n): ").strip().lower()
    if erase == 'y':
        if not erase_flash(port):
            print("⚠️  Erase failed, attempting to flash anyway...")
    
    # Flash firmware
    print("\n⏳ Starting firmware flash...")
    print("   ⚠️  DO NOT DISCONNECT USB CABLE!")
    print()
    
    if flash_firmware(port, BAUD_RATE):
        print("\n" + "=" * 60)
        print("✅ Firmware flashed successfully!")
        print("=" * 60)
        print("\n🎉 The CrowPanel should now boot with:")
        print("   • Diatonic (Green) chord suggestions by default")
        print("   • Rotate ring to change pages & see LED colors")
        print("   • Touch screen to trigger chords")
        print("   • MIDI output on Pin 43 (TX)")
        time.sleep(2)
        return 0
    else:
        print("\n⚠️  Flash at standard baud failed, trying fallback (115200)...")
        if flash_firmware(port, BAUD_RATE_FALLBACK):
            print("\n✅ Firmware flashed successfully (fallback baud)!")
            return 0
        
        print("\n" + "=" * 60)
        print("❌ Firmware flash failed")
        print("=" * 60)
        print("\n🔧 Troubleshooting:")
        print("   • Hold Boot button, then press Power to enter download mode")
        print("   • Check USB cable (data pins)")
        print("   • Try different baud rate: --baud 115200")
        print("   • Manual flash: see FLASH_INSTRUCTIONS.md")
        return 1

if __name__ == "__main__":
    sys.exit(main())
