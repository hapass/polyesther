#define GLFW_INCLUDE_GLCOREARB
#define GL_SILENCE_DEPRECATION

#include <GLFW/glfw3.h>

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

int main(int argc, char** argv)
{
  if (!glfwInit())
  {
     return -1;
  }

  glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint (GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow* window = glfwCreateWindow(1280, 720, "Software rasterizer", NULL, NULL);
  if (!window)
  {
     glfwTerminate();
     return -1;
  }

  glfwMakeContextCurrent(window);

  while (!glfwWindowShouldClose(window))
  {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwTerminate();
  return 0;
}