# IoT Vision App
Analyze images captured by USB camera and then move camera angle by using servo motors.

## HOW TO RUN - First run

### 1. Flash binary
Tizen 5.0 M2 IoT 부트 이미지 다운로드 [[링크](http://download.tizen.org/releases/milestone/tizen/unified/tizen-unified_20181024.1/images/standard/iot-boot-armv7l-artik533s/)]

Tizen 5.0 M2 Iot Headed 이미지 다운로드 [[링크](http://download.tizen.org/releases/milestone/tizen/unified/tizen-unified_20181024.1/images/standard/iot-headed-3parts-armv7l-artik530_710/)]
```
sudo minicom
    thordown
lthor tizen-unified_20181024.1_iot-boot-armv7l-artik533s.tar.gz tizen-unified_20181024.1_iot-headed-3parts-armv7l-artik530_710.tar.gz
```

### 2. Install plug-in

ARTIK 530(5.0) Plugin download from https://developer.samsung.com/tizendevice/firmware


### 3. Install Wifi util (*optional)
Tizen 5.0 M2 wifi-manager-tool 다운로드 [[링크](http://download.tizen.org/releases/milestone/tizen/unified/tizen-unified_20181024.1/repos/standard/packages/armv7l/capi-network-wifi-manager-tool-1.0.39-81.5.armv7l.rpm)]
```
sdb root on; sdb shell 'mount -o remount,rw /'

sdb push capi-network-wifi-manager-tool-1.0.39-81.5.armv7l.rpm /tmp

sdb shell 'rpm -ivh /tmp/capi-network-wifi-manager-tool-1.0.39-81.5.armv7l.rpm'
```

### 4. Build and install custom version of IoT.js to include WebSocket feature
```
git clone https://github.com/Samsung/iotjs.git

cd iotjs

sed -i 's/Release: 0/Release: 99/' config/tizen/packaging/iotjs.spec

sed -i '/--no-parallel-build/a \ \ --cmake-param=-DENABLE_MODULE_WEBSOCKET=ON \\' config/tizen/packaging/iotjs.spec

git diff config/tizen/packaging/iotjs.spec

./config/tizen/gbsbuild.sh --clean

sdb push ~/GBS-ROOT/local/repos/tizen_unified_m1/armv7l/RPMS/iotjs-1.0.0-9.0.armv7l.rpm /tmp

sdb shell 'cd /tmp; rpm -ivh iotjs-1.0.0-9.0.armv7l.rpm'
```
- https://github.com/Samsung/iotjs/wiki/Build-for-RPi3-Tizen
- https://github.com/Samsung/iotjs/blob/master/docs/api/IoT.js-API-WebSocket.md


### 5. Change camera setting
```
sdb shell "cat /etc/multimedia/mmfw_camcorder_camera0.ini | grep 640"

sdb shell "sed -i 's/640,480/320,240/g' /etc/multimedia/mmfw_camcorder_camera0.ini"

sdb shell "cat /etc/multimedia/mmfw_camcorder_camera0.ini | grep 320"
```

### 6. Build and install vision app
```
git clone git://git.tizen.org/apps/native/smart-surveillance-camera # This git

cd smart-surveillance-camera

gbs build -A armv7l --include-all

sdb push ~/GBS-ROOT/local/BUILD-ROOTS/scratch.armv7l.0/home/abuild/rpmbuild/RPMS/armv7l/iot-vision-camera-0.0.1-1.armv7l.rpm /tmp

sdb shell 'rpm -ivh --force /tmp/iot-vision-camera-0.0.1-1.armv7l.rpm'
```
### 7. Some more settings
```
sdb shell 'pkg_initdb'
```
### 8. Launch vision app
```
sdb shell 'app_launcher -s iot-vision-camera'
```
### 9. Check vision app activity
```
sdb shell 'ls -l /tmp/latest.jpg'
```
### 10. Install and run monitor server
* If you install latest version of "iot-vision-camera" package, the monitor server is automatically launched in booting time

* To check status of the monitor server
	```
	sdb shell 'systemctl status iot-dashboard'
	```

* To restart the monitor server
	```
	sdb shell 'systemctl stop iot-dashboard'
	sdb shell 'systemctl start iot-dashboard'
	sdb shell 'systemctl status iot-dashboard'  # To check status
	```

* To update some files of the monitor server, push the files to dashboard path `/opt/home/dashboard` and restart the monitor server as above
	```
	sdb shell push {path/yourfile} /opt/home/dashboard/{path}
	```
OR! YOU CAN SIMPLY USE THE SCRIPT:
```
./update-dashboard.sh
```


## HOW TO RUN - Subsequent Runs
```
sdb root on; sdb shell 'mount -o remount,rw /'

sdb shell 'app_launcher -s iot-vision-camera'

 ./update-dashboard.sh
```

## Profiling Data

### 카메라의 물리적 이동시간
슈퍼슬로우 카메라로 촬영결과 500ms 정도 소요

### Rpi

#### Image Encoding (buffer -> jpg file)
소요시간 20 ~ 135ms (가끔씩 오래걸림, 장담 못함)

#### Vision Survailance (input -> cb event)
소요시간 38 ~ 50ms


### Artik

#### Image Encoding (buffer -> jpg file)
소요시간 10 ~ 20ms, 대부분 10ms 초반 안정적

#### Vision Survailance (input -> cb event)
소요시간 24 ~ 42ms

## Vision 움직임 정보 형식 (exif)
최대 244Byte 크기의 스트링으로 모두 숫자로 이루어져있다.

첫 2자리 숫자는 분석결과의 타입으로 TT 의 값을 갖는다.

그 다음 2자리 숫자는 포함된 움직임의 갯수 NN 의 값을 갖는다.

하나의 움직임은 8개 숫자로 구성되며 xxyywwhh 의 값을 갖는다.

xx: x 상대 좌표
yy: y 상대 좌표
ww: 상대 넓이
hh: 상대 높이

각각 0~99까지의 범위를 갖는 4개의 숫자를 이어놓은 것이다.
1자리 숫자의 경우 0을 넣어서 전체 길이를 고정한다.

TTNNxxyywwhhxxyywwhh....xxyywwhh 의 형태의 스트링이 된다.
