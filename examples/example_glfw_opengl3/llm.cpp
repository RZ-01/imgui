// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
//
// This file has been modified to create a clean starting point for a new application.

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <vector>
#include <string>
#include <cmath> // For basic math operations

// NEW: OpenGL Extension Loader
// GLAD is used to load modern OpenGL functions. Ensure you have it configured in your project.
#include <glad/glad.h> 

#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// NEW: Application State Management
enum class RequestStatus {
    IDLE,
    SENDING,
};

struct AppState {
    char prompt_buffer[1024] = "Make the cube twice as tall.";
    RequestStatus status = RequestStatus::IDLE;
    std::vector<std::string> log_messages;
    double request_sent_time = 0.0; // Correctly track request time
};

// NEW: Framebuffer structure for our 3D viewport
struct Framebuffer {
    GLuint FBO = 0;
    GLuint textureID = 0;
    GLuint RBO = 0; // Renderbuffer Object for depth/stencil
    int width = 0;
    int height = 0;
};

// NEW: Helper function to create a framebuffer
void createFramebuffer(Framebuffer& fb, int width, int height) {
    if (fb.FBO) { // If framebuffer already exists, delete it
        glDeleteFramebuffers(1, &fb.FBO);
        glDeleteTextures(1, &fb.textureID);
        glDeleteRenderbuffers(1, &fb.RBO);
    }

    fb.width = width;
    fb.height = height;

    glGenFramebuffers(1, &fb.FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, fb.FBO);

    glGenTextures(1, &fb.textureID);
    glBindTexture(GL_TEXTURE_2D, fb.textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb.textureID, 0);

    glGenRenderbuffers(1, &fb.RBO);
    glBindRenderbuffer(GL_RENDERBUFFER, fb.RBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, fb.RBO);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        fprintf(stderr, "ERROR::FRAMEBUFFER:: Framebuffer is not complete!\n");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Main code
int main(int, char**)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3); // Request a slightly newer GL version for FBOs
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    #ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif

    GLFWwindow* window = glfwCreateWindow(1280, 720, "LLM 3D Control UI", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // NEW: Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        fprintf(stderr, "Failed to initialize GLAD\n");
        return -1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // --- NEW: 3D Scene Setup ---
    // Shaders
    const char* vertex_shader_source = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;
        void main() {
            gl_Position = projection * view * model * vec4(aPos, 1.0);
        }
    )";
    const char* fragment_shader_source = R"(
        #version 330 core
        out vec4 FragColor;
        void main() {
            FragColor = vec4(0.2, 0.5, 0.8, 1.0);
        }
    )";

    // Compile shaders and create shader program (error checking omitted for brevity)
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
    glCompileShader(vertex_shader);
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
    glCompileShader(fragment_shader);
    GLuint shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glLinkProgram(shader_program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    // Cube vertices
    float vertices[] = {
        -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,
    };
    unsigned int indices[] = {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4, 0, 4, 7, 7, 3, 0, 1, 5, 6, 6, 2, 1, 3, 7, 6, 6, 2, 3, 0, 4, 5, 5, 1, 0};
    GLuint VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Framebuffer for the 3D viewport
    Framebuffer viewport_fb;
    createFramebuffer(viewport_fb, 800, 600); // Initial size

    // Our application state
    AppState app_state;
    app_state.log_messages.push_back("[Info] Application started. Waiting for command.");

    ImVec4 clear_color = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // --- 1. Render 3D Scene to Framebuffer ---
        glBindFramebuffer(GL_FRAMEBUFFER, viewport_fb.FBO);
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shader_program);

        // Simple matrix math for a rotating cube
        float time_val = (float)glfwGetTime();
        float angle = time_val * 50.0f;
        // Basic model matrix
        float model_mat[16] = { static_cast<float>(cos(angle*0.01f)), 0, static_cast<float>(sin(angle*0.01f)), 0, 0, 1, 0, 0, static_cast<float>(-sin(angle*0.01f)), 0, static_cast<float>(cos(angle*0.01f)), 0, 0, 0, 0, 1};
        // Basic view matrix
        float view_mat[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,-3,1};
        // Basic projection matrix
        float aspect = (float)viewport_fb.width / (float)viewport_fb.height;
        float fov = 45.0f * 3.14159f / 180.0f;
        float f = 1.0f / tan(fov / 2.0f);
        float proj_mat[16] = {f/aspect,0,0,0, 0,f,0,0, 0,0,(100.0f+0.1f)/(0.1f-100.0f),-1, 0,0,2*100.0f*0.1f/(0.1f-100.0f),0};
        
        glUniformMatrix4fv(glGetUniformLocation(shader_program, "model"), 1, GL_FALSE, model_mat);
        glUniformMatrix4fv(glGetUniformLocation(shader_program, "view"), 1, GL_FALSE, view_mat);
        glUniformMatrix4fv(glGetUniformLocation(shader_program, "projection"), 1, GL_FALSE, proj_mat);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        // Unbind the framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDisable(GL_DEPTH_TEST);

        // --- 2. Render ImGui UI ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport()->ID);

        // Panel 1: Controls & Commands
        ImGui::Begin("Controls & Commands");
        ImGui::Text("Enter your command for the LLM:");
        ImGui::InputTextMultiline("##Prompt", app_state.prompt_buffer, sizeof(app_state.prompt_buffer), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 6));
        bool is_processing = (app_state.status == RequestStatus::SENDING);
        if (is_processing) ImGui::BeginDisabled();
        if (ImGui::Button("Send Command")) {
            if (!is_processing) {
                app_state.status = RequestStatus::SENDING;
                app_state.request_sent_time = glfwGetTime(); // Correctly set time on send
                app_state.log_messages.push_back("[Info] Sending prompt: " + std::string(app_state.prompt_buffer));
            }
        }
        if (is_processing) {
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::Text("Processing...");
        }
        ImGui::End();

        // Panel 2: LLM Command Log
        ImGui::Begin("LLM Command Log");
        for (const auto& msg : app_state.log_messages) {
            ImGui::TextUnformatted(msg.c_str());
        }
        if (app_state.status == RequestStatus::SENDING) {
            if (glfwGetTime() - app_state.request_sent_time > 2.0) {
                 app_state.log_messages.push_back("[Success] LLM responded. Executing: extrude(face=3, height=10).");
                 app_state.status = RequestStatus::IDLE;
            }
        }
        ImGui::End();
        
        // Panel 3: 3D Viewport
        ImGui::Begin("3D Viewport");
        // Check if the viewport size has changed
        ImVec2 viewport_panel_size = ImGui::GetContentRegionAvail();
        if (viewport_panel_size.x != viewport_fb.width || viewport_panel_size.y != viewport_fb.height) {
            // Recreate framebuffer with new size
            createFramebuffer(viewport_fb, (int)viewport_panel_size.x, (int)viewport_panel_size.y);
        }
        // Display the FBO texture
        ImGui::Image((void*)(intptr_t)viewport_fb.textureID, viewport_panel_size, ImVec2(0, 1), ImVec2(1, 0));
        ImGui::End();

        // --- 3. Final Rendering to Screen ---
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shader_program);
    glDeleteFramebuffers(1, &viewport_fb.FBO);
    glDeleteTextures(1, &viewport_fb.textureID);
    glDeleteRenderbuffers(1, &viewport_fb.RBO);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
