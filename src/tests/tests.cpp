#include <CppUnitTest.h>

#include <utils.h>

#include <renderer/scene.h>
#include <renderer/scenerendererdx12.h>
#include <renderer/scenerenderersoftware.h>

#include <functional>
#include <filesystem>

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
        std::string BuildDir;
        std::string AssetsDir;
        std::string TestsDir;
        std::string QuadsDir;
        std::string TriangleDir;
    }

    TEST_MODULE_INITIALIZE(TestsInitialize)
    {
        BuildDir = std::filesystem::current_path().string() + "\\";
        AssetsDir = BuildDir + "assets\\";
        TestsDir = AssetsDir + "tests\\";
        QuadsDir = TestsDir + "quads\\";
        TriangleDir = TestsDir + "triangle\\";

        Utils::DebugUtils::GetInstance().AddOutput([](const std::string& message)
        {
            Logger::WriteMessage(message.c_str());
        });

        LOG("BuildDir: " << BuildDir);
        LOG("AssetsDir: " << AssetsDir);
        LOG("TestsDir: " << TestsDir);
        LOG("QuadsDir: " << QuadsDir);
        LOG("TriangleDir: " << TriangleDir);
    }

    TEST_MODULE_CLEANUP(TestsCleanup)
    {
        Utils::DebugUtils::GetInstance().RemoveOutputs();
    }

    TEST_CLASS(Scene)
    {
        TEST_METHOD(LoadShouldSucceedWhenThereIsSceneFile)
        {
            Renderer::Scene scene;
            bool success = Renderer::Load(QuadsDir + "scene.sce", scene);

            // everything is loaded
            Assert::IsTrue(success);
            Assert::AreEqual(std::string("quads"), scene.name);

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
            Assert::AreEqual(std::string(), scene.name);

            success = Renderer::Load("NonExistantDrive:\\", scene);

            Assert::IsFalse(success);
            Assert::AreEqual(std::string(), scene.name);

            success = Renderer::Load("NonExistantDrive:\\scene.sce", scene);

            Assert::IsFalse(success);
            Assert::AreEqual(std::string(), scene.name);

            success = Renderer::Load("\\", scene);

            Assert::IsFalse(success);
            Assert::AreEqual(std::string(), scene.name);

            success = Renderer::Load("\\\\", scene);

            Assert::IsFalse(success);
            Assert::AreEqual(std::string(), scene.name);
        }
    };

    TEST_CLASS(RendererDX12)
    {
        TEST_METHOD(RenderShouldProperlyRenderSimpleScene)
        {
            Renderer::Scene scene;
            bool success = Renderer::Load(AssetsDir + "cars\\scene.sce", scene);

            Assert::IsTrue(success);
            Assert::AreEqual(std::string("cars"), scene.name);

            Renderer::DeviceDX12 device(Renderer::DeviceDX12::Mode::UseSoftwareRasterizer);
            Renderer::SceneRendererDX12 renderer(AssetsDir, device);

            Renderer::Texture texture(200, 150); // todo.pavelza: test error if texture has no dimensions and passed to render
            renderer.Render(scene, texture);
            Renderer::Save(BuildDir + "dx12.bmp", texture);

            Renderer::Texture reference;
            Renderer::Load(TestsDir + "reference_dx12.bmp", reference);

            Renderer::Texture result(200, 150);
            uint32_t differentPixelsCount = 0;
            Renderer::Diff(texture, reference, result, differentPixelsCount);

            Renderer::Save(BuildDir + "dx12_diff.bmp", result);

            float error = static_cast<float>(differentPixelsCount) / (200.0f * 150.0f);
            Assert::IsTrue(error < 0.01);
        }

        TEST_METHOD(RenderShouldProperlyRenderColoredTriangleScene)
        {
            Renderer::Scene scene;
            bool success = Renderer::Load(TriangleDir + "scene.sce", scene);

            Assert::IsTrue(success);
            Assert::AreEqual(std::string("triangle"), scene.name);

            Renderer::DeviceDX12 device(Renderer::DeviceDX12::Mode::UseSoftwareRasterizer);
            Renderer::SceneRendererDX12 renderer(AssetsDir, device);

            Renderer::Texture texture(200, 150);
            renderer.Render(scene, texture);
            Renderer::Save(BuildDir + "triangle_dx12.bmp", texture);

            Renderer::Texture reference;
            Renderer::Load(TestsDir + "reference_triangle_dx12.bmp", reference);

            Renderer::Texture result(200, 150);
            uint32_t differentPixelsCount = 0;
            Renderer::Diff(texture, reference, result, differentPixelsCount);

            Renderer::Save(BuildDir + "triangle_dx12_diff.bmp", result);

            float error = static_cast<float>(differentPixelsCount) / (200.0f * 150.0f);
            Assert::IsTrue(error < 0.01);
        }
    };

    TEST_CLASS(RendererSoftware)
    {
        TEST_METHOD(RenderShouldProperlyRenderSimpleScene)
        {
            Renderer::Scene scene;
            bool success = Renderer::Load(AssetsDir + "cars\\scene.sce", scene);

            Assert::IsTrue(success);
            Assert::AreEqual(std::string("cars"), scene.name);

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

            Assert::IsTrue(success);
            Assert::AreEqual(std::string("triangle"), scene.name);

            Renderer::SceneRendererSoftware renderer;

            Renderer::Texture texture(200, 150);
            renderer.Render(scene, texture);

            Renderer::Texture reference;
            Renderer::Load(TestsDir + "reference_triangle_software.bmp", reference);

            Assert::IsTrue(texture == reference);
        }
    };
}
