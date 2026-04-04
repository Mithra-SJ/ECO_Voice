from pathlib import Path
import csv
import subprocess

Import("env")


PROJECT_DIR = Path(env["PROJECT_DIR"])
BUILD_DIR = Path(env.subst("$BUILD_DIR"))
SDKCONFIG_PATH = PROJECT_DIR / f"sdkconfig.{env['PIOENV']}"
MOVE_MODEL_SCRIPT = PROJECT_DIR / "components" / "esp-sr" / "model" / "movemodel.py"
MODEL_IMAGE = BUILD_DIR / "srmodels" / "srmodels.bin"
PARTITIONS_FILE = PROJECT_DIR / env.GetProjectOption("board_build.partitions", "partitions.csv")


def _find_model_partition_offset():
    with PARTITIONS_FILE.open("r", encoding="utf-8") as fp:
        reader = csv.reader(
            line for line in fp if line.strip() and not line.lstrip().startswith("#")
        )
        for row in reader:
            if len(row) < 4:
                continue
            if row[0].strip() == "model":
                return row[3].strip()
    raise RuntimeError(f"Could not find 'model' partition in {PARTITIONS_FILE}")


def _generate_sr_models(target, source, env):
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    command = [
        env.subst("$PYTHONEXE"),
        str(MOVE_MODEL_SCRIPT),
        "-d1",
        str(SDKCONFIG_PATH),
        "-d2",
        str(PROJECT_DIR / "components" / "esp-sr"),
        "-d3",
        str(BUILD_DIR),
    ]
    subprocess.run(command, check=True, cwd=str(PROJECT_DIR))
    if not MODEL_IMAGE.exists():
        raise RuntimeError(f"ESP-SR model image was not generated: {MODEL_IMAGE}")
    print(f"Generated ESP-SR model image: {MODEL_IMAGE}")


MODEL_OFFSET = _find_model_partition_offset()
model_target = env.Command(str(MODEL_IMAGE), [], _generate_sr_models)
env.Depends("$BUILD_DIR/firmware.bin", model_target)
env.Depends("upload", model_target)
env.Append(FLASH_EXTRA_IMAGES=[(MODEL_OFFSET, str(MODEL_IMAGE))])
