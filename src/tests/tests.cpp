#include "pch.h"
#include "CppUnitTest.h"

#include <renderer/scene.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace tests
{
	TEST_CLASS(Scene)
	{
	public:
		
		TEST_METHOD(Load)
		{
			Renderer::Scene scene;
			bool success = Renderer::Load("scene.sce", scene);

			Assert::AreEqual(true, success);
			Assert::AreEqual(size_t(2), scene.models.size());
			Assert::AreEqual(size_t(4), scene.models[1].vertices.size());
			Assert::AreEqual(size_t(6), scene.models[1].indices.size());

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
			} == scene.models[1].vertices);

			Assert::IsTrue(std::vector<uint32_t>{0, 1, 2, 2, 3, 0} == scene.models[1].indices);
		}
	};
}
