// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
//
// This file has been updated to implement a "Visual Polish Pass",
// including custom fonts, icons, and a professional, Apple-inspired UI style.
// NOTE: Interactive camera controls have been temporarily reverted to ensure stability.

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <vector>
#include <string>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <functional>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "IconsFontAwesome6.h"

#define IMGUI_NOTIFY_IMPLEMENTATION
#include "plugins/ImGuiNotify.hpp"
#include "plugins/imspinner.h"

#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// --- Application State Management ---
enum class RequestStatus { IDLE, SENDING };
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

// --- Helper Functions ---
void createFramebuffer(Framebuffer& fb, int width, int height);
GLuint CreateShaderProgram(const char* vs_src, const char* fs_src) {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vs_src, NULL);
    glCompileShader(vs);
    int success;
    char infoLog[512];
    glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vs, 512, NULL, infoLog);
        fprintf(stderr, "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n%s\n", infoLog);
        return 0;
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fs_src, NULL);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fs, 512, NULL, infoLog);
        fprintf(stderr, "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n%s\n", infoLog);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        fprintf(stderr, "ERROR::SHADER::PROGRAM::LINKING_FAILED\n%s\n", infoLog);
        return 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

void CreateCubeVAO(GLuint& vao, GLuint& vbo, GLuint& ebo) {
    float vertices[] = {
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,
    };
    unsigned int indices[] = {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4,
        0, 4, 7, 7, 3, 0,
        1, 5, 6, 6, 2, 1,
        3, 7, 6, 6, 2, 3,
        0, 4, 5, 5, 1, 0
    };
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}
void SetDarkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Colors
    style.Colors[ImGuiCol_Text]                  = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    style.Colors[ImGuiCol_ChildBg]               = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_PopupBg]               = ImVec4(1.00f, 1.00f, 1.00f, 0.98f);
    style.Colors[ImGuiCol_Border]                = ImVec4(0.00f, 0.00f, 0.00f, 0.30f);
    style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg]               = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.90f, 0.90f, 0.90f, 0.40f);
    style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.85f, 0.85f, 0.85f, 0.45f);
    style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.78f, 0.78f, 0.78f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.86f, 0.86f, 0.86f, 0.75f);
    style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.98f, 0.98f, 0.98f, 0.53f);
    style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.69f, 0.69f, 0.69f, 0.80f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.49f, 0.49f, 0.49f, 0.80f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
    style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.00f, 0.47f, 0.84f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.00f, 0.47f, 0.84f, 0.78f);
    style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.00f, 0.47f, 0.84f, 1.00f);
    style.Colors[ImGuiCol_Button]                = ImVec4(0.00f, 0.47f, 0.84f, 0.60f);
    style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.00f, 0.47f, 0.84f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_Header]                = ImVec4(0.00f, 0.47f, 0.84f, 0.31f);
    style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.00f, 0.47f, 0.84f, 0.80f);
    style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.00f, 0.47f, 0.84f, 1.00f);
    style.Colors[ImGuiCol_Separator]             = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
    style.Colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.14f, 0.44f, 0.80f, 0.78f);
    style.Colors[ImGuiCol_SeparatorActive]       = ImVec4(0.14f, 0.44f, 0.80f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.80f, 0.80f, 0.80f, 0.56f);
    style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    style.Colors[ImGuiCol_Tab]                   = ImVec4(0.76f, 0.80f, 0.84f, 0.93f);
    style.Colors[ImGuiCol_TabHovered]            = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    style.Colors[ImGuiCol_TabActive]             = ImVec4(0.60f, 0.73f, 0.88f, 1.00f);
    style.Colors[ImGuiCol_TabUnfocused]          = ImVec4(0.92f, 0.93f, 0.94f, 0.99f);
    style.Colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.74f, 0.82f, 0.91f, 1.00f);
    style.Colors[ImGuiCol_DockingPreview]        = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
    style.Colors[ImGuiCol_DockingEmptyBg]        = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    
    // Style properties
    style.WindowPadding     = ImVec2(8.0f, 8.0f);
    style.FramePadding      = ImVec2(5.0f, 3.0f);
    style.CellPadding       = ImVec2(6.0f, 6.0f);
    style.ItemSpacing       = ImVec2(6.0f, 6.0f);
    style.ItemInnerSpacing  = ImVec2(6.0f, 6.0f);
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 0.0f; // No border for frames
    style.ChildBorderSize   = 1.0f;
    style.WindowRounding    = 8.0f;
    style.ChildRounding     = 8.0f;
    style.FrameRounding     = 8.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding      = 8.0f;
    style.TabRounding       = 8.0f;
}
void SetLightTheme(){
    ImGuiStyle& style = ImGui::GetStyle();
  // Colors
    style.Colors[ImGuiCol_Text]         = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled]     = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    style.Colors[ImGuiCol_WindowBg]       = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    style.Colors[ImGuiCol_ChildBg]        = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_PopupBg]        = ImVec4(0.11f, 0.11f, 0.11f, 0.92f);
    style.Colors[ImGuiCol_Border]        = ImVec4(0.19f, 0.19f, 0.19f, 0.29f);
    style.Colors[ImGuiCol_BorderShadow]     = ImVec4(0.00f, 0.00f, 0.00f, 0.24f);
    style.Colors[ImGuiCol_FrameBg]        = ImVec4(0.20f, 0.20f, 0.20f, 0.54f);
    style.Colors[ImGuiCol_FrameBgHovered]    = ImVec4(0.25f, 0.25f, 0.25f, 0.54f);
    style.Colors[ImGuiCol_FrameBgActive]     = ImVec4(0.30f, 0.30f, 0.30f, 0.54f);
    style.Colors[ImGuiCol_TitleBg]        = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive]     = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed]   = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    style.Colors[ImGuiCol_MenuBarBg]       = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarBg]      = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    style.Colors[ImGuiCol_ScrollbarGrab]     = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    style.Colors[ImGuiCol_CheckMark]       = ImVec4(0.00f, 0.47f, 0.84f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab]      = ImVec4(0.34f, 0.34f, 0.34f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive]   = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    style.Colors[ImGuiCol_Button]        = ImVec4(0.00f, 0.47f, 0.84f, 0.60f); // Accent color
    style.Colors[ImGuiCol_ButtonHovered]     = ImVec4(0.00f, 0.47f, 0.84f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive]     = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_Header]        = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
    style.Colors[ImGuiCol_HeaderHovered]     = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    style.Colors[ImGuiCol_HeaderActive]     = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_Separator]       = style.Colors[ImGuiCol_Border];
    style.Colors[ImGuiCol_SeparatorHovered]   = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
    style.Colors[ImGuiCol_SeparatorActive]    = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip]      = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
    style.Colors[ImGuiCol_ResizeGripHovered]   = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    style.Colors[ImGuiCol_ResizeGripActive]   = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    style.Colors[ImGuiCol_Tab]          = ImVec4(0.18f, 0.35f, 0.58f, 0.86f);
    style.Colors[ImGuiCol_TabHovered]      = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    style.Colors[ImGuiCol_TabActive]       = ImVec4(0.20f, 0.41f, 0.68f, 1.00f);
    style.Colors[ImGuiCol_TabUnfocused]     = ImVec4(0.09f, 0.15f, 0.22f, 0.97f);
    style.Colors[ImGuiCol_TabUnfocusedActive]  = ImVec4(0.14f, 0.26f, 0.42f, 1.00f);
    style.Colors[ImGuiCol_DockingPreview]    = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
    style.Colors[ImGuiCol_DockingEmptyBg]    = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);


    // Style properties
    style.WindowPadding   = ImVec2(8.0f, 8.0f);
    style.FramePadding   = ImVec2(5.0f, 3.0f);
    style.CellPadding    = ImVec2(6.0f, 6.0f);
    style.ItemSpacing    = ImVec2(6.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize  = 1.0f;
    style.ChildBorderSize  = 1.0f;
    style.WindowRounding  = 8.0f;
    style.ChildRounding   = 8.0f;
    style.FrameRounding   = 8.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding   = 8.0f;
    style.TabRounding    = 8.0f;
}
namespace UI {

bool PillButton(const char* label, const ImVec2& size_arg) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImGui::CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    ImGui::ItemSize(size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, 0);

