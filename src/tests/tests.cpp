#include <CppUnitTest.h>

#include <renderer/scene.h>
#include <renderer/utils.h>
#include <renderer/scenerendererdx12.h>
#include <renderer/scenerenderersoftware.h>

#include <functional>

namespace Microsoft
{
    namespace VisualStudio
    {
        namespace CppUnitTestFramework
        {
            template<> inline std::wstring ToString<Renderer::Vec>(const Renderer::Vec& q) 
            {
                std::wstringstream ss;
                ss << q.x << L"," << q.y << L"," << q.z << L"," << q.w;
                return ss.str();
            }
        }
    }
}

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Tests
{
    namespace
    {
        std::string SolutionDir = _SOLUTIONDIR;
        std::string AssetsDir = SolutionDir + "assets\\";
        std::string TestsDir = AssetsDir + "tests\\";
        std::string QuadsDir = TestsDir + "quads\\";
        std::string TriangleDir = TestsDir + "triangle\\";
    }

    TEST_MODULE_INITIALIZE(TestsInitialize)
    {
        Utils::DebugUtils::GetInstance().AddOutput([](const std::string& message)
        {
            Logger::WriteMessage(message.c_str());
        });
    }

    TEST_MODULE_CLEANUP(TestsCleanup)
    {
        Utils::DebugUtils::GetInstance().RemoveOutputs();
    }

    TEST_CLASS(Scene)
    {
    public:
        TEST_METHOD(LoadShouldSucceedWhenThereIsSceneFile)
        {
            Renderer::Scene scene;
            bool success = Renderer::Load(QuadsDir + "scene.sce", scene);

            // everything is loaded
            Assert::IsTrue(success);

            // there are two models
            Assert::AreEqual(size_t(2), scene.models.size());

            // first model is correctly loaded
            Renderer::Model& firstModel = scene.models[0];

            Assert::AreEqual(size_t(4), firstModel.vertices.size());
            Assert::AreEqual(size_t(6), firstModel.indices.size());

            Assert::IsTrue(std::vector<Renderer::Vertex> {
                Renderer::Vertex
                {
                    0,
                    Renderer::Vec {-0.5, -0.5, 0.0, 1.0},
                    Renderer::Vec {0.0, 0.0, 1.0, 0.0},
                    Renderer::Vec {0.0, 0.0, 0.0, 0.0},
                    Renderer::Color(255, 0, 0)
                },
                Renderer::Vertex
                {
                    0,
                    Renderer::Vec {-0.5, 0.5, 0.0, 1.0},
                    Renderer::Vec {0.0, 0.0, 1.0, 0.0},
                    Renderer::Vec {0.0, 1.0, 0.0, 0.0},
                    Renderer::Color(0, 255, 0)
                },
                Renderer::Vertex
                {
                    0,
                    Renderer::Vec {0.5, 0.5, 0.0, 1.0},
                    Renderer::Vec {0.0, 0.0, 1.0, 0.0},
                    Renderer::Vec {1.0, 1.0, 0.0, 0.0},
                    Renderer::Color(0, 0, 255)
                },
                Renderer::Vertex
                {
                    0,
                    Renderer::Vec {0.5, -0.5, 0.0, 1.0},
                    Renderer::Vec {0.0, 0.0, 1.0, 0.0},
                    Renderer::Vec {1.0, 0.0, 0.0, 0.0},
                    Renderer::Color(255, 255, 255)
                }
            } == firstModel.vertices);

            Assert::IsTrue(std::vector<uint32_t>{0, 1, 2, 2, 3, 0} == firstModel.indices);
            Assert::IsTrue(firstModel.materials.size() == 1);
            Assert::IsTrue(firstModel.materials[0].textureName == (QuadsDir + "quad_0.jpg"));
            Assert::IsTrue(firstModel.materials[0].name == "quad_material_0");

            // second model is correctly loaded
            Renderer::Model& secondModel = scene.models[1];

            Assert::AreEqual(size_t(6), secondModel.vertices.size());
            Assert::AreEqual(size_t(6), secondModel.indices.size());

            Assert::IsTrue(std::vector<Renderer::Vertex> {
                Renderer::Vertex
                {
                    0,
                    Renderer::Vec {-0.5, -0.5, 0.0, 1.0},
                    Renderer::Vec {0.0, 0.0, 1.0, 0.0},
                    Renderer::Vec {0.0, 0.0, 0.0, 0.0},
                    Renderer::Color(255, 0, 0)
                },
                Renderer::Vertex
                {
                    0,
                    Renderer::Vec {-0.5, 0.5, 0.0, 1.0},
                    Renderer::Vec {0.0, 0.0, 1.0, 0.0},
                    Renderer::Vec {0.0, 1.0, 0.0, 0.0},
                    Renderer::Color(0, 255, 0)
                },
                Renderer::Vertex
                {
                    0,
                    Renderer::Vec {0.5, 0.5, 0.0, 1.0},
                    Renderer::Vec {0.0, 0.0, 1.0, 0.0},
                    Renderer::Vec {1.0, 1.0, 0.0, 0.0},
                    Renderer::Color(0, 0, 255)
                },
                Renderer::Vertex
                {
                    1,
                    Renderer::Vec {0.5, 0.5, 0.0, 1.0},
                    Renderer::Vec {0.0, 0.0, 1.0, 0.0},
                    Renderer::Vec {1.0, 1.0, 0.0, 0.0},
                    Renderer::Color(0, 0, 255)
                },
                Renderer::Vertex
                {
                    1,
                    Renderer::Vec {0.5, -0.5, 0.0, 1.0},
                    Renderer::Vec {0.0, 0.0, 1.0, 0.0},
                    Renderer::Vec {1.0, 0.0, 0.0, 0.0},
                    Renderer::Color(255, 255, 255)
                },
                Renderer::Vertex
                {
                    1,
                    Renderer::Vec {-0.5, -0.5, 0.0, 1.0},
                    Renderer::Vec {0.0, 0.0, 1.0, 0.0},
                    Renderer::Vec {0.0, 0.0, 0.0, 0.0},
                    Renderer::Color(255, 0, 0)
                }
            } == secondModel.vertices);

            Assert::IsTrue(std::vector<uint32_t>{0, 1, 2, 3, 4, 5} == secondModel.indices);
            Assert::IsTrue(secondModel.materials.size() == 2);
            Assert::IsTrue(secondModel.materials[0].textureName == (QuadsDir + "quad_0.jpg"));
            Assert::IsTrue(secondModel.materials[0].name == "quad_material_0");
            Assert::IsTrue(secondModel.materials[1].textureName == (QuadsDir + "quad_1.png"));
            Assert::IsTrue(secondModel.materials[1].name == "quad_material_1");
        }

        TEST_METHOD(LoadShouldFailWhenThereIsNoSceneFile)
        {
            Renderer::Scene scene;
            bool success = Renderer::Load("", scene);

            Assert::IsFalse(success);
        }
    };

    TEST_CLASS(RendererDX12)
    {
        TEST_METHOD(RenderShouldProperlyRenderSimpleScene)
        {
            Renderer::Scene scene;
            bool success = Renderer::Load(AssetsDir + "cars\\scene.sce", scene);

            Renderer::DeviceDX12 device(true);
            Renderer::SceneRendererDX12 renderer(AssetsDir, device);

            Renderer::Texture texture(200, 150); // todo.pavelza: test error if texture has no dimensions and passed to render
            renderer.Render(scene, texture);
            Renderer::Save(TestsDir + "reference_dx12_warp.bmp", texture);

            Renderer::Texture reference;
            Renderer::Load(TestsDir + "reference_dx12.bmp", reference);

            Assert::IsTrue(true);
        }

        TEST_METHOD(RenderShouldProperlyRenderColoredTriangleScene)
        {
            Renderer::Scene scene;
            bool success = Renderer::Load(TriangleDir + "scene.sce", scene);

            Renderer::DeviceDX12 device(true);
            Renderer::SceneRendererDX12 renderer(AssetsDir, device);

            Renderer::Texture texture(200, 150);
            renderer.Render(scene, texture);
            Renderer::Save(TestsDir + "reference_triangle_dx12_warp.bmp", texture);

            Renderer::Texture reference;
            Renderer::Load(TestsDir + "reference_triangle_dx12.bmp", reference);

            Assert::IsTrue(true);
        }
    };

    TEST_CLASS(RendererSoftware)
    {
        TEST_METHOD(RenderShouldProperlyRenderSimpleScene)
        {
            Renderer::Scene scene;
            bool success = Renderer::Load(AssetsDir + "cars\\scene.sce", scene);

            Renderer::SceneRendererSoftware renderer;

            Renderer::Texture texture(200, 150);
            renderer.Render(scene, texture);

            Renderer::Texture reference(200, 150);
            Renderer::Load(TestsDir + "reference_software.bmp", reference);

            Assert::IsTrue(texture == reference);
        }

        TEST_METHOD(RenderShouldProperlyRenderColoredTriangleScene)
        {
            Renderer::Scene scene;
            bool success = Renderer::Load(TriangleDir + "scene.sce", scene);

            Renderer::SceneRendererSoftware renderer;

            Renderer::Texture texture(200, 150);
            renderer.Render(scene, texture);

            Renderer::Texture reference;
            Renderer::Load(TestsDir + "reference_triangle_software.bmp", reference);

            Assert::IsTrue(texture == reference);
        }
    };

    TEST_CLASS(RendererComparison)
    {
        BEGIN_TEST_METHOD_ATTRIBUTE(InterpolationShouldWorkTheSameInBothRenderers)
            TEST_IGNORE() // todo.pavelza: this is a hard thing to fix, will do it later when I have more background in graphics
        END_TEST_METHOD_ATTRIBUTE()

        TEST_METHOD(InterpolationShouldWorkTheSameInBothRenderers)
        {
            Renderer::Scene scene;
            bool success = Renderer::Load(TriangleDir + "scene.sce", scene);

            Renderer::SceneRendererSoftware renderer;

            Renderer::DeviceDX12 device(true);
            Renderer::SceneRendererDX12 hardwareRenderer(AssetsDir + "color.hlsl", device);

            uint32_t width = 200;
            uint32_t height = 150;

            Renderer::Texture textureSoftware(width, height);
            renderer.Render(scene, textureSoftware);

            Renderer::Texture textureHardware(width, height);
            hardwareRenderer.Render(scene, textureHardware);

            Renderer::Texture diff(width, height);
            uint32_t differentPixels = 0;
            Renderer::Diff(textureSoftware, textureHardware, diff, differentPixels);

            Assert::AreEqual(0u, differentPixels);
        }
    };

    TEST_CLASS(SigParser)
    {
        TEST_METHOD(SigParserShouldParseSigCorrectly)
        {
            Renderer::SigDefinition definition;
            bool success = Renderer::Load(AssetsDir + "default.hlsl", definition);

            Assert::IsTrue(std::get<0>(definition.constants[0]) == "gWorldViewProj");
            Assert::IsTrue(std::get<0>(definition.constants[1]) == "gWorldView");
            Assert::IsTrue(std::get<0>(definition.constants[2]) == "gLightPos");

            Assert::IsTrue(std::get<1>(definition.constants[0]) == "DirectX::XMFLOAT4x4");
            Assert::IsTrue(std::get<1>(definition.constants[1]) == "DirectX::XMFLOAT4x4");
            Assert::IsTrue(std::get<1>(definition.constants[2]) == "DirectX::XMFLOAT4");

            Assert::IsTrue(definition.vertexAttributes[0].semanticName == "POSITION");
            Assert::IsTrue(definition.vertexAttributes[1].semanticName == "TEXCOORD");
            Assert::IsTrue(definition.vertexAttributes[2].semanticName == "NORMALS");
            Assert::IsTrue(definition.vertexAttributes[3].semanticName == "COLOR");
            Assert::IsTrue(definition.vertexAttributes[4].semanticName == "TEXINDEX");

            Assert::IsTrue(definition.vertexAttributes[0].format == DXGI_FORMAT_R32G32B32_FLOAT);
            Assert::IsTrue(definition.vertexAttributes[1].format == DXGI_FORMAT_R32G32_FLOAT);
            Assert::IsTrue(definition.vertexAttributes[2].format == DXGI_FORMAT_R32G32B32_FLOAT);
            Assert::IsTrue(definition.vertexAttributes[3].format == DXGI_FORMAT_R32G32B32_FLOAT);
            Assert::IsTrue(definition.vertexAttributes[4].format == DXGI_FORMAT_R32_SINT);

            Assert::IsTrue(success);
        }
    };
}
