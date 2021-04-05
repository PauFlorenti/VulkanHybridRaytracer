# VkPinut

## Intro
This is a render engine in Vulkan with Ray tracing capabilities for the final project in the computer science grade in Universitat Pompeu Fabra.

The aim of the project is to propose a Hybrid Rendering pipeline and compare its results and optimization with a rasterization and raytraced pipelines.

![alt text](https://github.com/Pinut97/vkPinut/blob/master/images/scene.jpg?raw=true)

### Raster
The rasterization pipeline is based in deferred shading. A previous pass builds the g-buffers and the lightning pass will use those to shade the scene.

### Raytraced
The raytraced pipeline its a pure raytraced scene. The number of samples per pixel can be modified up to 64. Same for the shadow rays samples.

### Hybrid
The hybrid pipeline takes advantage of the Gbuffers created in a previous pass to trace rays from there. The aim is to reduce the number of rays traced in order to improve performance.