    float* anim_factor = ImGui::GetStateStorage()->GetFloatRef(id, 0.0f);
    *anim_factor = ImClamp(*anim_factor + (hovered || held ? 1.0f : -1.0f) * g.IO.DeltaTime * 8.f, 0.0f, 1.0f);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImVec4& col_bg_base = style.Colors[ImGuiCol_Button];
    const ImVec4& col_bg_hover = style.Colors[ImGuiCol_ButtonHovered];
    ImVec4 bg_color_v4 = ImLerp(col_bg_base, col_bg_hover, *anim_factor);

    draw_list->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(bg_color_v4), size.y / 2.0f);
    ImVec2 text_clip_min(bb.Min.x + style.FramePadding.x, bb.Min.y + style.FramePadding.y);
    ImVec2 text_clip_max(bb.Max.x - style.FramePadding.x, bb.Max.y - style.FramePadding.y);
    ImGui::RenderTextClipped(text_clip_min, text_clip_max, label, NULL, &label_size, style.ButtonTextAlign, &bb);

    if (hovered) ImGui::SetTooltip("Run Prompt");

    return pressed;
}

// A custom button with a gradient background and hover animation.
bool GradientButton(const char* label, const ImVec2& size = ImVec2(0, 0)) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 button_size = ImGui::CalcItemSize(size, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

    const ImRect bb(pos, ImVec2(pos.x + button_size.x, pos.y + button_size.y));
    ImGui::ItemSize(bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, 0);

    // --- Animation Logic ---
    // Get a persistent float value for the animation factor
    float* anim_factor = ImGui::GetStateStorage()->GetFloatRef(id, 0.0f);
    const float anim_speed = 0.08f;
    if (hovered) {
        *anim_factor = ImMin(1.0f, *anim_factor + anim_speed);
    } else {
        *anim_factor = ImMax(0.0f, *anim_factor - anim_speed);
    }
    // Ease-out function for a smoother effect
    float eased_factor = 1.0f - (1.0f - *anim_factor) * (1.0f - *anim_factor);

    // --- Drawing Logic ---
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    // Base colors from the current style
    const ImVec4& col_bg_base = style.Colors[ImGuiCol_Button];
    const ImVec4& col_bg_hover = style.Colors[ImGuiCol_ButtonHovered];
    // CORRECTED: Manual linear interpolation for colors
    auto lerp = [](const ImVec4& a, const ImVec4& b, float t) {
        return ImVec4(a.x + (b.x - a.x) * t,
                      a.y + (b.y - a.y) * t,
                      a.z + (b.z - a.z) * t,
                      a.w + (b.w - a.w) * t);
    };

    ImVec4 bg_color_v4 = lerp(col_bg_base, col_bg_hover, eased_factor);
    ImU32 bg_color = ImGui::ColorConvertFloat4ToU32(bg_color_v4);
    
    // Draw the rounded rectangle
    // The key change is here: use AddRectFilled and a high rounding value
    draw_list->AddRectFilled(bb.Min, bb.Max, bg_color, button_size.y / 2.0f);

    // Draw text centered
    ImVec2 text_pos = ImVec2(
        bb.Min.x + (button_size.x - label_size.x) / 2.0f,
        bb.Min.y + (button_size.y - label_size.y) / 2.0f
    );
    draw_list->AddText(text_pos, ImGui::GetColorU32(ImGuiCol_Text), label, label + strlen(label));
    
    // NEW: Add the tooltip on hover, just like Google AI Studio
    if (hovered) {
        ImGui::SetTooltip("Run Prompt");
    }

    return pressed;
}

