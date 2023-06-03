#include <CppUnitTest.h>

#include <renderer/scene.h>
#include <renderer/utils.h>
#include <renderer/rendererdx12.h>
#include <renderer/renderersoftware.h>

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

            Renderer::RendererDX12 renderer(AssetsDir + "color.hlsl");

            Renderer::Texture texture(200, 150); // todo.pavelza: test error if texture has no dimensions and passed to render
            renderer.Render(scene, texture);

            Renderer::Texture reference;
            Renderer::Load(TestsDir + "reference.bmp", reference);

            Assert::IsTrue(texture == reference);
        }

        TEST_METHOD(RenderShouldProperlyRenderColoredTriangleScene)
        {
            Renderer::Scene scene;
            bool success = Renderer::Load(TriangleDir + "scene.sce", scene);

            Renderer::RendererDX12 renderer(AssetsDir + "color.hlsl");

            Renderer::Texture texture(200, 150);
            renderer.Render(scene, texture);

            Renderer::Texture reference;
            Renderer::Load(TestsDir + "backface_culling_dx12.bmp", reference);

            Assert::IsTrue(texture == reference);
        }
    };

    TEST_CLASS(RendererSoftware)
    {
        TEST_METHOD(RenderShouldProperlyRenderSimpleScene)
        {
            Renderer::Scene scene;
            bool success = Renderer::Load(AssetsDir + "cars\\scene.sce", scene);

            Renderer::RendererSoftware renderer;

            Renderer::Texture texture(200, 150);
            renderer.Render(scene, texture);

            Renderer::Texture reference;
            Renderer::Load(TestsDir + "reference_software.bmp", reference);

            Assert::IsTrue(texture == reference);
        }

        TEST_METHOD(RenderShouldProperlyRenderColoredTriangleScene)
        {
            Renderer::Scene scene;
            bool success = Renderer::Load(TriangleDir + "scene.sce", scene);

            Renderer::RendererSoftware renderer;

            Renderer::Texture texture(200, 150);
            renderer.Render(scene, texture);

            Renderer::Texture reference;
            Renderer::Load(TestsDir + "reference_backface_culling.bmp", reference);

            Assert::IsTrue(texture == reference);
        }
    };
}
