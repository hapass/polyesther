# Description

Simple obj model viewer. Renders obj models with software rasterizer or DX12 renderer.

Press R to switch renderer.
Use WASD to control camera offset.
Use arrow keys to control camera angle.

## Build steps Windows

1. Clone the repository with submodules.
1. Install [VisualStudio](https://visualstudio.microsoft.com/downloads/)
1. Open polyesther.sln and build.

## TODO

1. Create imgui DX12 renderer.
1. Separate common "device" part of the two DX12 renderers.
1. Integrate imgui with DX12.
1. Draw to GPU texture on software rasterizer and remove readback texture from hardware DX12.
1. Draw output textures as Imgui::Image by passing GPU texture handle.
1. Add model loading with imgui.
1. Change camera controls to resemble those of windows 3D viewer (rotating around the model with mouse)
1. Add lighting, render feature, switching renderer controls to imgui.
1. Minimize code duplication in DX12 renderer.
1. Add SSAO to DX12 renderer.
1. Add SSR to DX12 renderer.
1. Fix backface culling in software renderer.
1. Refactoring: fix todos in code.
1. Add more render features to DX12 implementation for education purposes.
1. Fix window resize issues.
1. Speed up software rasterizer.