void ActionInputBox(AppState& app_state) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const ImGuiStyle& style = ImGui::GetStyle();
    const float input_box_height = 50.0f; // Taller for a better feel
    
    ImVec2 pos = window->DC.CursorPos;
    pos.x = style.WindowPadding.x;
    pos.y = ImGui::GetWindowHeight() - input_box_height - style.WindowPadding.y * 2.0f;
    const float width = ImGui::GetWindowWidth() - style.WindowPadding.x * 2.0f;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // 1. Draw the drop shadow
    const ImU32 shadow_color = IM_COL32(0, 0, 0, 50);
    const float shadow_offset = 2.0f;
    const float shadow_blur = 8.0f;
    draw_list->AddRect(ImVec2(pos.x - shadow_offset, pos.y - shadow_offset), 
                       ImVec2(pos.x + width + shadow_offset, pos.y + input_box_height + shadow_offset), 
                       shadow_color, input_box_height / 2.0f, ImDrawFlags_None, shadow_blur);

    // 2. Draw the main container
    const ImU32 bg_color = IM_COL32(255, 255, 255, 255);
    const ImU32 border_color = IM_COL32(200, 200, 200, 255);
    draw_list->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + input_box_height), bg_color, input_box_height / 2.0f);
    draw_list->AddRect(pos, ImVec2(pos.x + width, pos.y + input_box_height), border_color, input_box_height / 2.0f);

    // 3. Position and draw the widgets inside
    const float button_width = 100.0f;
    const float internal_padding = 10.0f;
    const float text_input_width = width - button_width - internal_padding * 3.0f;

    // Position the text input
    ImGui::SetCursorScreenPos(ImVec2(pos.x + internal_padding, pos.y + (input_box_height - ImGui::GetTextLineHeightWithSpacing()) / 2.0f));
    ImGui::PushItemWidth(text_input_width);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0,0,0,0)); // Transparent background
    ImGui::InputText("##Prompt", app_state.prompt_buffer, sizeof(app_state.prompt_buffer));
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::PopItemWidth();

    // Position the button
    ImGui::SetCursorScreenPos(ImVec2(pos.x + width - button_width - internal_padding, pos.y + (input_box_height - 30.0f) / 2.0f));
    
    if (app_state.status == RequestStatus::SENDING) {
        ImGui::BeginDisabled();
        PillButton(ICON_FA_PAPER_PLANE " Run", ImVec2(button_width, 30.0f));
        ImGui::EndDisabled();
    } else {
        if (PillButton(ICON_FA_PAPER_PLANE " Run", ImVec2(button_width, 30.0f))) {
             app_state.status = RequestStatus::SENDING;
             app_state.request_sent_time = glfwGetTime();
             app_state.log_messages.push_back(std::string(ICON_FA_ARROW_UP) + " [Info] Send Prompt: " + std::string(app_state.prompt_buffer));
        }
    }
}

}

