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
Camera camera(glm::vec3(-3.5f, 8.0f, 45.0f));
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


glm::vec3 E_field(glm::vec3 pos,
                  std::vector<glm::vec3>& charge_positions,
                  std::vector<float>& charges) {
                    
    glm::vec3 E(0.0f, 0.0f, 0.0f);

    for (int i=0;i<charges.size(); i++) {
        glm::vec3 r = pos - charge_positions[i];
        float dist = glm::length(r);

        if (dist > 0.1f) {
            E += charges[i] * r/(dist*dist*dist);
        }
    }
    return E;
}


void traceFieldLine(std::vector<float>& lineVerts,
                    glm::vec3 startPos,
                    std::vector<glm::vec3>& charge_positions,
                    std::vector<float>& charge_values,
                    float stepSize = 0.1f,
                    int maxSteps = 500) {
    
    glm::vec3 pos = startPos;

    for (int step=0; step<maxSteps; step++)
    {
        glm::vec3 E = E_field(pos, charge_positions, charge_values);

        float E_mag = glm::length(E);
        if (E_mag < 0.001f) break;

        // positions to lines:
        lineVerts.push_back(pos.x);
        lineVerts.push_back(pos.y); //2D
        lineVerts.push_back(pos.z);

        glm::vec3 direction = E/E_mag;
        pos += direction*stepSize;

        //check bounds:
        if (pos.x < 0 || pos.x > 50.0f || pos.z < 0 || pos.z>50.0f) break;

    }

}


