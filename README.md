# AttysHRV

![alt tag](screenshots/attyshrv02.jpg)

An immersive biofeedback app which displays the heartrate variability to de-stress.

## Prerequisites

1. Change into `AttysHRV/app/src/main/cpp` and clone the IIR filter library and the spline interpolation library:

```
git clone https://github.com/berndporr/iir1.git
git https://github.com/berndporr/cxx-spline.git
```

2. Download the [Oculus openXR API](https://developer.oculus.com/downloads/package/oculus-openxr-mobile-sdk/), create
   a subdirectory `ovr_openxr_mobile_sdk` and unzip the contents of the openxr SDK into it.

   Edit `app/src/main/cpp/CMakeLists.txt` and point it to the openXR API:
  `set(OCULUS_OPENXR_MOBILE_SDK /home/yourname/ovr_openxr_mobile_sdk)`

3. Clone [AttysComm](https://github.com/glasgowneuro/AttysComm) and modify `app/build.gradle` so that it points
   to AttysComm.

## How to compile & run

Start Android Studio, open AttysHRV and click `run`.
