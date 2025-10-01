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
Camera camera(glm::vec3(0.0f, 7.0f, 40.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

struct TraceParticle {
    glm::vec3 position;
    float life;
};

const int MAX_TRACE_PARTICLES = 50000;
std::vector<TraceParticle> traceParticles(MAX_TRACE_PARTICLES);
float traceLifeTime = 15.9f;
int traceSpawnRate = 500;
static float timeSinceLastSpawn = 0.0f;

using Vec = glm::vec3; // this is really jsut for RK stuff
template<class F>
Vec rk4(F&& f, const Vec& y, float dt) {
    Vec k1 = f(y);
    Vec k2 = f(y + 0.5f*dt*k1);
    Vec k3 = f(y + 0.5f*dt*k2);
    Vec k4 = f(y + dt*k3);
    return y + (dt/6.0f)*(k1+2.0f*k2 + 2.0f*k3 + k4);
}

int main()

{
    // glfw: initialize and configure
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);


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

    glEnable(GL_DEPTH_TEST);
    //glEnable(GL_PROGRAM_POINT_SIZE);  // Allow setting point size in shader

    // build and compile shaders
    Shader particleShader("particle.vs", "particle.fs");

    // Generate the particle positions for our function
    //std::vector<float> particlePositions;
    
    // Domain: x from -10 to 10, y from -10 to 10
    // Samples: 100x100 = 10,000 particles
    //generatePlotParticles(particlePositions, -10.0f, 10.0f, -10.0f, 10.0f, 300, 300);
    
  //  std::cout << "Generated " << particlePositions.size() / 3 << " particles" << std::endl;

        // Setup particle VAO and VBO
    unsigned int particleVBO, particleVAO;
    glGenVertexArrays(1, &particleVAO);
    glGenBuffers(1, &particleVBO);

    glBindVertexArray(particleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_TRACE_PARTICLES * 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);



   // float curr_time = 0;



    // Lorenz stuff:
    const float sigma = 10.0f, rho = 28.0f, beta = 8.0f/3.0f;
    


    // [=] capture
    auto lorenz = [=](const Vec& r) -> Vec {
        return Vec {
            sigma * (r.y - r.x),
            r.x *   (rho - r.z) - r.y,
            r.x * r.y - beta*r.z
        };
    };

    Vec r_old = Vec(-8.0f, 8.0f, 27.0f);


    float dt = 1e-3f; // time step for numerical integration
    float simSpeed = 0.5f; // time scale
    float acc = 0.0f; // leftover time from previous frame
    const int maxSubsteps = 100000;

    while (!glfwWindowShouldClose(window))
    {
        
                // per-frame time logic
        // --------------------
        float now = (float)glfwGetTime();
        deltaTime = now - lastFrame;    // keep the real frame delta
        lastFrame = now;

        // frameCount++;
        // fpsTimer += deltaTime;
        // deltaTime = 0.01f;
        // curr_time += deltaTime;

        processInput(window);


        // render
        // ------
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        acc += deltaTime * simSpeed;

        int stepsThisFrame = 0;
        while (acc >= dt && stepsThisFrame < maxSubsteps) {
            r_old = rk4(lorenz, r_old, dt);
            acc  -= dt;
            ++stepsThisFrame;
        }


        // x_new = 4*sin(curr_time*5);
        // y_new = 4*cos(curr_time*5);
        // z_new = z_old - 0.01;
        // x_new,y_new,z_new = r_new.x,r_new.y,r_new.z;

        timeSinceLastSpawn += deltaTime;
        if (timeSinceLastSpawn >= 1.0f/traceSpawnRate) {
            for (auto& particle : traceParticles) {
                if (particle.life <= 0.0f) {
                    particle.position = r_old;
                    particle.life = 1.0f;
                    break;
                }
            }
            timeSinceLastSpawn = 0.0f;
        }

//        std::vector<float> tracePositions;
        for (auto& particle : traceParticles) {
            if (particle.life > 0.0f) {
                particle.life -= deltaTime/traceLifeTime;

                if (particle.life < 0.0f) particle.life = 0.0f;
            }
        }

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), 
                                                    (float)SCR_WIDTH / (float)SCR_HEIGHT, 
                                                    0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();

        particleShader.use();
        particleShader.setMat4("projection", projection);
        particleShader.setMat4("view", view);
        particleShader.setMat4("model", glm::mat4(1.0f));

        for (auto& particle : traceParticles) {
            if (particle.life > 0.0f) {
                float intensity = particle.life; // 1.0 when fresh, 0.0 when dying    

  
                particleShader.setVec3("color", 0.0f, intensity, 0.0f);

   

                glBindVertexArray(particleVAO);
                glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, 3 * sizeof(float), &particle.position.x);

                glPointSize(4.0f);
                glDrawArrays(GL_POINTS, 0, 1);

            }
        }



        glfwSwapBuffers(window);
        glfwPollEvents();
    }

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