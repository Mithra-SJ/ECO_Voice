"""
PlatformIO extra script: packs and flashes the ESP-SR model partition on every upload.
Runs AFTER the app binary is uploaded.
"""
import subprocess
import sys
import os

Import("env")

MODEL_TARGET_DIR = "managed_components/espressif__esp-sr/model/target"
PACK_SCRIPT     = "managed_components/espressif__esp-sr/model/pack_model.py"
# pack_model.py writes output inside MODEL_TARGET_DIR, not cwd
MODEL_BIN       = os.path.join(MODEL_TARGET_DIR, "srmodels.bin")
MODEL_OFFSET    = "0x310000"


def flash_model_partition(source, target, env):
    # Step 1 — pack model files into srmodels.bin (written inside MODEL_TARGET_DIR)
    if not os.path.exists(MODEL_BIN):
        print("\n[flash_model] Packing ESP-SR models into srmodels.bin ...")
        result = subprocess.run(
            [sys.executable, PACK_SCRIPT, "-m", MODEL_TARGET_DIR, "-o", "srmodels.bin"],
            check=False
        )
        if result.returncode != 0:
            print("[flash_model] ERROR: pack_model.py failed.")
            return
    else:
        print(f"\n[flash_model] {MODEL_BIN} already exists — skipping pack step.")

    # Step 2 — flash srmodels.bin to the model partition
    port = env.get("UPLOAD_PORT", "COM8")
    print(f"\n[flash_model] Flashing {MODEL_BIN} → offset {MODEL_OFFSET} on {port} ...")

    # Use PlatformIO's bundled esptool — avoids dependency on system Python esptool
    pio_esptool = os.path.join(
        os.path.expanduser("~"), ".platformio", "packages", "tool-esptoolpy", "esptool.py"
    )
    esptool_args = [
        sys.executable, pio_esptool,
        "--chip", "esp32s3",
        "--port", port,
        "--baud", "460800",
        "write_flash",
        MODEL_OFFSET, MODEL_BIN,  # MODEL_BIN is the full relative path
    ]
    result = subprocess.run(esptool_args, check=False)
    if result.returncode == 0:
        print("[flash_model] Model partition flashed successfully.")
    else:
        print("[flash_model] ERROR: esptool failed. Close serial monitor and retry.")


env.AddPostAction("upload", flash_model_partition)
