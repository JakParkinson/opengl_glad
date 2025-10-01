#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader_m.h"
#include "camera.h"

#include <iostream>
#include <vector>  // ADD THIS
#include <cmath>   // ADD THIS

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
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

//global projection and view:
glm::mat4 globalProjection;
glm::mat4 globalView;

glm::vec3 globalSpherePos;
float sphereRadius = 0.3f;

float theta_old;
float theta_dot_old;
float rope_L = 3.0f;
glm::vec3 anchorPoint(0.0f, 2.0f, 0.0f);

bool isDragging = false;


struct TraceParticle {
    glm::vec3 position;
    float life;
};

const int MAX_TRACE_PARTICLES = 1000;
std::vector<TraceParticle> traceParticles(MAX_TRACE_PARTICLES);
float traceLifeTime = 0.9f;
int traceSpawnRate = 100;
static float timeSinceLastSpawn = 0.0f;

// Function to generate sphere vertices and indices
void generateSphere(float radius, int latitudeSegments, int longitudeSegments, 
                   std::vector<float>& vertices, std::vector<unsigned int>& indices) {
    vertices.clear();
    indices.clear();
    
    // Generate vertices
    for (int lat = 0; lat <= latitudeSegments; ++lat) {
        float theta = lat * M_PI / latitudeSegments; // 0 to PI
        float sinTheta = sin(theta);
        float cosTheta = cos(theta);
        
        for (int lon = 0; lon <= longitudeSegments; ++lon) {
            float phi = lon * 2 * M_PI / longitudeSegments; // 0 to 2*PI
            float sinPhi = sin(phi);
            float cosPhi = cos(phi);
            
            // Calculate position
            float x = radius * sinTheta * cosPhi;
            float y = radius * cosTheta;
            float z = radius * sinTheta * sinPhi;
            
            // Calculate normal (for a sphere, normal = normalized position)
            float nx = sinTheta * cosPhi;
            float ny = cosTheta;
            float nz = sinTheta * sinPhi;
            
            // Add vertex (position + normal)
            vertices.push_back(x);  // position x
            vertices.push_back(y);  // position y
            vertices.push_back(z);  // position z
            vertices.push_back(nx); // normal x
            vertices.push_back(ny); // normal y
            vertices.push_back(nz); // normal z
        }
    }
    
    // Generate indices for triangles
    for (int lat = 0; lat < latitudeSegments; ++lat) {
        for (int lon = 0; lon < longitudeSegments; ++lon) {
            int current = lat * (longitudeSegments + 1) + lon;
            int next = current + longitudeSegments + 1;
            
            // First triangle
            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);
            
            // Second triangle
            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }
}

void generateRopeLine(glm::vec3 anchorPoint, glm::vec3 spherePos, std::vector<float>& lineVertices) {
    lineVertices.clear();

    // anchor:
    lineVertices.push_back(anchorPoint.x);
    lineVertices.push_back(anchorPoint.y);
    lineVertices.push_back(anchorPoint.z);

    // sphere center
    lineVertices.push_back(spherePos.x);
    lineVertices.push_back(spherePos.y);
    lineVertices.push_back(spherePos.z);

}



static float fpsTimer = 0.0f;
static int frameCount = 0;

