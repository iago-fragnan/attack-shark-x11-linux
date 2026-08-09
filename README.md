<img width="722" height="380" alt="Screenshot DEMO" src="https://github.com/user-attachments/assets/242df54c-0cb7-4bb3-a49a-8738981876f3" />


## Attack Shark X11 Software GUI for Linux

This software is for the mouse [Attack Shark X11](https://attackshark.com/products/attack-shark-x11-wireless-gaming-mouse-charging-dock), which you can control basic settings from it like polling rate, color modes, this is a workaround software control and still being developed to support all features he has on Windows.

## Disclaimer
This project is an independent, unofficial, and community-driven effort. It is not affiliated with, endorsed by, sponsored by, or otherwise associated with Attack Shark, Guangzhou Shijunxingcheng Electronics Technology Co., Ltd., or PixArt Imaging Inc.
This repository does not contain any source code from the original software driver. All communication protocols used by this project were reverse engineered through lawful methods based on publicly observable behavior and independent analysis.
All trademarks, product names, and company names mentioned in this repository are the property of their respective owners and are used for identification purposes only.

## Install

**Build it yourself (Recommended):**
```sh
git clone https://github.com/iago-fragnan/attack-shark-x11-linux.git
# Install dependencies
# cmake build-essential pkg-config libudev-dev libusb-1.0-0-dev qt6-base-dev
cd attack-shark-x11-linux
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/attackshark-x11
```

**Arch Linux (AUR):**
```sh
paru -S attackshark-x11 # you can also use yay
```
> [!WARNING]
> **AUR package currently outdated due to the [AUR incident](https://archlinux.org/news/active-aur-malicious-packages-incident/).**

**Debian/Ubuntu:**
```sh
sudo apt-get install -y wget git cmake build-essential pkg-config libudev-dev libusb-1.0-0-dev qt6-base-dev
wget https://github.com/iago-fragnan/attack-shark-x11-linux/releases/download/v2.0.1/attackshark-x11-v2.0.1.deb
sudo dpkg -i attackshark-x11.deb
```


## TODO

- [x] Polling Rate
- [x] DPI Configuration
- [x] LED Color Control
- [x] Apply User-Selected Settings
- [x] Battery Percentage Monitoring
- [x] Display Current Applied Settings on Startup
- [x] Battery Charging Indicator
- [x] Power Management & Standby Control
- [x] Key Response Time
- [x] Ripple Control
- [x] Angle Snapping
- [x] Automatic Device Selection
- [x] Custom window for settings
    - [ ] Macro Support
    - [ ] Minimize to System Tray
    - [x] Profiles
        - [x] Apply
        - [ ] Save
        - [ ] Load
    - [x] Device selection
    - [ ] Auto Startup


## CREDITS

- Lightning icon from [Flaticon](https://www.flaticon.com/free-icon/flash_252851?term=lightning&related_id=252851)

- Settings icon from [Flaticon](https://www.flaticon.com/free-icon/settings_3524659?term=gear&related_id=3524659)

- Additional reverse engineering from [HarukaYamamoto0](https://github.com/HarukaYamamoto0/attack-shark-x11-driver/)
