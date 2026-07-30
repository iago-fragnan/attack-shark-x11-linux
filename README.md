<img width="722" height="380" alt="Screenshot DEMO" src="https://github.com/user-attachments/assets/242df54c-0cb7-4bb3-a49a-8738981876f3" />


## Attack Shark X11 Software GUI for Linux

This software is for the mouse [Attack Shark X11](https://attackshark.com/products/attack-shark-x11-wireless-gaming-mouse-charging-dock), which you can control basic settings from it like polling rate, color modes, this is a workaround software control and still being developed to support all features he has on Windows.

## Install

**Arch Linux (AUR):**
```sh
paru -S attackshark-x11 # you can also use yay
```

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

- Lightining Icon from [Flaticon](https://www.flaticon.com/free-icon/flash_252851?term=lightning&related_id=252851)

- Additional reverse engineering from [HarukaYamamoto0](https://github.com/HarukaYamamoto0/attack-shark-x11-driver/)