int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);
//    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // build and compile our shader zprogram
    // ------------------------------------
    Shader lightingShader("2.2.basic_lighting.vs", "2.2.basic_lighting.fs");
    Shader lightCubeShader("2.2.light_cube.vs", "2.2.light_cube.fs");



    std::vector<float> sphereVertices;
    std::vector<unsigned int> sphereIndices;
    //generateSphere(0.5f, 30, 30, sphereVertices, sphereIndices); // radius, lat segments, lon segments
    
    generateSphere(sphereRadius, 20, 20, sphereVertices, sphereIndices); // smaller radius and fewer segments for performance
    // set up vertex data (and buffer(s)) and configure vertex attributes
    // ------------------------------------------------------------------

    glm::vec3 spherePosition = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 sphereVelocity = glm::vec3(0.0f, 0.0f, 0.0f);


    float vertices[] = {
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,

        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,

         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f
    };
    // first, configure the cube's VAO (and VBO)
    // Setup sphere VAO and VBO
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
    
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // particle setup:
    unsigned int traceVBO, traceVAO;
    glGenVertexArrays(1, &traceVAO);
    glGenBuffers(1, &traceVBO);

    glBindVertexArray(traceVAO);
    glBindBuffer(GL_ARRAY_BUFFER, traceVBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_TRACE_PARTICLES * 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // rope setup
    
  //  glm::vec3 anchorPoint(0.0, 2.0f, 0.0f);
    std::vector<float> ropeVertices;
    unsigned int ropeVAO, ropeVBO;

    glGenVertexArrays(1, &ropeVAO);
    glGenBuffers(1, &ropeVBO);

    glBindVertexArray(ropeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, ropeVBO);

    // position attribute- only need position for simple line
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    
    // Setup light cube VAO and VBO (for the light source - keep this the same)
    unsigned int cubeVBO, lightCubeVAO;
    glGenVertexArrays(1, &lightCubeVAO);
    glGenBuffers(1, &cubeVBO);
    
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glBindVertexArray(lightCubeVAO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // important inital tings for da pendulum:
    theta_old = 20.0f * M_PI/180.0f;
    float theta_new;
    float g = 9.81f; // m/s^2
    
    // float rope_L = 3.0f;
    float x_old = anchorPoint.x + sin(theta_old)*rope_L;
    float y_old = anchorPoint.y -cos(theta_old)*rope_L;
    float z_old = anchorPoint.z;
    float x_new;
    float y_new;
    float z_new;

    theta_dot_old = 0;
    float theta_dot_new;

    float theta_dot_dot_old = -(g/rope_L)*sin(theta_old);
    float theta_dot_dot_new;

    globalSpherePos = glm::vec3(x_old, y_old, z_old);



    // render loop
    // -----------
    
    while (!glfwWindowShouldClose(window))
    {
        // per-frame time logic
        // --------------------
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        frameCount++;
        fpsTimer += deltaTime;

        if (fpsTimer) {
            float fps = frameCount/fpsTimer;
            std::cout << "FPS: " << fps << std::endl;
            frameCount = 0;
            fpsTimer = 0.0f;
        }


        // input
        // -----
        processInput(window);

        // render
        // ------
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        

        // UPDATE SPHERE AND ROPE:
        // -------

        globalProjection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        globalView = camera.GetViewMatrix();
        

        if (!isDragging) {
            theta_dot_new = theta_dot_old + theta_dot_dot_old * deltaTime;
            theta_new = theta_old + theta_dot_new * deltaTime;
            theta_dot_dot_new = -(g/rope_L) * sin(theta_new);
            
            x_new = anchorPoint.x + sin(theta_new) * rope_L;
            y_new = anchorPoint.y - cos(theta_new) * rope_L;
            z_new = anchorPoint.z;

            theta_old = theta_new;
            theta_dot_old = theta_dot_new;
            theta_dot_dot_old = theta_dot_dot_new;
        } else {
            // position is set by mouse callback
            x_new = globalSpherePos.x;
            y_new = globalSpherePos.y;
            z_new = globalSpherePos.z;
        }

        globalSpherePos = glm::vec3(x_new, y_new, z_new);





        x_old = x_new;
        y_old = y_new;
        z_old = z_new;

        
        // END OF DUPTATENG


        // update trace particleS:
        // -----
        
        timeSinceLastSpawn += deltaTime;
        if (timeSinceLastSpawn >= 1.0f/traceSpawnRate) {
            for (auto& particle : traceParticles) {
                if (particle.life <= 0.0f) {
                    particle.position = spherePosition; // spawn at sphere center
                    particle.life = 1.0f;
                    break;
                }
            }
            timeSinceLastSpawn = 0.0f;
        }

        std::vector<float> tracePositions;
        for (auto& particle : traceParticles) {
            if (particle.life > 0.0f) {
                particle.life -= deltaTime/traceLifeTime;

                tracePositions.push_back(particle.position.x);
                tracePositions.push_back(particle.position.y);
                tracePositions.push_back(particle.position.z);
            }
        }
    

        // be sure to activate shader when setting uniforms/drawing objects
        lightingShader.use();
        lightingShader.setVec3("objectColor", 0.0f, 0.5f, 0.31f);
        lightingShader.setVec3("lightColor", 1.0f, 1.0f, 1.0f);
        lightingShader.setVec3("lightPos", lightPos);
        lightingShader.setVec3("viewPos", camera.Position);
        
  
        lightingShader.setMat4("projection", globalProjection);
        lightingShader.setMat4("view", globalView);
        
        // sphere:
        
        spherePosition = glm::vec3(x_new, y_new, z_new);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, spherePosition);
        lightingShader.setMat4("model", model);
        
        // Render the sphere using indices
        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);

        // rope time:
        generateRopeLine(anchorPoint, spherePosition, ropeVertices);

        glBindBuffer(GL_ARRAY_BUFFER, ropeVBO);
        glBufferData(GL_ARRAY_BUFFER,  ropeVertices.size() * sizeof(float), ropeVertices.data(), GL_DYNAMIC_DRAW);

        // particles:
        // if (!tracePositions.empty()) {
        //     lightCubeShader.use();
        //     lightCubeShader.setMat4("projection", globalProjection);
        //     lightCubeShader.setMat4("view", globalView);
        //     lightCubeShader.setMat4("model", glm::mat4(1.0f));
        //     lightCubeShader.setVec3("color", 0.0f, 1.0f, 0.0f);

        //     glBindVertexArray(traceVAO);
        //     glBindBuffer(GL_ARRAY_BUFFER, traceVBO);
        //     glBufferSubData(GL_ARRAY_BUFFER, 0, tracePositions.size() * sizeof(float), tracePositions.data());

        //     glPointSize(3.0f);
        //     glDrawArrays(GL_POINTS, 0, tracePositions.size()/3);

        // }

        for (auto& particle : traceParticles) {
            if (particle.life > 0.0f) {
                float intensity = particle.life; // 1.0 when fresh, 0.0 when dying

                lightCubeShader.use();
                lightCubeShader.setMat4("projection", globalProjection);
                lightCubeShader.setMat4("view", globalView);
                lightCubeShader.setMat4("model", glm::mat4(1.0f));
                lightCubeShader.setVec3("color", 0.0f, intensity, 0.0f);


                glBindVertexArray(traceVAO);
                glBindBuffer(GL_ARRAY_BUFFER, traceVBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, 3 * sizeof(float), &particle.position.x);

                glPointSize(4.0f);
                glDrawArrays(GL_POINTS, 0, 1);
            }
        }


        // light ube shader
        lightCubeShader.use();
        lightCubeShader.setMat4("projection", globalProjection);
        lightCubeShader.setMat4("view", globalView);
        glm::mat4 ropeModel = glm::mat4(1.0f);
        lightCubeShader.setMat4("model", ropeModel);

        // rope color set to... brown
        lightCubeShader.setVec3("color", 0.6f, 0.3f, 0.1f);

        glBindVertexArray(ropeVAO);
        glLineWidth(5.0f);
        glDrawArrays(GL_LINES, 0, 2);


        model = glm::mat4(1.0f);
        model = glm::translate(model, lightPos);
        model = glm::scale(model, glm::vec3(0.2f));
        lightCubeShader.setMat4("model", model);

        lightCubeShader.setVec3("color", 1.0f, 1.0f, 1.0f); // White color

        
        glBindVertexArray(lightCubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    glDeleteVertexArrays(1, &sphereVAO);
    glDeleteVertexArrays(1, &lightCubeVAO);
    glDeleteBuffers(1, &sphereVBO);
    glDeleteBuffers(1, &sphereEBO);
    glDeleteBuffers(1, &cubeVBO);

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
bool wangWANG = true;
bool rKeyPressed = false;  // Add this with your other globals
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
    
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS && !rKeyPressed) {
        if (wangWANG)
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        else
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        
        wangWANG = !wangWANG;
      //  std::cout << "wang wang: " << wangWANG << std::endl;
        rKeyPressed = true;
    } else if (glfwGetKey(window, GLFW_KEY_R) == GLFW_RELEASE) {
        rKeyPressed = false;
    }

    // Toggle wireframe on key press (not hold)
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
}





// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}



void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            double mouseX, mouseY;
            glfwGetCursorPos(window, &mouseX, &mouseY);

            //Project sphere coords to screen coords:
            glm::vec4 sphereWorldPos = glm::vec4(globalSpherePos, 1.0);
            glm::vec4 sphereClipPos = globalProjection*globalView*sphereWorldPos;
            // normalized device coordaintes
            glm::vec3 sphereNDC = glm::vec3(sphereClipPos) / sphereClipPos.w;

            float sphereScreenX = (sphereNDC.x + 1.0f) * 0.5f*SCR_WIDTH; // maps [-1,1] to [0, SCR_WIDTH]
            float sphereScreenY = (1.0f - sphereNDC.y) * 0.5f*SCR_HEIGHT;

            // now need to project a point on the edge of sphere:
            glm::vec4 edgeWorldPos = glm::vec4(globalSpherePos.x + sphereRadius, globalSpherePos.y, globalSpherePos.z, 1.0);
            glm::vec4 edgeClipPos = globalProjection*globalView*edgeWorldPos;
            glm::vec3 edgeNDC = glm::vec3(edgeClipPos)/edgeClipPos.w;

            float edgeX = (edgeNDC.x + 1.0f)*0.5f*SCR_WIDTH;

            float pixel_sphere_radius = abs(sphereScreenX-edgeX);
            
            float distance = sqrt(pow(mouseX-sphereScreenX, 2)+pow(mouseY-sphereScreenY, 2));
            if (distance < pixel_sphere_radius) {
                isDragging = true;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      //          std::cout << "Started Dragging" << std::endl;
            }




        } else if (action == GLFW_RELEASE) {
            if (isDragging) {
                double xpos, ypos;
                glfwGetCursorPos(window, &xpos, &ypos);
          ///      std::cout << "Released at: " << xpos << ", " << ypos << std::endl;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); 

                lastX = xpos;
                lastY = ypos;
                firstMouse = true;
                


                isDragging = false;
            }
        }
    }
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
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
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    if (isDragging) {
        // 2D case:
        // project anchor to screen coords:
        glm::vec4 anchorClip = globalProjection * globalView * glm::vec4(anchorPoint, 1.0);
        glm::vec3 anchorNDC = glm::vec3(anchorClip) / anchorClip.w;
        float anchorScreenX = (anchorNDC.x + 1.0f) * 0.5f * SCR_WIDTH;
        float anchorScreenY = (1.0f - anchorNDC.y) * 0.5f * SCR_HEIGHT;

        // calculate the angle from anchor to mouse on screen
        float deltaX = xpos - anchorScreenX;
        float deltaY = ypos-anchorScreenY;
        float screenAngle = atan2(deltaX, deltaY);

        // convert directly to world position:
        glm::vec3 newSpherePos = glm::vec3(
            anchorPoint.x + sin(screenAngle) * rope_L,
            anchorPoint.y - cos(screenAngle) * rope_L,
            anchorPoint.z
        );

        //Update physics variables:
        theta_old = screenAngle;
        theta_dot_old = 0.0f;
        globalSpherePos = newSpherePos;

        // std::cout << "Dragging - Screen angle: " << screenAngle << ", New pos: " << 
        //             newSpherePos.x << ", " << newSpherePos.y << std::endl;
                 
    } else {
        camera.ProcessMouseMovement(xoffset, yoffset);
    }

}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}


