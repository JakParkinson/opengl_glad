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

#include <array>
#include <vector>

std::vector<float> linspace(float start, float end, int n) {
    std::vector<float> result(n);
    for (int i = 0; i < n; ++i) {
        result[i] = start + (end - start)*i/(n-1.0f);
    }
    return result;
}


void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);

// settings
const unsigned int SCR_WIDTH = 1400;
const unsigned int SCR_HEIGHT = 800;

// camera
Camera camera(glm::vec3(6.0f, 6.0f, 24.0f));
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

void updateParticlesFromWave(std::vector<float>& positions,
                             const std::vector<std::vector<float>>& wave_slice,
                             const std::vector<float>& x,
                             const std::vector<float>& y) {
    positions.clear();

    for (int i=0;i<x.size();++i) {
        for (int k=0;k<y.size();++k){
            positions.push_back(x[i]);
            positions.push_back(wave_slice[i][k]);
            positions.push_back(y[k]);
        }
    }

}

void updateLinesFromWave(std::vector<float>& linePositions,
                        const std::vector<std::vector<float>>& wave_slice,
                        const std::vector<float>& x,
                        const std::vector<float>& y) {
    linePositions.clear();
    
    int nx = x.size();
    int ny = y.size();
    
    // Horizontal lines (connect each point to its right neighbor)
    for (int i = 0; i < nx-1; ++i) {
        for (int k = 0; k < ny; ++k) {
            // Start vertex
            linePositions.push_back(x[i]);
            linePositions.push_back(wave_slice[i][k]);
            linePositions.push_back(y[k]);
            
            // End vertex (right neighbor)
            linePositions.push_back(x[i+1]);
            linePositions.push_back(wave_slice[i+1][k]);
            linePositions.push_back(y[k]);
        }
    }
    
    // Vertical lines (connect each point to its bottom neighbor)
    for (int i = 0; i < nx; ++i) {
        for (int k = 0; k < ny-1; ++k) {
            // Start vertex
            linePositions.push_back(x[i]);
            linePositions.push_back(wave_slice[i][k]);
            linePositions.push_back(y[k]);
            
            // End vertex (bottom neighbor)
            linePositions.push_back(x[i]);
            linePositions.push_back(wave_slice[i][k+1]);
            linePositions.push_back(y[k+1]);
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
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_PROGRAM_POINT_SIZE);  // Allow setting point size in shader

    // build and compile shaders
    Shader particleShader("particle.vs", "particle.fs");



/// WAVE SETUP
    // physics paramters
    float L = 15.0f;
    float cx = 1.5f;
    float cy = 0.0001f;

    // IC: 
    float sigma = 0.9f;
    float amplitude = 5.0f;
    float t_final = 10.0f;

    /// nx,ny stuff:
    int nx = 200;
    int ny = 200;
    float dx = L/ static_cast<float>(nx - 1);
    float dy = L/ static_cast<float>(ny - 1);
    float dt = std::min(dx/cx, dy/cy);
    int nt = static_cast<int>(t_final / dt) + 1;

    std::vector<float> x = linspace(0.0f, L, nx);
    std::vector<float> y = linspace(0.0f, L, ny);
    std::vector<float> t = linspace(0.0f, t_final, nt);

    
    std::vector<std::vector<float>> X(nx, std::vector<float>(ny));
    std::vector<std::vector<float>> Y(nx, std::vector<float>(ny));


    /// pause for VAOs stuff

    // Generate the particle positions for our function
    std::vector<float> particlePositions;
    


    //Meshgrid  : 2v vectros


    // filling:
   for (int i = 0; i < nx; ++i) {
        for (int k = 0; k < ny; ++k) {
            X[i][k] = x[i];
            Y[i][k] = y[k];
        }
    }


    std::vector<std::vector<float>> u_current(nx, std::vector<float>(ny, 0.0f));
    std::vector<std::vector<float>> u_next(nx, std::vector<float>(ny, 0.0f));


    // iniial condition: Gaussian
    for (int i = 0; i < nx; ++i) {
        for(int k = 0; k < ny; ++k) {
            float dx_term = (X[i][k] - L/4.0f) * (X[i][k] - L/4.0f);
            float dy_term = (Y[i][k] - L/2.0f) * (Y[i][k] - L/2.0f);
            u_current[i][k] = amplitude*std::exp(-(dx_term + dy_term)/(2.0f*sigma*sigma));
        }
    }

    // inital periodic BC conditions of 0s:
    for (int i=0; i<nx; ++i){
        u_current[i][0] = u_current[i][ny-2];
        u_current[i][ny-1] = u_current[i][1];
    }
    for (int k=0; k<ny; ++k){
        u_current[0][k] =  u_current[nx-2][k];
        u_current[nx-1][k] = u_current[1][k];
    }







    int stepsPerFrame = 1;
    int frameSkip = 1;  // Only update physics every 5th frame
    int frameCounter = 0;

/// END OF SETUP


    updateParticlesFromWave(particlePositions, u_current, x, y);
    std::cout << "Generated " << particlePositions.size() / 3 << " particles" << std::endl;

    // Setup particle VAO and VBO
    unsigned int particleVBO, particleVAO;
    glGenVertexArrays(1, &particleVAO);
    glGenBuffers(1, &particleVBO);

    glBindVertexArray(particleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
    glBufferData(GL_ARRAY_BUFFER, particlePositions.size() * sizeof(float), 
             particlePositions.data(), GL_DYNAMIC_DRAW);  // use .data(), not nullptr

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);



    std::vector<float> linePositions;

    unsigned int lineVBO, lineVAO;
    glGenVertexArrays(1, &lineVAO);
    glGenBuffers(1, &lineVBO);

    glBindVertexArray(lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);

    // Initialize the line positions from initial wave state
    updateLinesFromWave(linePositions, u_current, x, y);

    glBufferData(GL_ARRAY_BUFFER, linePositions.size() * sizeof(float), 
                linePositions.data(), GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    std::cout << "Generated " << linePositions.size() / 6 << " line segments" << std::endl;

    std::vector<std::vector<float>> k1(nx, std::vector<float>(ny,0.0f));
    std::vector<std::vector<float>> k2(nx, std::vector<float>(ny,0.0f));
    std::vector<std::vector<float>> k3(nx, std::vector<float>(ny,0.0f));
    std::vector<std::vector<float>> k4(nx, std::vector<float>(ny,0.0f));

    /// end VAO set up
    // render loop
    while (!glfwWindowShouldClose(window))
    {
        // per-frame time logic
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        frameCounter++;

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


        //main loop

    // MAIN LOOP

    // (j = time, i = x, k = y):

    std::vector<std::vector<float>> u_temp(nx, std::vector<float>(ny, 0.0f));
    if (frameCounter % frameSkip == 0) {
   
        for (int step =0; step < stepsPerFrame; step++) {

                //k1:
                for (int i=1; i < nx-1; ++i) {
                        for (int k=1; k<ny-1; ++k) {
                            k1[i][k] = -cx*(u_current[i][k] - u_current[i-1][k])/dx - cy*(u_current[i][k] - u_current[i][k-1])/dy;
                        }
                }

                //k2:
                for (int i=0;i<nx;++i){
                    for (int k=0;k<ny;++k){
                        u_temp[i][k]=u_current[i][k] + (dt/2.0f)*k1[i][k];
                    }
                }
                // BC (periodic):
                for (int i =0; i < nx; ++i) {
                    u_temp[i][0] = u_temp[i][ny-2];
                    u_temp[i][ny-1] = u_temp[i][1];
                }
                for (int k =0; k < ny; ++k) {
                    u_temp[0][k] = u_temp[nx-2][k];
                    u_temp[nx-1][k] = u_temp[1][k];
                }
                for (int i = 1; i<nx-1; ++i){
                    for (int k=1; k<ny-1; ++k){
                        k2[i][k] = -cx*(u_temp[i][k] - u_temp[i-1][k])/dx - cy*(u_temp[i][k] - u_temp[i][k-1])/dy;
                    }
                }

                //k3:
                for (int i=0;i<nx;++i){
                    for (int k=0;k<ny;++k){
                        u_temp[i][k]=u_current[i][k] + (dt/2.0f)*k2[i][k];
                    }
                }
                // BC (periodic):
                for (int i =0; i < nx; ++i) {
                    u_temp[i][0] = u_temp[i][ny-2];
                    u_temp[i][ny-1] = u_temp[i][1];
                }
                for (int k =0; k < ny; ++k) {
                    u_temp[0][k] = u_temp[nx-2][k];
                    u_temp[nx-1][k] = u_temp[1][k];
                }
                for (int i = 1; i<nx-1; ++i){
                    for (int k=1; k<ny-1; ++k){
                        k3[i][k] = -cx*(u_temp[i][k] - u_temp[i-1][k])/dx - cy*(u_temp[i][k] - u_temp[i][k-1])/dy;
                    }
                }

                //k4:
                for (int i=0;i<nx;++i){
                    for (int k=0;k<ny;++k){
                        u_temp[i][k]=u_current[i][k] + dt*k3[i][k];
                    }
                }
                // BC (periodic):
                for (int i =0; i < nx; ++i) {
                    u_temp[i][0] = u_temp[i][ny-2];
                    u_temp[i][ny-1] = u_temp[i][1];
                }
                for (int k =0; k < ny; ++k) {
                    u_temp[0][k] = u_temp[nx-2][k];
                    u_temp[nx-1][k] = u_temp[1][k];
                }
                for (int i = 1; i<nx-1; ++i){
                    for (int k=1; k<ny-1; ++k){
                        k4[i][k] = -cx*(u_temp[i][k] - u_temp[i-1][k])/dx - cy*(u_temp[i][k] - u_temp[i][k-1])/dy;
                    }
                }


                for (int i=0;i<nx;++i){
                    for (int k=0;k<ny;++k){
                        u_next[i][k] = u_current[i][k] + (dt/6.0f)*(k1[i][k]+2.0f*k2[i][k]+2.0f*k3[i][k]+k4[i][k]);
                    }
                }

                for (int i = 0; i < nx; ++i) {
                    u_next[i][0] = u_next[i][ny-2];
                    u_next[i][ny-1] = u_next[i][1];
                }
                for (int k = 0; k < ny; ++k) {
                    u_next[0][k] = u_next[nx-2][k];
                    u_next[nx-1][k] = u_next[1][k];
                }
                std::swap(u_current, u_next);

        }

    }





        //

        // Render particles
        particleShader.use();
        particleShader.setMat4("projection", projection);
        particleShader.setMat4("view", view);

        // DATA
        updateParticlesFromWave(particlePositions, u_current, x, y);

        particleShader.setVec4("color", 0.0f, 1.0f, 0.0f, 1.0f);  // green for particles

        glBindVertexArray(particleVAO);
        glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, particlePositions.size() * sizeof(float), 
                particlePositions.data());
        glDrawArrays(GL_POINTS, 0, particlePositions.size() / 3);

        // LINES:

        updateLinesFromWave(linePositions, u_current, x, y);

        particleShader.setVec4("color", 1.0f, 1.0f, 1.0f, 0.2f);  // white for lines

        glLineWidth(0.00001f);

        glBindVertexArray(lineVAO);
        glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, linePositions.size() * sizeof(float),
                        linePositions.data());
        glDrawArrays(GL_LINES, 0, linePositions.size() / 3);




//        glPointSize(5.0f);

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