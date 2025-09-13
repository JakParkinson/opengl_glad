#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>



#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader_m.h"
#include "camera.h"
#include "model.h"

#include <iostream>


void framebuffer_size_callback(GLFWwindow* window, int width, int height);

const unsigned int SCR_WIDTH = 2000;
const unsigned int SCR_HEIGHT = 1400;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);



// camera
Camera camera(glm::vec3(0.0f, 0.0f, 100.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float orbitTime = 0.0f;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;



int main()
{
        glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);


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
    glfwSetScrollCallback(window, scroll_callback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // flip it around
    stbi_set_flip_vertically_on_load(true);

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);

    //Shaders:
    Shader asteroidShader("asteroids.vs", "asteroids.fs");
    Shader planetShader("planets.vs", "planets.fs");

    //models
    Model rock("resources/objects/rock/rock.obj");
    Model planet("resources/objects/planet/planet.obj");


   // Model rock("resources/objects/rock/rock.obj");
    std::cout << "Rock meshes loaded: " << rock.meshes.size() << std::endl;
    std::cout << "Rock textures loaded: " << rock.textures_loaded.size() << std::endl;

    // Also check if the model file exists
    std::ifstream file("resources/objects/rock/rock.obj");
    if (!file.good()) {
        std::cout << "Model file not found!" << std::endl;
    } else {
        std::cout << "Model file exists" << std::endl;
    }

    //semi-random transformation matrices
    unsigned int amount = 5000;
    glm::mat4* modelMatrices;
    modelMatrices = new glm::mat4[amount];
    srand(glfwGetTime());
    float radius = 50.0f;
    float offset = 6.5f;

    float* asteroidScales = new float[amount];
    float* asteroidRadii = new float[amount];      // Each asteroid's distance from center
    float* asteroidHeights = new float[amount];    // Each asteroid's y offset
    float* asteroidRotAngles = new float[amount];

    float scaleRange = 30.0f; // the variation in asteroid size
    float minScale = 0.05f; // smallest asteroid size

    for (unsigned int i=0;i<amount;i++)
    {
        glm::mat4 model = glm::mat4(1.0f);
        // translation: displace along circle wih radius in range [-offset, offset]
        float angle = (float)i/(float)amount*360.0f;
        float displacement = (rand()%(int)(2*offset*100))/100.0f-offset;
        asteroidRadii[i] = radius + displacement;

        displacement = (rand() % (int)(2 * offset * 100)) / 100.0f - offset;
        asteroidHeights[i] = displacement * 0.4f;  // Store y offset

        asteroidScales[i] = (rand() % (int)scaleRange) / 100.0f + minScale;
        asteroidRotAngles[i] = (rand() % 360);

        float x = sin(angle) * asteroidRadii[i];
        float y = asteroidHeights[i];
        float z = cos(angle) * asteroidRadii[i];

        model = glm::translate(model, glm::vec3(x,y,z));

        // scale: scale between 0.05 and 0.25f
        model = glm::scale(model, glm::vec3(asteroidScales[i]));

        model = glm::rotate(model, asteroidRotAngles[i], glm::vec3(0.4f, 0.6f, 0.8f));

   
        // add to list of matrix
        modelMatrices[i] = model;
    } // after this for loop have amount number of modelMatrices describing the matrices for asteroid positions


    // we need to configure the instanced array:
    unsigned int buffer;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, amount*sizeof(glm::mat4), &modelMatrices[0], GL_STATIC_DRAW);

    //
    for (unsigned int i = 0; i < rock.meshes.size(); i++)
    {
        unsigned int VAO = rock.meshes[i].VAO;
        glBindVertexArray(VAO);
        // set attribute pointers for matrix (4 times vec4)
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)0);
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(glm::vec4)));
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(2 * sizeof(glm::vec4)));
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(3 * sizeof(glm::vec4)));

        glVertexAttribDivisor(3, 1);
        glVertexAttribDivisor(4, 1);
        glVertexAttribDivisor(5, 1);
        glVertexAttribDivisor(6, 1);

        glBindVertexArray(0);
    }



    //render loop
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        orbitTime += deltaTime*0.1f; // 0.5f controls speed of orbit
        for (unsigned int i = 0; i < amount; i++)
        {
            glm::mat4 model = glm::mat4(1.0f);
            float baseAngle =  (float)i / (float)amount * 360.0f;
            float currentAngle = baseAngle + orbitTime*30.0f; // 30.0f degrees per second

            //circle:
            float x = sin(glm::radians(currentAngle)) * asteroidRadii[i];
            float z = cos(glm::radians(currentAngle)) * asteroidRadii[i];
            float y = -3.0f + asteroidHeights[i];  // Add the stored height offset
            model = glm::translate(model, glm::vec3(x,y,z));
            model = glm::scale(model, glm::vec3(asteroidScales[i]));
            model = glm::rotate(model, asteroidRotAngles[i], glm::vec3(0.4f, 0.6f, 0.8f)); 

            //scale as before
            modelMatrices[i] = model;
        }

        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glBufferSubData(GL_ARRAY_BUFFER, 0, amount * sizeof(glm::mat4), &modelMatrices[0]);
                

        //input
        processInput(window);
        //render
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        //configure transformatoin matrices
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH/(float)SCR_HEIGHT, 0.1f, 1000.0f);
        glm::mat4 view = camera.GetViewMatrix();
        asteroidShader.use();
        asteroidShader.setMat4("projection", projection);
        asteroidShader.setMat4("view", view);
        planetShader.use();
        planetShader.setMat4("projection", projection);
        planetShader.setMat4("view", view);

        //draw planet
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, -3.0f, 0.0f));
        model = glm::scale(model, glm::vec3(4.0f,4.0f,4.0f));
        planetShader.setMat4("model",model);
        planet.Draw(planetShader);

        //meteroites
        asteroidShader.use();
        asteroidShader.setInt("texture_diffuse1", 0);
        glActiveTexture(GL_TEXTURE0);
        if (!rock.textures_loaded.empty()) {
            glBindTexture(GL_TEXTURE_2D, rock.textures_loaded[0].id); 
        } else{
            std::cout << "empty!" << std::endl;
        }

        
        for (unsigned int i = 0; i<rock.meshes.size(); i++)
        {
            glBindVertexArray(rock.meshes[i].VAO);
            glDrawElementsInstanced(GL_TRIANGLES, static_cast<unsigned int>(rock.meshes[i].indices.size()), GL_UNSIGNED_INT, 0, amount);
            glBindVertexArray(0);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}




// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
    float move_velocity = 6.0f;
    float keyboard_change = move_velocity * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    {
     //   std::cout << "deltaTime: " << deltaTime << "keyboard change: " << keyboard_change << std::endl;
        camera.ProcessKeyboard(FORWARD, keyboard_change);
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, keyboard_change);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, keyboard_change);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, keyboard_change);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
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

    camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}