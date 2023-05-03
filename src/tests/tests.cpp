#include "pch.h"
#include "CppUnitTest.h"

#include <renderer/scene.h>
#include <renderer/utils.h>
#include <renderer/rendererdx12.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Tests
{
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
			bool success = Renderer::Load("C:\\Users\\hapas\\Documents\\Code\\software_rasterizer\\assets\\tests\\scene.sce", scene);

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
					Renderer::Color()
				},
				Renderer::Vertex
				{
					0,
					Renderer::Vec {-0.5, 0.5, 0.0, 1.0},
					Renderer::Vec {0.0, 0.0, 1.0, 0.0},
					Renderer::Vec {0.0, 1.0, 0.0, 0.0},
					Renderer::Color()
				},
				Renderer::Vertex
				{
					0,
					Renderer::Vec {0.5, 0.5, 0.0, 1.0},
					Renderer::Vec {0.0, 0.0, 1.0, 0.0},
					Renderer::Vec {1.0, 1.0, 0.0, 0.0},
					Renderer::Color()
				},
				Renderer::Vertex
				{
					0,
					Renderer::Vec {0.5, -0.5, 0.0, 1.0},
					Renderer::Vec {0.0, 0.0, 1.0, 0.0},
					Renderer::Vec {1.0, 0.0, 0.0, 0.0},
					Renderer::Color()
				}
			} == firstModel.vertices);

			Assert::IsTrue(std::vector<uint32_t>{0, 1, 2, 2, 3, 0} == firstModel.indices);

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
					Renderer::Color()
				},
				Renderer::Vertex
				{
					0,
					Renderer::Vec {-0.5, 0.5, 0.0, 1.0},
					Renderer::Vec {0.0, 0.0, 1.0, 0.0},
					Renderer::Vec {0.0, 1.0, 0.0, 0.0},
					Renderer::Color()
				},
				Renderer::Vertex
				{
					0,
					Renderer::Vec {0.5, 0.5, 0.0, 1.0},
					Renderer::Vec {0.0, 0.0, 1.0, 0.0},
					Renderer::Vec {1.0, 1.0, 0.0, 0.0},
					Renderer::Color()
				},
				Renderer::Vertex
				{
					1,
					Renderer::Vec {0.5, 0.5, 0.0, 1.0},
					Renderer::Vec {0.0, 0.0, 1.0, 0.0},
					Renderer::Vec {1.0, 1.0, 0.0, 0.0},
					Renderer::Color()
				},
				Renderer::Vertex
				{
					1,
					Renderer::Vec {0.5, -0.5, 0.0, 1.0},
					Renderer::Vec {0.0, 0.0, 1.0, 0.0},
					Renderer::Vec {1.0, 0.0, 0.0, 0.0},
					Renderer::Color()
				},
				Renderer::Vertex
				{
					1,
					Renderer::Vec {-0.5, -0.5, 0.0, 1.0},
					Renderer::Vec {0.0, 0.0, 1.0, 0.0},
					Renderer::Vec {0.0, 0.0, 0.0, 0.0},
					Renderer::Color()
				}
			} == secondModel.vertices);

			Assert::IsTrue(std::vector<uint32_t>{0, 1, 2, 3, 4, 5} == secondModel.indices);
		}

		TEST_METHOD(LoadShouldFailWhenThereIsNoSceneFile)
		{
			Renderer::Scene scene;
			bool success = Renderer::Load("", scene);

			Assert::IsFalse(success);
		}

		TEST_METHOD(DX12RenderShouldProvideExactImage)
		{
			Renderer::Scene scene;
			bool success = Renderer::Load("C:\\Users\\hapas\\Documents\\Code\\software_rasterizer\\assets\\scene.sce", scene);

			scene.camera.position.z = 2;
			scene.camera.position.x = 0;
			scene.camera.position.y = 0;
			scene.camera.pitch = 0.0f;
			scene.camera.yaw = 0.0f;
			scene.camera.forward = Renderer::Vec{ 0.0f, 0.0f, -1.0f, 0.0f };
			scene.camera.left = Renderer::Vec{ -1.0f, 0.0f, 0.0f, 0.0f };
			scene.light.position = Renderer::Vec{ 100.0f, 100.0f, 100.0f, 1.0f };

			Renderer::RendererDX12 renderer("C:\\Users\\hapas\\Documents\\Code\\software_rasterizer\\assets\\color.hlsl");

			Renderer::Texture texture(40, 30); // test error if texture has no dimensions and passed to render
			renderer.Render(scene, texture);

			Renderer::Texture reference;
			Renderer::Load("C:\\Users\\hapas\\Documents\\Code\\software_rasterizer\\assets\\tests\\reference.jpg", reference);

			Assert::IsTrue(texture == reference);
		}
	};
}
