#define _HAS_STD_BYTE 0
#define _USE_MATH_DEFINES
#define IMGUI_DEFINE_MATH_OPERATORS

#include <cmath>

#include <renderer/math.cpp>
#include <renderer/color.cpp>
#include <renderer/texture.cpp>
#include <renderer/scene.cpp>
#include <renderer/devicedx12.cpp>
#include <renderer/imguirendererdx12.cpp>
#include <renderer/scenerendererdx12.cpp>
#include <renderer/scenerenderersoftware.cpp>

#include <imgui_impl_dx12.cpp>
#include <imgui.cpp>
#include <imgui_demo.cpp>
#include <imgui_draw.cpp>
#include <imgui_tables.cpp>
#include <imgui_widgets.cpp>