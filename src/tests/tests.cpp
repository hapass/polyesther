#include "pch.h"
#include "CppUnitTest.h"

#include <renderer/scene.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace tests
{
	TEST_CLASS(Scene)
	{
	public:
		
		TEST_METHOD(LoadShouldSucceedWhenThereIsSceneFile)
		{
			Renderer::Scene scene;
			bool success = Renderer::Load("scene.sce", scene);

			// everything is loaded
			Assert::AreEqual(true, success);

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
	};
}
