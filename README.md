## Nightly Releases
- Win: https://nightly.link/composingcap/Grainflow_Pd/workflows/cmake-multi-platform/master/Grainflow-windows-latest.zip
- Mac: https://nightly.link/composingcap/Grainflow_Pd/workflows/cmake-multi-platform/master/Grainflow-macos-latest.zip
- CI build for linux currently are not working
## How to build
### Clone this repo 
```
git clone https://github.com/composingcap/Grainflow_Pd.git --recursive 
```
### Build
inside of this root of this repo run
```
mkdir ./build
cd build
cmake ..
cmake --build .
```
