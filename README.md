# Real-CUGAN ncnn Vulkan plugin for AviSynth+

![CI](https://github.com/dimag0g/realcugan-ncnn-vulkan/actions/workflows/CI.yml/badge.svg?branch=avisynth)

ncnn implementation of Real-CUGAN converter. Runs fast on Intel / AMD / Nvidia / Apple-Silicon with Vulkan API.
This repo provides AviSynth+ plugin implementing Real-CUGAN in `avisyth` branch.

## [Download](https://github.com/dimag0g/realcugan-ncnn-vulkan/releases)

Download Windows/Linux/MacOS Executable for Intel/AMD/Nvidia/Apple-Silicon GPU

This package includes all the binaries and models required. It is portable, so no CUDA or PyTorch runtime environment is needed :)


## Usages

### Example avs script

```avs
LoadPlugin("AviSynthPlus-realcugan-x64.dll")
FFVideoSource("test.mkv") # or AviSource("test.avi")
ConvertToRGB32()
realcugan(noise=0, scale=2, gpu_thread=1)
ConvertToYV12(matrix="PC.709")
```

### Supported parameters

- `noise` = noise level, large value means strong denoise effect, -1 = no effect
- `scale` = scale level, 1 = no scaling, 2 = upscale 2x
- `model` = model directory to use, 0: "models-nose", 1: "models-pro" 2: "models-se", default = 2.
- `tilesize` = tile size (min = 32), use smaller value to reduce GPU memory usage, default selects automatically
- `gpu_id` = ID of GPU to use, default is zero (first available).
- `gpu_thread` = thread count for the realcugan upscaling, using larger values increases GPU usage and consumes more GPU memory, default is one.
- `list_gpu` = simply prints a list of available GPU devices on the frame and does nothing else.

Not yet supported: `sync` = sync gap mode, 0 = no sync, 1 = accurate sync, 2 = rough sync, 3 = very rough sync


### Troubleshooting

If you increase `gpu_thread`, don't forget to tell AviSynth+ to spawn several threads for your filter:

```avs
realcugan(noise=0, scale=2, gpu_thread=2)
SetFilterMTMode("realcugan", MT_MULTI_INSTANCE)
Prefetch(2)
```

If you encounter a crash or error, try upgrading your GPU driver:

- Intel: https://downloadcenter.intel.com/product/80939/Graphics-Drivers
- AMD: https://www.amd.com/en/support
- NVIDIA: https://www.nvidia.com/Download/index.aspx

## Build from Source

1. Download and setup the Vulkan SDK from https://vulkan.lunarg.com/
  - For Linux distributions, you can either get the essential build requirements from package manager
```shell
dnf install vulkan-headers vulkan-loader-devel
```
```shell
apt-get install libvulkan-dev
```
```shell
pacman -S vulkan-headers vulkan-icd-loader
```

2. Clone this project with all submodules

```shell
git clone --recurse-submodules https://github.com/dimag0g/realcugan-ncnn-vulkan
cd realcugan-ncnn-vulkan
git switch avisynth
```

3. Build with CMake
  - You can pass -DUSE_STATIC_MOLTENVK=ON option to avoid linking the vulkan loader library on MacOS

```shell
mkdir build
cd build
cmake ../src
cmake --build . -j 4
```

On Windows, you can build with Visual Studio by simply opening the `src` directory.
CMake should automatically configure the project when you do.
Once the project is configured, right-click on CMakeLists.txt and pick "Build".

## Sample Images

### Original Image

![origin](images/0.jpg)

### Upscale 2x Lanczos Filter

```avs
LanczosResize(width * 2, height * 2)
```

![browser](images/4.png)

### Upscale 2x with Real-CUGAN

```shell
realcugan(noise=0, scale=2, gpu_thread=1)
```

![realcugan](images/2.png)

## Original Projects

Real-CUGAN (Real Cascade U-Nets for Anime Image Super Resolution)

- https://github.com/bilibili/ailab/tree/main/Real-CUGAN
- https://github.com/nihui/realcugan-ncnn-vulkan

Waifu2x AviSynth+ plugin used as a base

- https://github.com/Asd-g/AviSynthPlus-w2xncnnvk

## Other Open-Source Code Used

- https://github.com/Tencent/ncnn for fast neural network inference on ALL PLATFORMS
- https://github.com/webmproject/libwebp for encoding and decoding Webp images on ALL PLATFORMS
- https://github.com/nothings/stb for decoding and encoding image on Linux / MacOS
- https://github.com/tronkko/dirent for listing files in directory on Windows
