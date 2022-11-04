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
		}
	};
}
