// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
//
// This file has been modified to implement a "Visual Polish Pass",
// including custom fonts, icons, and a professional, Apple-inspired UI style.

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <vector>
#include <string>
#include <cmath>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

// NEW: Include the header for Font Awesome icons
#include "IconsFontAwesome6.h"

#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// --- Application State Management ---
enum class RequestStatus {
    IDLE,
    SENDING,
};
struct AppState {
    char prompt_buffer[1024] = "Make the cube twice as tall.";
    RequestStatus status = RequestStatus::IDLE;
    std::vector<std::string> log_messages;
    double request_sent_time = 0.0;
};

// --- 3D Scene Data ---
struct Framebuffer {
    GLuint FBO = 0; GLuint textureID = 0; GLuint RBO = 0;
    int width = 0; int height = 0;
};
void createFramebuffer(Framebuffer& fb, int width, int height); // Forward declaration

// NEW: Step 1c - A function to apply our custom, professional style
void SetAppleStyle() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Colors
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 1.00f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.2f, 0.2f, 0.2f, 0.54f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.25f, 0.78f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);

    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.29f, 0.48f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);

    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);

    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);

    style.Colors[ImGuiCol_Button] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);

    style.Colors[ImGuiCol_Header] = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    
    style.Colors[ImGuiCol_Separator] = style.Colors[ImGuiCol_Border];
    style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
    style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);

    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);

    style.Colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.35f, 0.58f, 0.86f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.41f, 0.68f, 1.00f);
    style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.09f, 0.15f, 0.22f, 0.97f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.26f, 0.42f, 1.00f);

    style.Colors[ImGuiCol_DockingPreview] = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
    style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);

    style.Colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);

    // Style properties
    style.WindowPadding = ImVec2(8.00f, 8.00f);
    style.FramePadding = ImVec2(5.00f, 2.00f);
    style.CellPadding = ImVec2(6.00f, 6.00f);
    style.ItemSpacing = ImVec2(6.00f, 6.00f);
    style.ItemInnerSpacing = ImVec2(6.00f, 6.00f);
    style.TouchExtraPadding = ImVec2(0.00f, 0.00f);
    style.IndentSpacing = 25;
    style.ScrollbarSize = 15;
    style.GrabMinSize = 10;
    style.WindowBorderSize = 1;
    style.ChildBorderSize = 1;
    style.PopupBorderSize = 1;
    style.FrameBorderSize = 1;
    style.TabBorderSize = 1;
    style.WindowRounding = 7;
    style.ChildRounding = 7;
    style.FrameRounding = 7;
    style.PopupRounding = 7;
    style.ScrollbarRounding = 9;
    style.GrabRounding = 7;
    style.LogSliderDeadzone = 4;
    style.TabRounding = 4;
}