void addArrowsToLine(std::vector<float>& arrowVerts,
                     const std::vector<float>& lineVerts,
                     std::vector<glm::vec3>& charge_positions,
                     std::vector<float>& charges,
                     int arrowFrequency = 20,  // Arrow every N points
                     float arrowSize = 0.5f) {
    
    int numPoints = lineVerts.size() / 3;
    
    for (int i = arrowFrequency; i < numPoints; i += arrowFrequency) {
        // Get point position
        glm::vec3 pos(lineVerts[i*3], lineVerts[i*3 + 1], lineVerts[i*3 + 2]);
        
        // Get field direction at this point
        glm::vec3 E = E_field(pos, charge_positions, charges);
        if (glm::length(E) < 0.001f) continue;
        
        glm::vec3 direction = glm::normalize(E);
        
        // Create arrow head: two lines at 135° and -135° (pointing backwards)
        float angle1 = 2.356f;  // 135 degrees in radians
        float angle2 = -2.356f;
        
        // Rotate direction in x-z plane (since y=0)
        glm::vec3 arrow1(
            direction.x * cos(angle1) - direction.z * sin(angle1),
            0.0f,
            direction.x * sin(angle1) + direction.z * cos(angle1)
        );
        
        glm::vec3 arrow2(
            direction.x * cos(angle2) - direction.z * sin(angle2),
            0.0f,
            direction.x * sin(angle2) + direction.z * cos(angle2)
        );
        
        arrow1 = glm::normalize(arrow1) * arrowSize;
        arrow2 = glm::normalize(arrow2) * arrowSize;
        
        // First arrow line
        arrowVerts.push_back(pos.x);
        arrowVerts.push_back(pos.y);
        arrowVerts.push_back(pos.z);
        arrowVerts.push_back(pos.x + arrow1.x);
        arrowVerts.push_back(pos.y + arrow1.y);
        arrowVerts.push_back(pos.z + arrow1.z);
        
        // Second arrow line
        arrowVerts.push_back(pos.x);
        arrowVerts.push_back(pos.y);
        arrowVerts.push_back(pos.z);
        arrowVerts.push_back(pos.x + arrow2.x);
        arrowVerts.push_back(pos.y + arrow2.y);
        arrowVerts.push_back(pos.z + arrow2.z);
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


    /// pause for VAOs stuff

    // Generate the particle positions for our function
    std::vector<float> particlePositions;



std::vector<glm::vec3> charge_positions = {
    glm::vec3(20.0f, 0.0f, 25.0f),
    glm::vec3(30.0f, 0.0f, 25.0f)
};
std::vector<float> charges = {1.0f, -1.0f}; glm::vec3 testPos(10.0f, 0.0f, 15.0f);
    // glm::vec3 E = E_field(testPos, charge_positions, charges);
    
    // Domain: x from -10 to 10, y from -10 to 10
    // Samples: 100x100 = 10,000 particles
  //  generatePlotParticles(particlePositions, 0, L, 0, L, nx, ny); ///not 100% on L domain
    



    //Meshgrid  : 2v vectros


    // first iteration



    int stepsPerFrame = 5;
    int frameSkip = 1;  // Only update physics every frame lol
    int frameCounter = 0;

/// END OF SETUP


    // updateParticlesFromWave(particlePositions, u_current, x, y);
    // std::cout << "Generated " << particlePositions.size() / 3 << " particles" << std::endl;

    // Setup particle VAO and VBO


for (auto& charge_pos : charge_positions) {
    particlePositions.push_back(charge_pos.x);
    particlePositions.push_back(charge_pos.y);
    particlePositions.push_back(charge_pos.z);
}


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
   // updateLinesFromWave(linePositions, u_current, x, y);


    std::vector<std::vector<float>> allFieldLines;  // Vector of individual lines
    std::vector<float> allArrows;  // store all arrows

    // generate field lines around each charge
    for (size_t chargeIdx = 0; chargeIdx < charge_positions.size(); chargeIdx++) {
        glm::vec3 chargePos = charge_positions[chargeIdx];
        float chargeVal = charges[chargeIdx];

        int numLines = 64; // lines per charge
        float radius = 1.5f; // starting radius from charge

        for (int i=0; i<numLines; i++) {
            float angle = 2.0f * 3.14159f*i/numLines;

            // start positions in circle around charge:
            glm::vec3 startPos (
                chargePos.x + radius*cos(angle),
                chargePos.y,
                chargePos.z + radius*sin(angle)
            );

            std::vector<float> line;
            traceFieldLine(line, startPos, charge_positions, charges, 0.05f, 1000);

            if(!line.empty()) {
                allFieldLines.push_back(line);
                addArrowsToLine(allArrows, line, charge_positions, charges, 40, 0.1f);
            }
        }
    }

    // // trace field lines:
    // for (int i = 0; i < 64; i++) {
    //     float angle = 2.0f*3.14159f*i/64.0f;
    //     float radius = 1.5f;

    //     // Each line gets its own vector
    //     std::vector<float> line1;
    //     glm::vec3 start1(10.0f + radius*cos(angle), 0.0f, 15.0f+radius*sin(angle));
    //     traceFieldLine(line1, start1, charge_positions, charges, 0.05f, 1000);
    //     if (!line1.empty()) {
    //         allFieldLines.push_back(line1);
    //         addArrowsToLine(allArrows, line1, charge_positions, charges, 20, 0.1f);  // Add arrows
    //     }   

    //     std::vector<float> line2;
    //     glm::vec3 start2(20.0f + radius*cos(angle), 0.0f, 15.0f+radius*sin(angle));
    //     traceFieldLine(line2, start2, charge_positions, charges, 0.05f, 1000);
    //     if (!line2.empty()) {
    //         allFieldLines.push_back(line2);
    //         addArrowsToLine(allArrows, line2, charge_positions, charges, 20, 0.1f);
    //     }
    // }

   std::cout << "Generated " << allFieldLines.size() << " field lines from " 
          << charge_positions.size() << " charges" << std::endl;


    glBufferData(GL_ARRAY_BUFFER, linePositions.size() * sizeof(float), 
                linePositions.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);


    unsigned int arrowVBO, arrowVAO;
    glGenVertexArrays(1, &arrowVAO);
    glGenBuffers(1, &arrowVBO);

    glBindVertexArray(arrowVAO);
    glBindBuffer(GL_ARRAY_BUFFER, arrowVBO);
    glBufferData(GL_ARRAY_BUFFER, allArrows.size() * sizeof(float), 
                allArrows.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);




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





        //

        // Render particles
        particleShader.use();
        particleShader.setMat4("projection", projection);
        particleShader.setMat4("view", view);

        particleShader.setVec4("color", 1.0f, 0.0f, 0.0f, 0.8f);
        glBindVertexArray(particleVAO);
        glDrawArrays(GL_POINTS, 0, particlePositions.size() / 3);


        glLineWidth(1.0f);  // Add before drawing lines
        particleShader.setVec4("color", 0.8f, 0.8f, 1.0f, 0.4f);  // Cyan color, more visible

        for (const auto& line : allFieldLines) {
            glBindVertexArray(lineVAO);
            glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
            glBufferData(GL_ARRAY_BUFFER, line.size() * sizeof(float), 
                        line.data(), GL_STATIC_DRAW);
            glDrawArrays(GL_LINE_STRIP, 0, line.size() / 3);  // NOW strip is correct!
        }
//        glPointSize(5.0f);


            // After drawing field lines:
            particleShader.setVec4("color", 1.0f, 1.0f, 0.0f, 0.3f);  // Yellow arrows
            glBindVertexArray(arrowVAO);
            glDrawArrays(GL_LINES, 0, allArrows.size() / 3);

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