// Main code
int main(int, char**)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    #ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif

    GLFWwindow* window = glfwCreateWindow(1600, 900, "LLM 3D Control UI", nullptr, nullptr);
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

    // Load fonts and icons
    float baseFontSize = 18.0f;
    io.Fonts->AddFontFromFileTTF("fonts/Inter-Regular.ttf", baseFontSize);
    ImFontConfig config;
    config.MergeMode = true;
    config.PixelSnapH = true;
    config.GlyphMinAdvanceX = baseFontSize;
    static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    io.Fonts->AddFontFromFileTTF("fonts/Font Awesome 6 Free-Solid-900.otf", baseFontSize, &config, icon_ranges);
    
    static bool is_dark_mode = true;
    SetDarkTheme();


    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    // --- 3D Scene Setup ---
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
    // CORRECTED: Shader program object is now properly created and linked.
    GLuint shader_program = CreateShaderProgram(vertex_shader_source, fragment_shader_source);
    if (shader_program == 0) {
        // Shader creation failed, error message already printed.
        // It's better to exit gracefully.
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }
    
    GLuint VAO, VBO, EBO;
    CreateCubeVAO(VAO, VBO, EBO);

    Framebuffer viewport_fb; createFramebuffer(viewport_fb, 1, 1);
    AppState app_state; app_state.log_messages.push_back("[Info] Application started. Waiting for command.");

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem(is_dark_mode ? ICON_FA_SUN " Dark Mode " : ICON_FA_MOON " Light Mode ")) {
                    is_dark_mode = !is_dark_mode;
                    if (is_dark_mode) {
                        SetDarkTheme();
                    } else {
                        SetLightTheme();
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport()->ID);

        // --- UI Panels with new style ---
        ImGui::Begin(ICON_FA_TERMINAL " Controls & Commands");
        UI::ActionInputBox(app_state);
        
        /*ImGui::Text("Enter your command for the LLM:");
        ImGui::InputTextMultiline("##Prompt", app_state.prompt_buffer, sizeof(app_state.prompt_buffer), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 6));

        if (app_state.status == RequestStatus::SENDING) {
            // State: Processing - Show disabled button and spinner
            ImGui::BeginDisabled();
            UI::GradientButton(ICON_FA_PAPER_PLANE " Send Command");
            ImGui::EndDisabled();
            ImGui::SameLine();
            // This is the spinner, it will now be drawn correctly.
            ImSpinner::SpinnerPulsar("spinner_processing", 12.0f, 3.0f, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
            ImGui::SameLine(0.0f, 8.0f);
            ImGui::Text("Processing...");
        } else {
            // State: Idle - Show clickable button
            if (UI::GradientButton(ICON_FA_PAPER_PLANE " Send Command")) {
                app_state.status = RequestStatus::SENDING;
                app_state.request_sent_time = glfwGetTime();
                app_state.log_messages.push_back(std::string(ICON_FA_ARROW_UP) + " [Info] Sending prompt: " + std::string(app_state.prompt_buffer));
            }
        }*/
        ImGui::End();

        ImGui::Begin(ICON_FA_CLIPBOARD " LLM Command Log");
        for (const auto& msg : app_state.log_messages) { ImGui::TextUnformatted(msg.c_str()); }
        if (app_state.status == RequestStatus::SENDING) {
            if (glfwGetTime() - app_state.request_sent_time > 2.0) {
                 // Simulate a random outcome
                 if (rand() % 10 < 7) { 
                    app_state.log_messages.push_back(std::string(ICON_FA_CHECK) + " [Success] LLM responded. Executing: extrude(face=3, height=10).");             
                    ImGui::InsertNotification(ImGuiToast(ImGuiToastType::Success, 3000, ICON_FA_CHECK " Command Succeeded\nThe 3D model was updated."));
                 } else { 
                    app_state.log_messages.push_back(std::string(ICON_FA_TRIANGLE_EXCLAMATION) + " [Error] LLM failed to understand the command.");
                    ImGui::InsertNotification(ImGuiToast(ImGuiToastType::Error, 5000, ICON_FA_TRIANGLE_EXCLAMATION " Command Failed\nPlease rephrase your prompt."));
                 }
                 app_state.status = RequestStatus::IDLE;
            }
        }
        ImGui::End();
        
        // --- 3D Viewport with Camera Controls ---
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
        ImGui::Begin(ICON_FA_CUBE " 3D Viewport");
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

            // REVERTED: Using a static view matrix for stability
            float view_mat[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,-3.0f,1};
            
            float time_val = (float)glfwGetTime(); float angle_rad = time_val * 0.4f;
            float model_mat[16] = { cosf(angle_rad), 0, sinf(angle_rad), 0, 0, 1, 0, 0, -sinf(angle_rad), 0, cosf(angle_rad), 0, 0, 0, 0, 1};
            
            float fov_rad = 45.0f * 3.14159f / 180.0f; float aspect = (float)viewport_fb.width / (float)viewport_fb.height;
            float near_plane = 0.1f; float far_plane = 100.0f; float f = 1.0f / tanf(fov_rad / 2.0f);
            float proj_mat[16] = { f / aspect, 0, 0, 0, 0, f, 0, 0, 0, 0, (far_plane + near_plane) / (near_plane - far_plane), -1.0f, 0, 0, (2.0f * far_plane * near_plane) / (near_plane - far_plane), 0.0f };
            
            glUniformMatrix4fv(glGetUniformLocation(shader_program, "model"), 1, GL_FALSE, model_mat);
            glUniformMatrix4fv(glGetUniformLocation(shader_program, "view"), 1, GL_FALSE, view_mat);
            glUniformMatrix4fv(glGetUniformLocation(shader_program, "projection"), 1, GL_FALSE, proj_mat);
            
            // Draw the cube
            glBindVertexArray(VAO);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        ImGui::Image((void*)(intptr_t)viewport_fb.textureID, viewport_panel_size, ImVec2(0, 1), ImVec2(1, 0));
        ImGui::End();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.f); // Optional: Round the notifications
        ImGui::RenderNotifications();
        ImGui::PopStyleVar();

        // --- Final Rendering ---
        ImGui::Render();
        int display_w, display_h; glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
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

// Helper function definitions
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