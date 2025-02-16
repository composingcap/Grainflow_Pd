Grainflow is a powerful granulation tool build for Max designed to be as flexible as possible to allow for rapid experimentation with granulation. Grainflow is a synchronous and sample accurate granulator what also is offering a toolkit of vast customization. 


## Key Features
- Multichannel soundfile and live granulation
- Control of grains using snake signals
- Use custom envelopes including 2D buffers
- Per grain spatialization in circular space
- Graphical tools to help visualize grains on a waveform
  
## Advanced Features 
- Set delay, window placement, and pitch as banks using buffers
- Support for grouping grains into streams
- Set grain level loop points 
- Set glisson curves using Max buffers 
- Provides detailed grain information 

## Nightly Releases
- Win: https://nightly.link/composingcap/Grainflow_Pd/workflows/cmake-multi-platform/master/Grainflow-windows-latest.zip
- Mac: https://nightly.link/composingcap/Grainflow_Pd/workflows/cmake-multi-platform/master/Grainflow-macos-latest.zip
- CI build for linux currently is not working

## How to contribute
Grainflow is open to contributions both in terms of examples, abstractions, and help file improvements as well as help with the C++ codebase in `./source` and the [GrainflowLib](https://github.com/composingcap/GrainflowLib) repository. \
Additionally, please post any feature requests to [Issues](https://github.com/composingcap/Grainflow_pd/issues) or the [Grainflow Discord](https://discord.gg/8RUUUvjVgK). \
We are also happy to assist in implementation of GrainflowLib outside of Max and am planning to target other platforms as I have time. 
## Building from source
Building from source is the best way to ensure you get the most up-to-date changes

You must have [cmake](https://cmake.org/) and both a c and cpp compiler.
### Downloading the repo
Open a terminal window in the directory you would like to place grainflow.
If you would like to place grainflow in you Max Packages directory, make sure to remove any other grainflow folder.
```
git clone https://github.com/composingcap/Grainflow_pd.git --recursive
```

## How to build
### Clone this repo 
```
git clone https://github.com/composingcap/Grainflow_Pd.git --recursive 
```
### Build
**On windows, you must move the Pd linked library into the pd source folder** 
``` 
cp {path-to-pd}/bin/pd.lib {path-to-repo}/source/pure-data/src/
cp {path-to-pd}/bin/pthreadVC.lib {path-to-repo}/source/pure-data/src/
cp {path-to-pd}/bin/pthreadVC.dll {path-to-repo}/source/pure-data/src/
```
**Inside of this root of this repo run**
```
mkdir ./build
cd build
cmake ..
cmake --build .
```
