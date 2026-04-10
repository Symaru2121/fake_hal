#!/bin/bash


set -e

echo "Setting up AOSP build environment..."


sudo apt update
sudo apt install -y openjdk-17-jdk python3 python3-pip git curl unzip ccache


mkdir -p ~/bin
curl https://storage.googleapis.com/git-repo-downloads/repo > ~/bin/repo
chmod a+x ~/bin/repo
export PATH=$PATH:~/bin


export AOSP_ROOT=$HOME/aosp
mkdir -p $AOSP_ROOT
cd $AOSP_ROOT


repo init -u https://android.googlesource.com/platform/manifest -b android-14.0.0_r1


repo sync -c -j$(nproc) --no-tags --no-clone-bundle \
  platform/frameworks/av \
  platform/frameworks/native \
  platform/system/core \
  platform/hardware/interfaces \
  device/google/pantah \
  device/google/gs201 \
  device/google/flame \
  device/google/coral

echo "AOSP setup complete."


cd ~
mkdir actions-runner && cd actions-runner
curl -o actions-runner-linux-x64-2.311.0.tar.gz -L https://github.com/actions/runner/releases/download/v2.311.0/actions-runner-linux-x64-2.311.0.tar.gz
tar xzf ./actions-runner-linux-x64-2.311.0.tar.gz

echo "Runner downloaded. Now configure:"
echo "1. Go to https://github.com/Symaru2121/fake_hal/settings/actions/runners"
echo "2. Click 'New self-hosted runner'"
echo "3. Choose Linux, x64"
echo "4. Copy the token"
echo "5. Run: ./config.sh --url https://github.com/Symaru2121/fake_hal --token YOUR_TOKEN"
echo "6. Run: nohup ./run.sh &"

echo "After setup, push code to trigger build."
