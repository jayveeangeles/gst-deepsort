# Gstreamer Plugin for Deep SORT

Sample Gstreamer plugin for [Deep SORT](https://github.com/nwojke/deep_sort). Based off of this [C++ project](https://github.com/apennisi/deep_sort).

## To Build
```CXXFLAGS="-D_GLIBCXX_USE_CXX11_ABI=0" CMAKE_CXX_FLAGS_RELEASE="-D_GLIBCXX_USE_CXX11_ABI=0" cmake -DCMAKE_BUILD_TYPE=Release ..;make -j8 install``` This will install the headers in /usr/include and the library onto the gstreamer-1.0 library directory. By default, it expects Deep SORT to be in /workspace/deep_sort. If this is not the case, specify the root directory of Deep SORT project by adding __**-D DEEPSORT_ROOT_DIR**__ option when doing cmake. Use [my reference repository](https://github.com/jayveeangeles/deep_sort/tree/new-frozen-model) to create the standalone shared library to be used in conjunction with this plugin.

### References
1. [Nvidia's DeepStream Library](https://github.com/NVIDIA-AI-IOT/deepstream_reference_apps/tree/restructure)
2. Aforementioned libraries