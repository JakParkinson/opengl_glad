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
Camera camera(glm::vec3(0.0f, 0.0f, 8.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// lighting
glm::vec3 lightPos(1.2f, 1.0f, 2.0f);

static bool wireframe = false;
bool tKeyPressed = false;

// Double pendulum physics variables
float theta1, theta1_dot;
float theta2, theta2_dot;

// System parameters
float m1 = 1.0f, m2 = 1.0f;  // masses
float L1 = 1.5f, L2 = 1.5f;  // lengths
float g = 9.81f;

// Positions for both masses
glm::vec3 pos1, pos2;
glm::vec3 anchorPoint(0.0f, 2.0f, 0.0f);

float sphereRadius = 0.15f;

// Particle trail system
struct TraceParticle {
    glm::vec3 position;
    float life;
    glm::vec3 color; // Add color for red/blue trails
};

const int MAX_TRACE_PARTICLES = 500;
std::vector<TraceParticle> traceParticles1(MAX_TRACE_PARTICLES); // Red trail for first pendulum
std::vector<TraceParticle> traceParticles2(MAX_TRACE_PARTICLES); // Blue trail for second pendulum
float traceLifeTime = 2.0f;
int traceSpawnRate = 60; // particles per second
static float timeSinceLastSpawn = 0.0f;

// Function to generate sphere vertices and indices
void generateSphere(float radius, int latitudeSegments, int longitudeSegments, 
                   std::vector<float>& vertices, std::vector<unsigned int>& indices) {
    vertices.clear();
    indices.clear();
    
    for (int lat = 0; lat <= latitudeSegments; ++lat) {
        float theta = lat * M_PI / latitudeSegments;
        float sinTheta = sin(theta);
        float cosTheta = cos(theta);
        
        for (int lon = 0; lon <= longitudeSegments; ++lon) {
            float phi = lon * 2 * M_PI / longitudeSegments;
            float sinPhi = sin(phi);
            float cosPhi = cos(phi);
            
            float x = radius * sinTheta * cosPhi;
            float y = radius * cosTheta;
            float z = radius * sinTheta * sinPhi;
            
            float nx = sinTheta * cosPhi;
            float ny = cosTheta;
            float nz = sinTheta * sinPhi;
            
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
            vertices.push_back(nx);
            vertices.push_back(ny);
            vertices.push_back(nz);
        }
    }
    
    for (int lat = 0; lat < latitudeSegments; ++lat) {
        for (int lon = 0; lon < longitudeSegments; ++lon) {
            int current = lat * (longitudeSegments + 1) + lon;
            int next = current + longitudeSegments + 1;
            
            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);
            
            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }
}

void generateRopeLine(glm::vec3 start, glm::vec3 end, std::vector<float>& lineVertices) {
    lineVertices.clear();
    lineVertices.push_back(start.x);
    lineVertices.push_back(start.y);
    lineVertices.push_back(start.z);
    lineVertices.push_back(end.x);
    lineVertices.push_back(end.y);
    lineVertices.push_back(end.z);
}

void updatePhysics(float deltaTime) {
    // Cap deltaTime to prevent huge timesteps when framerate drops
    deltaTime = std::min(deltaTime, 0.016f);
    
    // Use small fixed timesteps for numerical stability
    const float dt = 0.001f;
    int substeps = std::max(1, (int)(deltaTime / dt));
    float actual_dt = deltaTime / substeps;
    
    for (int step = 0; step < substeps; step++) {
        // Lagrangian-derived double pendulum equations
        float delta = theta2 - theta1;
        float cos_delta = cos(delta);
        float sin_delta = sin(delta);
        
        // Denominators from Lagrangian derivation
        float den1 = (m1 + m2) * L1 - m2 * L1 * cos_delta * cos_delta;
        float den2 = (L2 / L1) * den1;
        
        // Check for singularities (very rare but possible)
        if (abs(den1) < 1e-12f) {
            continue; // Skip this timestep
        }
        
        // First pendulum angular acceleration (from Lagrangian)
        float num1 = -m2 * L1 * theta1_dot * theta1_dot * sin_delta * cos_delta;
        num1 += m2 * g * sin(theta2) * cos_delta;
        num1 += m2 * L2 * theta2_dot * theta2_dot * sin_delta;
        num1 -= (m1 + m2) * g * sin(theta1);
        
        // Second pendulum angular acceleration (from Lagrangian)
        float num2 = -m2 * L2 * theta2_dot * theta2_dot * sin_delta * cos_delta;
        num2 += (m1 + m2) * g * sin(theta1) * cos_delta;
        num2 += (m1 + m2) * L1 * theta1_dot * theta1_dot * sin_delta;
        num2 -= (m1 + m2) * g * sin(theta2);
        
        float theta1_ddot = num1 / den1;
        float theta2_ddot = num2 / den2;
        
        // Pure Euler integration - no damping, no artificial limits
        theta1_dot += theta1_ddot * actual_dt;
        theta2_dot += theta2_ddot * actual_dt;
        theta1 += theta1_dot * actual_dt;
        theta2 += theta2_dot * actual_dt;
    }
    
    // Update positions
    pos1 = anchorPoint + glm::vec3(L1 * sin(theta1), -L1 * cos(theta1), 0.0f);
    pos2 = pos1 + glm::vec3(L2 * sin(theta2), -L2 * cos(theta2), 0.0f);
}

int main()
{
    std::cout << "Starting Double Pendulum..." << std::endl;
    
    // GLFW initialization
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Double Pendulum", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    // Shaders
    Shader lightingShader("2.2.basic_lighting.vs", "2.2.basic_lighting.fs");
    Shader lightCubeShader("2.2.light_cube.vs", "2.2.light_cube.fs");

    // Generate sphere geometry
    std::vector<float> sphereVertices;
    std::vector<unsigned int> sphereIndices;
    generateSphere(sphereRadius, 15, 15, sphereVertices, sphereIndices);

    // Setup sphere VAO
    unsigned int sphereVBO, sphereVAO, sphereEBO;
    glGenVertexArrays(1, &sphereVAO);
    glGenBuffers(1, &sphereVBO);
    glGenBuffers(1, &sphereEBO);
    
    glBindVertexArray(sphereVAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
    glBufferData(GL_ARRAY_BUFFER, sphereVertices.size() * sizeof(float), 
                 sphereVertices.data(), GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sphereIndices.size() * sizeof(unsigned int), 
                 sphereIndices.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Setup rope VAO
    std::vector<float> ropeVertices;
    unsigned int ropeVAO, ropeVBO;
    glGenVertexArrays(1, &ropeVAO);
    glGenBuffers(1, &ropeVBO);
    
    glBindVertexArray(ropeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, ropeVBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Setup particle trail VAO
    unsigned int traceVAO, traceVBO;
    glGenVertexArrays(1, &traceVAO);
    glGenBuffers(1, &traceVBO);

    glBindVertexArray(traceVAO);
    glBindBuffer(GL_ARRAY_BUFFER, traceVBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_TRACE_PARTICLES * 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Initialize double pendulum
    theta1 = M_PI/3.0f;    // 60 degrees from vertical
    theta2 = M_PI/2.0f;    // 90 degrees from vertical
    theta1_dot = 0.0f;
    theta2_dot = 0.0f;

    // Calculate initial positions
    pos1 = anchorPoint + glm::vec3(L1 * sin(theta1), -L1 * cos(theta1), 0);
    pos2 = pos1 + glm::vec3(L2 * sin(theta2), -L2 * cos(theta2), 0);
    
    std::cout << "Initial setup complete. Starting simulation..." << std::endl;

    // Main render loop
    int frame_counter = 0;
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Update physics
        updatePhysics(deltaTime);

        // Update particle trails
        timeSinceLastSpawn += deltaTime;
        if (timeSinceLastSpawn >= 1.0f / traceSpawnRate) {
            // Spawn red particle for first pendulum
            for (auto& particle : traceParticles1) {
                if (particle.life <= 0.0f) {
                    particle.position = pos1;
                    particle.life = 1.0f;
                    particle.color = glm::vec3(1.0f, 0.0f, 0.0f); // Red
                    break;
                }
            }
            // Spawn blue particle for second pendulum  
            for (auto& particle : traceParticles2) {
                if (particle.life <= 0.0f) {
                    particle.position = pos2;
                    particle.life = 1.0f;
                    particle.color = glm::vec3(0.0f, 0.0f, 1.0f); // Blue
                    break;
                }
            }
            timeSinceLastSpawn = 0.0f;
        }

        // Update particle lifetimes
        for (auto& particle : traceParticles1) {
            if (particle.life > 0.0f) {
                particle.life -= deltaTime / traceLifeTime;
            }
        }
        for (auto& particle : traceParticles2) {
            if (particle.life > 0.0f) {
                particle.life -= deltaTime / traceLifeTime;
            }
        }

        // Debug output every 2 seconds
        if (frame_counter++ % 120 == 0) {
            std::cout << "Angles: theta1=" << theta1*180.0f/M_PI << "° theta2=" << theta2*180.0f/M_PI << "°" << std::endl;
            std::cout << "Pos1: (" << pos1.x << ", " << pos1.y << ") Pos2: (" << pos2.x << ", " << pos2.y << ")" << std::endl;
        }

        // Setup matrices
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), 
                                               (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        
        // Render first pendulum mass (RED)
        lightingShader.use();
        lightingShader.setVec3("objectColor", 1.0f, 0.0f, 0.0f);
        lightingShader.setVec3("lightColor", 1.0f, 1.0f, 1.0f);
        lightingShader.setVec3("lightPos", lightPos);
        lightingShader.setVec3("viewPos", camera.Position);
        lightingShader.setMat4("projection", projection);
        lightingShader.setMat4("view", view);
        
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, pos1);
        lightingShader.setMat4("model", model);
        
        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);
        
        // Render second pendulum mass (BLUE)
        lightingShader.setVec3("objectColor", 0.0f, 0.0f, 1.0f);
        model = glm::mat4(1.0f);
        model = glm::translate(model, pos2);
        lightingShader.setMat4("model", model);
        glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);

        // Render ropes
        lightCubeShader.use();
        lightCubeShader.setMat4("projection", projection);
        lightCubeShader.setMat4("view", view);
        lightCubeShader.setMat4("model", glm::mat4(1.0f));
        lightCubeShader.setVec3("color", 0.6f, 0.3f, 0.1f);
        
        // First rope: anchor to pos1
        generateRopeLine(anchorPoint, pos1, ropeVertices);
        glBindVertexArray(ropeVAO);
        glBindBuffer(GL_ARRAY_BUFFER, ropeVBO);
        glBufferData(GL_ARRAY_BUFFER, ropeVertices.size() * sizeof(float), 
                     ropeVertices.data(), GL_DYNAMIC_DRAW);
        glLineWidth(3.0f);
        glDrawArrays(GL_LINES, 0, 2);
        
        // Second rope: pos1 to pos2
        generateRopeLine(pos1, pos2, ropeVertices);
        glBufferData(GL_ARRAY_BUFFER, ropeVertices.size() * sizeof(float), 
                     ropeVertices.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_LINES, 0, 2);

        // Render particle trails
        lightCubeShader.use();
        lightCubeShader.setMat4("projection", projection);
        lightCubeShader.setMat4("view", view);
        lightCubeShader.setMat4("model", glm::mat4(1.0f));

        // Render red trail particles for first pendulum
        for (auto& particle : traceParticles1) {
            if (particle.life > 0.0f) {
                float intensity = particle.life; // Fade out as life decreases
                lightCubeShader.setVec3("color", intensity, 0.0f, 0.0f); // Red with fading

                glBindVertexArray(traceVAO);
                glBindBuffer(GL_ARRAY_BUFFER, traceVBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, 3 * sizeof(float), &particle.position.x);

                glPointSize(4.0f);
                glDrawArrays(GL_POINTS, 0, 1);
            }
        }

        // Render blue trail particles for second pendulum
        for (auto& particle : traceParticles2) {
            if (particle.life > 0.0f) {
                float intensity = particle.life; // Fade out as life decreases
                lightCubeShader.setVec3("color", 0.0f, 0.0f, intensity); // Blue with fading

                glBindVertexArray(traceVAO);
                glBindBuffer(GL_ARRAY_BUFFER, traceVBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, 3 * sizeof(float), &particle.position.x);

                glPointSize(4.0f);
                glDrawArrays(GL_POINTS, 0, 1);
            }
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &sphereVAO);
    glDeleteVertexArrays(1, &ropeVAO);
    glDeleteVertexArrays(1, &traceVAO);
    glDeleteBuffers(1, &sphereVBO);
    glDeleteBuffers(1, &sphereEBO);
    glDeleteBuffers(1, &ropeVBO);
    glDeleteBuffers(1, &traceVBO);

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

    if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS && !tKeyPressed)
    {
        wireframe = !wireframe;
        if (wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
        tKeyPressed = true;
    }
    else if (glfwGetKey(window, GLFW_KEY_T) == GLFW_RELEASE)
    {
        tKeyPressed = false;
    }
    
    // Reset pendulum with R key
    static bool rKeyPressed = false;
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS && !rKeyPressed)
    {
        // Reset to initial conditions
        theta1 = M_PI/3.0f;    // 60 degrees
        theta2 = M_PI/2.0f;    // 90 degrees
        theta1_dot = 0.0f;
        theta2_dot = 0.0f;
        
        // Clear particle trails
        for (auto& particle : traceParticles1) {
            particle.life = 0.0f;
        }
        for (auto& particle : traceParticles2) {
            particle.life = 0.0f;
        }
        
        std::cout << "Reset pendulum!" << std::endl;
        rKeyPressed = true;
    }
    else if (glfwGetKey(window, GLFW_KEY_R) == GLFW_RELEASE)
    {
        rKeyPressed = false;
    }
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