// Main code
int main(int, char**)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    #ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif

    GLFWwindow* window = glfwCreateWindow(1280, 720, "LLM 3D Control UI", nullptr, nullptr);
    if (window == nullptr) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        fprintf(stderr, "Failed to initialize GLAD\n");
        return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // NEW: Step 1a & 1b - Load custom fonts and merge icons
    float base_font_size = 18.0f;
    io.Fonts->AddFontFromFileTTF("fonts/Inter-Regular.ttf", base_font_size);

    ImFontConfig config;
    config.MergeMode = true; // This is the magic! It merges the icons into the main font.
    config.GlyphMinAdvanceX = base_font_size; // Use if you want to make the icons mono-spaced
    static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    io.Fonts->AddFontFromFileTTF("fonts/Font Awesome 6 Free-Solid-900.otf", base_font_size, &config, icon_ranges);
    
    // Fallback font if above fails
    io.Fonts->AddFontDefault();
    // io.Fonts->Build();

    // NEW: Step 1c - Apply our custom style
    SetAppleStyle();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // --- 3D Scene Setup (Same as before) ---
    const char* vertex_shader_source = R"(#version 330 core
        layout (location = 0) in vec3 aPos; uniform mat4 model; uniform mat4 view; uniform mat4 projection;
        void main() { gl_Position = projection * view * model * vec4(aPos, 1.0); })";
    const char* fragment_shader_source = R"(#version 330 core
        out vec4 FragColor; void main() { FragColor = vec4(0.2, 0.5, 0.8, 1.0); })";
    GLuint shader_program; // ... Shader compilation and linking as before ...
    // --- (Shader compilation logic omitted for brevity, it's the same as the previous version) ---
        GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
    glCompileShader(vertex_shader);
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
    glCompileShader(fragment_shader);
    shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glLinkProgram(shader_program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    // --- (End of shader compilation logic) ---

    GLuint VAO; // ... VAO/VBO/EBO setup for cube as before ...
        float vertices[] = {
        -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,
    };
    unsigned int indices[] = {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4, 0, 4, 7, 7, 3, 0, 1, 5, 6, 6, 2, 1, 3, 7, 6, 6, 2, 3, 0, 4, 5, 5, 1, 0};
    GLuint VBO, EBO;
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
    // --- (End of cube setup) ---


    Framebuffer viewport_fb; createFramebuffer(viewport_fb, 1, 1);
    AppState app_state; app_state.log_messages.push_back("[Info] Application started. Waiting for command.");
    ImVec4 clear_color = ImVec4(0.1f, 0.1f, 0.1f, 1.00f);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport()->ID);

        // --- UI Panels with new style ---
        ImGui::Begin(ICON_FA_TERMINAL " Controls & Commands"); // NEW: Icon in title
        ImGui::Text("Enter your command for the LLM:");
        ImGui::InputTextMultiline("##Prompt", app_state.prompt_buffer, sizeof(app_state.prompt_buffer), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 6));
        
        bool is_processing = (app_state.status == RequestStatus::SENDING);
        if (is_processing) ImGui::BeginDisabled();
        
        // NEW: Icon in button
        if (ImGui::Button(ICON_FA_PAPER_PLANE " Send Command")) {
            if (!is_processing) {
                app_state.status = RequestStatus::SENDING;
                app_state.request_sent_time = glfwGetTime();
                app_state.log_messages.push_back(std::string(ICON_FA_ARROW_UP) + " [Info] Sending prompt: " + std::string(app_state.prompt_buffer));
            }
        }
        if (is_processing) {
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::Text(ICON_FA_HOURGLASS_HALF " Processing..."); // NEW: Icon in status text
        }
        ImGui::End();

        ImGui::Begin(ICON_FA_CLIPBOARD " LLM Command Log"); // NEW: Icon in title
        for (const auto& msg : app_state.log_messages) {
            ImGui::TextUnformatted(msg.c_str());
        }
        if (app_state.status == RequestStatus::SENDING) {
            if (glfwGetTime() - app_state.request_sent_time > 2.0) {
                 app_state.log_messages.push_back(std::string(ICON_FA_CHECK) + " [Success] LLM responded. Executing: extrude(face=3, height=10).");
                 app_state.status = RequestStatus::IDLE;
            }
        }
        ImGui::End();
        
        ImGui::SetNextWindowSizeConstraints(ImVec2(200, 200), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0)); // NEW: Remove padding for the viewport
        ImGui::Begin(ICON_FA_CUBE " 3D Viewport"); // NEW: Icon in title
        ImGui::PopStyleVar();

        ImVec2 viewport_panel_size = ImGui::GetContentRegionAvail();
        if (viewport_panel_size.x != viewport_fb.width || viewport_panel_size.y != viewport_fb.height) {
            createFramebuffer(viewport_fb, (int)viewport_panel_size.x, (int)viewport_panel_size.y);
        }

        if (viewport_fb.width > 0 && viewport_fb.height > 0)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, viewport_fb.FBO);
            glViewport(0, 0, viewport_fb.width, viewport_fb.height);
            glEnable(GL_DEPTH_TEST);
            glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glUseProgram(shader_program);
            // ... Matrix and rendering logic as before ...
            float time_val = (float)glfwGetTime(); float angle_rad = time_val * 0.8f;
            float model_mat[16] = { cosf(angle_rad), 0, sinf(angle_rad), 0, 0, 1, 0, 0, -sinf(angle_rad), 0, cosf(angle_rad), 0, 0, 0, 0, 1};
            float view_mat[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,-3.0f,1};
            float fov_rad = 45.0f * 3.14159f / 180.0f; float aspect = (float)viewport_fb.width / (float)viewport_fb.height; float near_plane = 0.1f; float far_plane = 100.0f; float f = 1.0f / tanf(fov_rad / 2.0f);
            float proj_mat[16] = { f / aspect, 0, 0, 0, 0, f, 0, 0, 0, 0, (far_plane + near_plane) / (near_plane - far_plane), -1.0f, 0, 0, (2.0f * far_plane * near_plane) / (near_plane - far_plane), 0.0f };
            glUniformMatrix4fv(glGetUniformLocation(shader_program, "model"), 1, GL_FALSE, model_mat);
            glUniformMatrix4fv(glGetUniformLocation(shader_program, "view"), 1, GL_FALSE, view_mat);
            glUniformMatrix4fv(glGetUniformLocation(shader_program, "projection"), 1, GL_FALSE, proj_mat);
            glBindVertexArray(VAO);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        ImGui::Image((void*)(intptr_t)viewport_fb.textureID, viewport_panel_size, ImVec2(0, 1), ImVec2(1, 0));
        ImGui::End();

        ImGui::Render();
        int display_w, display_h; glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    glDeleteVertexArrays(1, &VAO); glDeleteBuffers(1, &VBO); glDeleteBuffers(1, &EBO); glDeleteProgram(shader_program);
    glDeleteFramebuffers(1, &viewport_fb.FBO); glDeleteTextures(1, &viewport_fb.textureID); glDeleteRenderbuffers(1, &viewport_fb.RBO);
    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext();
    glfwDestroyWindow(window); glfwTerminate();

    return 0;
}

// Framebuffer creation function definition
void createFramebuffer(Framebuffer& fb, int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (fb.FBO) { glDeleteFramebuffers(1, &fb.FBO); glDeleteTextures(1, &fb.textureID); glDeleteRenderbuffers(1, &fb.RBO); }
    fb.width = width; fb.height = height;
    glGenFramebuffers(1, &fb.FBO); glBindFramebuffer(GL_FRAMEBUFFER, fb.FBO);
    glGenTextures(1, &fb.textureID); glBindTexture(GL_TEXTURE_2D, fb.textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb.textureID, 0);
    glGenRenderbuffers(1, &fb.RBO); glBindRenderbuffer(GL_RENDERBUFFER, fb.RBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, fb.RBO);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) fprintf(stderr, "ERROR::FRAMEBUFFER:: Framebuffer is not complete!\n");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}