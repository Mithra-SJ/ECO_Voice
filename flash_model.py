"""
PlatformIO extra script: packs and flashes the ESP-SR model partition on every upload.
Runs AFTER the app binary is uploaded.
"""

import os
import subprocess
import sys

Import("env")

MODEL_TARGET_DIR = "components/esp-sr/model/target"
PACK_SCRIPT = "components/esp-sr/model/pack_model.py"
MODEL_BIN = os.path.join(MODEL_TARGET_DIR, "srmodels.bin")
MODEL_OFFSET = "0x310000"


def flash_model_partition(source, target, env):
    # Always rebuild srmodels.bin so the flashed model partition stays in sync
    # with the installed esp-sr component and selected target models.
    print("\n[flash_model] Repacking ESP-SR models into srmodels.bin ...")
    if os.path.exists(MODEL_BIN):
        os.remove(MODEL_BIN)

    result = subprocess.run(
        [sys.executable, PACK_SCRIPT, "-m", MODEL_TARGET_DIR, "-o", "srmodels.bin"],
        check=False
    )
    if result.returncode != 0:
        print("[flash_model] ERROR: pack_model.py failed.")
        return

    port = env.get("UPLOAD_PORT", "COM8")
    print(f"\n[flash_model] Flashing {MODEL_BIN} to offset {MODEL_OFFSET} on {port} ...")

    pio_esptool = os.path.join(
        os.path.expanduser("~"), ".platformio", "packages", "tool-esptoolpy", "esptool.py"
    )
    esptool_args = [
        sys.executable,
        pio_esptool,
        "--chip", "esp32s3",
        "--port", port,
        "--baud", "460800",
        "write_flash",
        MODEL_OFFSET, MODEL_BIN,
    ]
    result = subprocess.run(esptool_args, check=False)
    if result.returncode == 0:
        print("[flash_model] Model partition flashed successfully.")
    else:
        print("[flash_model] ERROR: esptool failed. Close serial monitor and retry.")


env.AddPostAction("upload", flash_model_partition)
