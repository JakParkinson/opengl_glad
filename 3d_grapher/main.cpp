#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader_m.h"
#include "camera.h"

#include <iostream>
#include <vector>
#include <cmath>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);

// settings
const unsigned int SCR_WIDTH = 1600;
const unsigned int SCR_HEIGHT = 1200;

// camera
Camera camera(glm::vec3(0.0f, 7.0f, 22.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Function to plot: z = f(x, y)
float functionToPlot(float x, float y) {
    // Try different functions!
    
    // Ripple effect (water drop)
    float r = sqrt(x*x + y*y);
    if (r < 0.01f) return 5.0f;
    return 5.0f * sin(r) / r;
    
    // Other cool options to try:
 //    return sin(x) * cos(y);  // Peaks and valleys
    // return x*x - y*y;  // Saddle
    // return exp(-(x*x + y*y));  // Gaussian bump
     return sin(sqrt(x*x + y*y));  // Circular ripples
}

// Generate particle positions for the 3D plot
void generatePlotParticles(std::vector<float>& positions, 
                          float xMin, float xMax, 
                          float yMin, float yMax,
                          int xSamples, int ySamples) {
    positions.clear();
    
    float xStep = (xMax - xMin) / (xSamples - 1);
    float yStep = (yMax - yMin) / (ySamples - 1);
    
    for (int i = 0; i < xSamples; ++i) {
        for (int j = 0; j < ySamples; ++j) {
            float x = xMin + i * xStep;
            float y = yMin + j * yStep;
            float z = functionToPlot(x, y);
            
            positions.push_back(x);
            positions.push_back(z);  // z is the "height"
            positions.push_back(y);
        }
    }
}

int main()
{
    // glfw: initialize and configure
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "3D Particle Function Plotter", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // configure global opengl state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);  // Allow setting point size in shader

    // build and compile shaders
    Shader particleShader("particle.vs", "particle.fs");

    // Generate the particle positions for our function
    std::vector<float> particlePositions;
    
    // Domain: x from -10 to 10, y from -10 to 10
    // Samples: 100x100 = 10,000 particles
    generatePlotParticles(particlePositions, -10.0f, 10.0f, -10.0f, 10.0f, 300, 300);
    
    std::cout << "Generated " << particlePositions.size() / 3 << " particles" << std::endl;

    // Setup particle VAO and VBO
    unsigned int particleVBO, particleVAO;
    glGenVertexArrays(1, &particleVAO);
    glGenBuffers(1, &particleVBO);

    glBindVertexArray(particleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
    glBufferData(GL_ARRAY_BUFFER, particlePositions.size() * sizeof(float), 
                 particlePositions.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // render loop
    while (!glfwWindowShouldClose(window))
    {
        // per-frame time logic
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        processInput(window);

        // render
        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Setup camera matrices
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), 
                                                (float)SCR_WIDTH / (float)SCR_HEIGHT, 
                                                0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();

        // Render particles
        particleShader.use();
        particleShader.setMat4("projection", projection);
        particleShader.setMat4("view", view);

        glBindVertexArray(particleVAO);
        glDrawArrays(GL_POINTS, 0, particlePositions.size() / 3);

        // glfw: swap buffers and poll IO events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // cleanup
    glDeleteVertexArrays(1, &particleVAO);
    glDeleteBuffers(1, &particleVBO);

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);
    
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}