# ESP-SR Wake Word Training Script for "Eco Wake"
# Run this after collecting audio samples

import os
import subprocess
import argparse

def collect_audio_samples(output_dir, num_samples=100):
    """
    Instructions for collecting audio samples:
    1. Record 100-200 utterances of "Eco Wake"
    2. Use different speakers (male/female, different ages)
    3. Record in various environments (quiet, noisy)
    4. Save as 16kHz, 16-bit WAV files
    """
    print("Audio Collection Instructions:")
    print("1. Use a microphone to record 'Eco Wake'")
    print("2. Save files as: eco_wake_001.wav, eco_wake_002.wav, etc.")
    print(f"3. Place in directory: {output_dir}")
    print("4. Each file should be 1-2 seconds long")
    print("5. Record from different people and environments")

def train_wake_word_model(data_dir, model_name="eco_wake"):
    """
    Train the wake word model using ESP-SR tools
    """
    # This requires ESP-IDF environment and ESP-SR tools
    cmd = [
        "python", "-m", "esp_sr.wake_word_training",
        "--data_dir", data_dir,
        "--wake_word", "eco wake",
        "--output_dir", f"models/{model_name}",
        "--epochs", "50",
        "--batch_size", "32"
    ]

    try:
        subprocess.run(cmd, check=True)
        print(f"Model trained successfully: models/{model_name}/wake_word.model")
    except subprocess.CalledProcessError as e:
        print(f"Training failed: {e}")
        print("Make sure ESP-IDF and ESP-SR are properly installed")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Train Eco Wake word model")
    parser.add_argument("--data_dir", default="datasets/eco_wake", help="Directory with audio samples")
    parser.add_argument("--train", action="store_true", help="Start training")

    args = parser.parse_args()

    if args.train:
        train_wake_word_model(args.data_dir)
    else:
        collect_audio_samples(args.data_dir)