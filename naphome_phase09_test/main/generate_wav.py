#!/usr/bin/env python3
"""
Generate "Nap home" WAV file at 44.1kHz using Google TTS API
"""

import requests
import json
import base64
import struct
import sys

# Google TTS API key (same as in the firmware)
GOOGLE_TTS_API_KEY = "AIzaSyCjrdIBkpGWGXa4u9UileFFIMBZ_ZnMZ1w"
GOOGLE_TTS_URL = f"https://texttospeech.googleapis.com/v1/text:synthesize?key={GOOGLE_TTS_API_KEY}"

def generate_wav(text, output_file, sample_rate=44100):
    """Generate WAV file from text using Google TTS"""
    
    # Prepare the request
    request_data = {
        "input": {"text": text},
        "voice": {
            "languageCode": "en-US",
            "name": "en-US-Standard-D",
            "ssmlGender": "NEUTRAL"
        },
        "audioConfig": {
            "audioEncoding": "LINEAR16",
            "sampleRateHertz": sample_rate
        }
    }
    
    print(f"Requesting TTS for: '{text}' at {sample_rate}Hz...")
    
    # Make the API request
    response = requests.post(
        GOOGLE_TTS_URL,
        headers={"Content-Type": "application/json"},
        json=request_data,
        timeout=15
    )
    
    if response.status_code != 200:
        print(f"Error: HTTP {response.status_code}")
        print(f"Response: {response.text}")
        return False
    
    # Parse the response
    result = response.json()
    if "audioContent" not in result:
        print(f"Error: No audioContent in response")
        print(f"Response: {result}")
        return False
    
    # Decode the base64 audio
    audio_data = base64.b64decode(result["audioContent"])
    
    # Create WAV file
    # WAV header structure:
    # - RIFF header (12 bytes)
    # - fmt chunk (24 bytes for PCM)
    # - data chunk
    
    num_channels = 1  # Mono
    bits_per_sample = 16
    byte_rate = sample_rate * num_channels * (bits_per_sample // 8)
    block_align = num_channels * (bits_per_sample // 8)
    data_size = len(audio_data)
    file_size = 36 + data_size  # 36 = header size
    
    with open(output_file, 'wb') as f:
        # RIFF header
        f.write(b'RIFF')
        f.write(struct.pack('<I', file_size))
        f.write(b'WAVE')
        
        # fmt chunk
        f.write(b'fmt ')
        f.write(struct.pack('<I', 16))  # fmt chunk size
        f.write(struct.pack('<H', 1))   # audio format (1 = PCM)
        f.write(struct.pack('<H', num_channels))
        f.write(struct.pack('<I', sample_rate))
        f.write(struct.pack('<I', byte_rate))
        f.write(struct.pack('<H', block_align))
        f.write(struct.pack('<H', bits_per_sample))
        
        # data chunk
        f.write(b'data')
        f.write(struct.pack('<I', data_size))
        f.write(audio_data)
    
    print(f"Successfully generated {output_file} ({len(audio_data)} bytes of PCM data)")
    print(f"  Sample rate: {sample_rate}Hz")
    print(f"  Channels: {num_channels} (mono)")
    print(f"  Bits per sample: {bits_per_sample}")
    
    return True

if __name__ == "__main__":
    text = "Nap home"
    output_file = "offline_welcome.wav"
    
    if len(sys.argv) > 1:
        text = sys.argv[1]
    if len(sys.argv) > 2:
        output_file = sys.argv[2]
    
    success = generate_wav(text, output_file, sample_rate=44100)
    sys.exit(0 if success else 1)
