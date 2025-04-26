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
    std::string BuildDir;
    std::string AssetsDir;
    std::string TestsDir;
    std::string QuadsDir;
    std::string TriangleDir;
    std::string CarsDir;

    TEST_MODULE_INITIALIZE(TestsInitialize)
    {
        BuildDir = std::filesystem::current_path().string() + "\\";
        AssetsDir = BuildDir + "assets\\";
        TestsDir = AssetsDir + "tests\\";
        QuadsDir = TestsDir + "quads\\";
        TriangleDir = TestsDir + "triangle\\";
        CarsDir = AssetsDir + "cars\\";

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

    void RenderAndCompareToReference(Renderer::SceneRenderer& renderer, const Renderer::Scene& scene, const std::string& coreName)
    {
        constexpr uint32_t width = 200;
        constexpr uint32_t height = 150;

        Renderer::Texture texture(width, height);
        Assert::IsTrue(renderer.Render(scene, texture));

        Renderer::Texture reference;
        std::string referencePath(TestsDir + "reference_" + coreName + ".bmp");
        if (!Renderer::Load(referencePath, reference))
        {
            std::string savedReferencePath = BuildDir + "reference_" + coreName + ".bmp";

            LOG("Warning! No reference for the image: " << coreName << ". Saving rendered texture to: " << savedReferencePath << ". Please move it to: " << referencePath << ", if you think it should act as reference. Passing the test for now.");

            Assert::IsTrue(Renderer::Save(savedReferencePath, texture));
            Assert::IsTrue(Renderer::Load(savedReferencePath, reference));
        }

        Renderer::Texture diff(width, height);

        uint32_t differentPixelsCount = 0;
        Assert::IsTrue(Renderer::Diff(texture, reference, diff, differentPixelsCount));

        if (differentPixelsCount > 0)
        {
            Assert::IsTrue(Renderer::Save(BuildDir + coreName + ".bmp", texture));
            Assert::IsTrue(Renderer::Save(BuildDir + coreName + "_diff.bmp", diff));

            float error = static_cast<float>(differentPixelsCount) / (width * height);
            Assert::IsTrue(error < 0.01);
        }
        else
        {
            Assert::IsTrue(differentPixelsCount == 0);
        }
    }

    TEST_CLASS(RendererDX12)
    {
        TEST_METHOD(RenderShouldProperlyRenderSimpleScene)
        {
            Renderer::Scene scene;
            Assert::IsTrue(Renderer::Load(CarsDir + "scene.sce", scene));

            Renderer::DeviceDX12 device(Renderer::DeviceDX12::Mode::UseSoftwareRasterizer);
            Renderer::SceneRendererDX12 renderer(AssetsDir, device);

            RenderAndCompareToReference(renderer, scene, "dx12");
        }

        TEST_METHOD(RenderShouldProperlyRenderColoredTriangleScene)
        {
            Renderer::Scene scene;
            Assert::IsTrue(Renderer::Load(TriangleDir + "scene.sce", scene));

            Renderer::DeviceDX12 device(Renderer::DeviceDX12::Mode::UseSoftwareRasterizer);
            Renderer::SceneRendererDX12 renderer(AssetsDir, device);

            RenderAndCompareToReference(renderer, scene, "triangle_dx12");
        }

        TEST_METHOD(RenderShouldUseBackfaceCullingFlagProperly)
        {
            Renderer::Scene scene;
            Assert::IsTrue(Renderer::Load(CarsDir + "scene.sce", scene));

            Renderer::DeviceDX12 device(Renderer::DeviceDX12::Mode::UseSoftwareRasterizer);
            Renderer::SceneRendererDX12 renderer(AssetsDir, device);

            scene.camera.position = {0.0f, 2.3f, 5.0f, 1.0f };
            scene.camera.pitch = 0.7f;

            RenderAndCompareToReference(renderer, scene, "backface_0_dx12");

            scene.camera.position = {0.0f, -5.0f, 5.0f, 1.0f };
            scene.camera.pitch = 5.69f;

            RenderAndCompareToReference(renderer, scene, "backface_1_dx12");
        }

        TEST_METHOD(RenderShouldReturnFalseIfTextureHasZeroDimension)
        {
            Renderer::Scene scene;
            Assert::IsTrue(Renderer::Load(TriangleDir + "scene.sce", scene));

            Renderer::DeviceDX12 device(Renderer::DeviceDX12::Mode::UseSoftwareRasterizer);
            Renderer::SceneRendererDX12 renderer(AssetsDir, device);

            Renderer::Texture textureZeroHeight(200, 0);
            Assert::IsFalse(renderer.Render(scene, textureZeroHeight));

            Renderer::Texture textureZeroWidth(0, 150);
            Assert::IsFalse(renderer.Render(scene, textureZeroWidth));
        }

        TEST_METHOD(RenderShouldPaintMissingTexturesRed)
        {
            Renderer::Scene scene;
            Assert::IsTrue(Renderer::Load(CarsDir + "scene.sce", scene));

            scene.models[0].materials[0].textureName = "notfound";
            scene.models[0].materials[1].textureName = "notfound";

            Renderer::DeviceDX12 device(Renderer::DeviceDX12::Mode::UseSoftwareRasterizer);
            Renderer::SceneRendererDX12 renderer(AssetsDir, device);

            RenderAndCompareToReference(renderer, scene, "texture_not_found_dx12");
        }
    };

    TEST_CLASS(RendererSoftware)
    {
        TEST_METHOD(RenderShouldProperlyRenderSimpleScene)
        {
            Renderer::Scene scene;
            Assert::IsTrue(Renderer::Load(CarsDir + "scene.sce", scene));

            Renderer::SceneRendererSoftware renderer;

            RenderAndCompareToReference(renderer, scene, "software");
        }

        TEST_METHOD(RenderShouldProperlyRenderColoredTriangleScene)
        {
            Renderer::Scene scene;
            Assert::IsTrue(Renderer::Load(TriangleDir + "scene.sce", scene));

            Renderer::SceneRendererSoftware renderer;

            RenderAndCompareToReference(renderer, scene, "triangle_software");
        }

        TEST_METHOD(RenderShouldReturnFalseIfTextureHasZeroDimension)
        {
            Renderer::Scene scene;
            Assert::IsTrue(Renderer::Load(TriangleDir + "scene.sce", scene));

            Renderer::SceneRendererSoftware renderer;

            Renderer::Texture textureZeroHeight(200, 0);
            Assert::IsFalse(renderer.Render(scene, textureZeroHeight));

            Renderer::Texture textureZeroWidth(0, 150);
            Assert::IsFalse(renderer.Render(scene, textureZeroWidth));
        }

        TEST_METHOD(RenderShouldPaintMissingTexturesRed)
        {
            Renderer::Scene scene;
            Assert::IsTrue(Renderer::Load(CarsDir + "scene.sce", scene));

            scene.models[0].materials[0].textureName = "notfound";
            scene.models[0].materials[1].textureName = "notfound";

            Renderer::SceneRendererSoftware renderer;

            RenderAndCompareToReference(renderer, scene, "texture_not_found_software");
        }
    };
}
