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
static ImFont* G_Font_Regular = nullptr;
static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// --- Application State Management ---
enum class RequestStatus { IDLE, SENDING };
struct AppState {
    // 原有状态
    char prompt_buffer[1024] = "Make the cube twice as tall.";
    RequestStatus status = RequestStatus::IDLE;
    std::vector<std::string> log_messages;
    double request_sent_time = 0.0;

    // --- 新增状态用于LLM回复窗口 ---
    bool show_llm_response_window = true;  // 控制窗口是否显示
    bool is_generating_response = false;    // 是否处于“打字机”生成效果状态
    std::string llm_response_full_text;     // 存储从LLM收到的完整回复
    size_t typewriter_char_index = 0;       // “打字机”效果当前显示的字符索引
    double typewriter_start_time = 0.0;     // “打字机”效果的开始时间
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
    style.PopupRounding    = 8.0f;
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
    style.PopupRounding = 8.0f;
}
namespace UI {

bool PillButton(const char* label, const ImVec2& size_arg) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImGui::CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    ImGui::ItemSize(size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id)) return false;

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

void ActionInputBox(AppState& app_state) {
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    // --- 基本布局参数 ---
    const float container_width = ImGui::GetContentRegionAvail().x;
    const float internal_padding = 15.0f;
    const float button_width = 110.0f;
    const float button_height = 35.0f;
    
    // --- 定义最小/最大高度与动态高度计算 (用于外部容器) ---
    const float MIN_HEIGHT = 55.0f;
    const float MAX_HEIGHT = 200.0f;

    const float text_input_width = container_width - button_width - internal_padding * 3.0f;
    std::string calc_buffer = std::string(app_state.prompt_buffer) + " ";
    float text_height = ImGui::CalcTextSize(calc_buffer.c_str(), NULL, false, text_input_width).y;
    // 为容器的期望高度增加一些垂直内边距
    float desired_height = text_height + internal_padding * 2.0f + style.FramePadding.y * 2.0f;
    
    float target_height = ImClamp(desired_height, MIN_HEIGHT, MAX_HEIGHT);

    // --- 外部容器的动画高度 ---
    const ImGuiID height_id = ImGui::GetCurrentWindow()->GetID("##action_input_height");
    float* animated_height = ImGui::GetStateStorage()->GetFloatRef(height_id, MIN_HEIGHT);
    *animated_height = ImLerp(*animated_height, target_height, g.IO.DeltaTime * 10.0f);
    const float container_height = *animated_height;

    // --- 绘制与样式 (外部容器) ---
    const float rounding = 28.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, rounding);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(255, 255, 255, 255));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(210, 210, 210, 255));

    ImDrawList* parent_draw_list = ImGui::GetWindowDrawList();
    const ImVec2 container_pos = ImGui::GetCursorScreenPos();
    parent_draw_list->AddRect(container_pos, ImVec2(container_pos.x + container_width, container_pos.y + container_height), IM_COL32(0, 0, 0, 30), rounding, 0, 5.0f);
    
    // 外部容器永远不允许滚动
    ImGuiWindowFlags outer_child_flags = ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar;
    
    ImGui::BeginChild("ActionInputContainer", ImVec2(container_width, container_height), false, outer_child_flags);

    float text_area_height;
    float text_area_y_pos;
    
    if (desired_height <= MIN_HEIGHT) {
        text_area_height = g.FontSize + style.FramePadding.y * 2.0f;
        // 让整个TextInputScrollArea在容器中垂直居中
        text_area_y_pos = (container_height - text_area_height) / 2.0f;
    } else {
        text_area_height = container_height - internal_padding;
        text_area_y_pos = internal_padding / 2.0f;
    }

    // 设置TextInputScrollArea的位置（同时设置X和Y）
    ImGui::SetCursorPos(ImVec2(internal_padding, text_area_y_pos));

    // --- 1. 左侧的、可滚动的、隐形的文本区域 ---
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0,0,0,0)); // 透明背景
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f); // 无边框
    // 这个内部子窗口可以滚动
    ImGui::BeginChild("TextInputScrollArea", ImVec2(text_input_width, text_area_height), false, ImGuiWindowFlags_NoMove);
    {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32_BLACK_TRANS); // 输入框本身透明
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f)); // 输入框无内边距
        
        // 如果是单行情况，需要垂直居中文本
        if (desired_height <= MIN_HEIGHT) {
            // 计算单行文本的垂直居中位置
            float text_line_height = g.FontSize;
            float vertical_offset = (text_area_height - text_line_height) / 2.0f;
            ImGui::SetCursorPosY(vertical_offset);
        }
        
        // InputTextMultiline填满这个可滚动的子窗口
        ImGui::InputTextMultiline("##Prompt", app_state.prompt_buffer, sizeof(app_state.prompt_buffer), 
                                  ImGui::GetContentRegionAvail(), // 自动填满
                                  ImGuiInputTextFlags_None);
                                  
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();


    // --- 2. 右下角的固定按钮 ---
    float button_y_pos;
    if (desired_height <= MIN_HEIGHT) {
        button_y_pos = (container_height - button_height) / 2.0f;
    } else {
        button_y_pos = container_height - button_height - internal_padding;
    }
    ImGui::SetCursorPos(ImVec2(container_width - button_width - internal_padding, button_y_pos));
    
    if (app_state.status == RequestStatus::SENDING) {
        ImGui::BeginDisabled();
        PillButton(ICON_FA_PAPER_PLANE " Run", ImVec2(button_width, button_height));
        ImGui::EndDisabled();
    } else {
        if (PillButton(ICON_FA_PAPER_PLANE " Run", ImVec2(button_width, button_height))) {
            app_state.status = RequestStatus::SENDING;
            app_state.request_sent_time = glfwGetTime();
            app_state.log_messages.push_back(std::string(ICON_FA_ARROW_UP) + " [Info] Send Command: " + std::string(app_state.prompt_buffer));

            app_state.llm_response_full_text.clear(); 
        }
    }

    ImGui::EndChild(); // 结束 ActionInputContainer
    ImGui::PopStyleColor(2); // 弹出 ChildBg 和 Border 的颜色
    ImGui::PopStyleVar(2);   // 弹出 ChildRounding 和 ChildBorderSize 的样式
}

void GeminiLoadingSpinner(const char* id, float radius, float thickness, const ImU32& color) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return;

    ImGuiContext& g = *GImGui;
    const ImGuiID im_id = window->GetID(id);

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImGui::InvisibleButton(id, ImVec2(radius * 2, radius * 2));

    const ImVec2 center = ImVec2(pos.x + radius, pos.y + radius);
    float time = (float)g.Time;
    
    // 绘制旋转的圆弧 (逻辑不变)
    const float arc_start_angle_1 = time * 2.8f;
    const float arc_end_angle_1 = arc_start_angle_1 + IM_PI * 0.7f;
    draw_list->PathClear();
    draw_list->PathArcTo(center, radius, arc_start_angle_1, arc_end_angle_1, 32);
    draw_list->PathStroke(color, 0, thickness);

    const float arc_start_angle_2 = time * 2.8f + IM_PI;
    const float arc_end_angle_2 = arc_start_angle_2 + IM_PI * 0.7f;
    draw_list->PathClear();
    draw_list->PathArcTo(center, radius, arc_start_angle_2, arc_end_angle_2, 32);
    draw_list->PathStroke(color, 0, thickness);

    // 绘制中心脉动的四角星
    const int num_vertices = 8;
    ImVec2 points[num_vertices];
    float outer_radius = radius * 0.5f * (0.85f + 0.15f * sinf(time * 4.0f));

    // --- 关键修改：大幅减小内半径比例，让星星更锐利 ---
    float inner_radius = outer_radius * 0.7f; 

    for (int i = 0; i < 4; ++i) {
        float outer_angle = (i * IM_PI / 2.0f) + (IM_PI / 4.0f);
        points[i * 2] = ImVec2(center.x + outer_radius * cosf(outer_angle), 
                               center.y + outer_radius * sinf(outer_angle));
        float inner_angle = (i * IM_PI / 2.0f) + (IM_PI / 2.0f);
        points[i * 2 + 1] = ImVec2(center.x + inner_radius * cosf(inner_angle), 
                                   center.y + inner_radius * sinf(inner_angle));
    }
    draw_list->AddConvexPolyFilled(points, num_vertices, color);
}

void RenderLLMResponseWindow(AppState& app_state) {
    if (!app_state.show_llm_response_window) return;

    ImGui::SetNextWindowSize(ImVec2(450, 250), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("LLM Response", &app_state.show_llm_response_window)) {
        if (app_state.status == RequestStatus::SENDING) {
            // 状态一：正在等待回复，显示加载动画
            ImVec2 window_size = ImGui::GetWindowSize();
            // 计算一个半径为30px（总尺寸60x60）的动画的居中位置
            ImVec2 spinner_pos( (window_size.x - 60) / 2.0f, (window_size.y - 60) / 2.0f );
            ImGui::SetCursorPos(spinner_pos);
            // 使用之前居中版本的大尺寸参数
            GeminiLoadingSpinner("gemini_spinner", 40.0f, 4.0f, ImGui::GetColorU32(ImGuiCol_Button));

        } else if (app_state.is_generating_response) {
            // 状态二：收到回复，使用“打字机”效果显示
            const float chars_per_second = 80.0f;
            
            // FIXED: 统一使用 glfwGetTime() 来确保时间计算的准确性
            double elapsed_time = glfwGetTime() - app_state.typewriter_start_time;
            
            size_t chars_to_show = static_cast<size_t>(elapsed_time * chars_per_second);
            
            if (chars_to_show >= app_state.llm_response_full_text.length()) {
                // 打字机效果结束
                app_state.typewriter_char_index = app_state.llm_response_full_text.length();
                app_state.is_generating_response = false;
            } else {
                app_state.typewriter_char_index = chars_to_show;
            }

            // 为了避免substr在多字节字符（如中文）上出错，我们只在完全显示时使用完整字符串
            std::string displayed_text = app_state.is_generating_response 
                ? app_state.llm_response_full_text.substr(0, app_state.typewriter_char_index)
                : app_state.llm_response_full_text;

            ImGui::TextWrapped("%s", displayed_text.c_str());

            // 添加一个光标闪烁效果，增强“正在输入”的感觉
            if(app_state.is_generating_response) {
                ImGui::SameLine(0.0f, 0.0f);
                if (fmod(glfwGetTime(), 1.0) < 0.5) {
                    ImGui::TextUnformatted("_");
                }
            }


        } else {
            // 状态三：回复已完全显示
            ImGui::TextWrapped("%s", app_state.llm_response_full_text.c_str());
        }
    }
    ImGui::End();
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
    float baseFontSize = 24.0f;
    
    // 加载常规字体
    G_Font_Regular = io.Fonts->AddFontFromFileTTF("fonts/Inter-Regular.ttf", baseFontSize);
    if (!G_Font_Regular) {
        // 如果字体文件不存在，使用默认字体
        G_Font_Regular = io.Fonts->AddFontDefault();
    }
    
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
    AppState app_state; 
    app_state.log_messages.push_back("[Info] Application started. Waiting for command.");
    app_state.llm_response_full_text = "Welcome! I'm ready to help you modify the 3D scene. Please enter a command in the input box below.";


    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem(ICON_FA_FILE " New Scene", "Ctrl+N")) { /* Do something */ }
                if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Open Scene", "Ctrl+O")) { /* Do something */ }
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_FA_DOOR_OPEN " Exit")) { glfwSetWindowShouldClose(window, true); }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem(is_dark_mode ? ICON_FA_SUN " Light Mode" : ICON_FA_MOON " Dark Mode")) {
                    is_dark_mode = !is_dark_mode;
                    if (is_dark_mode) {
                        SetDarkTheme();
                    } else {
                        SetLightTheme();
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem(ICON_FA_CIRCLE_INFO " About")) { /* Do something */ }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport()->ID);

        // --- UI Panels with new style ---
        ImGui::Begin(ICON_FA_TERMINAL " Controls & Commands");
        UI::ActionInputBox(app_state);
        ImGui::End();

        ImGui::Begin(ICON_FA_CLIPBOARD " LLM Command Log");
        for (const auto& msg : app_state.log_messages) { ImGui::TextUnformatted(msg.c_str()); }
        if (app_state.status == RequestStatus::SENDING) {
            if (glfwGetTime() - app_state.request_sent_time > 2.0) { 
                if (rand() % 10 < 7) { 
                    app_state.log_messages.push_back(std::string(ICON_FA_CHECK) + " [Success] LLM responded. Executing: extrude(face=3, height=10).");
                    ImGui::InsertNotification(ImGuiToast(ImGuiToastType::Success, 3000, ICON_FA_CHECK " Command Succeeded\nThe 3D model was updated."));
                    
                    app_state.llm_response_full_text = "Okay, I've processed your request. Here are the steps I'll take:\n\n1.  Identify the top face of the cube.\n2.  Create a vector for the extrusion direction along the Y-axis.\n3.  Apply the extrusion operation to double the height.\n\nExecuting the command now on the 3D model.";

                } else { 
                    app_state.log_messages.push_back(std::string(ICON_FA_TRIANGLE_EXCLAMATION) + " [Error] LLM failed to understand the command.");
                    ImGui::InsertNotification(ImGuiToast(ImGuiToastType::Error, 5000, ICON_FA_TRIANGLE_EXCLAMATION " Command Failed\nPlease rephrase your prompt."));
                    app_state.llm_response_full_text = "I'm sorry, I couldn't understand that request. Could you please try rephrasing it? For example, try being more specific like 'Select the front face of the cube and move it forward by 2 units'.";
                }
                
                // 无论成功或失败，都开始生成回复
                app_state.status = RequestStatus::IDLE; // 停止主加载状态
                app_state.is_generating_response = true; // 开始“打字机”状态
                app_state.typewriter_char_index = 0;
                app_state.typewriter_start_time = glfwGetTime();
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

        UI::RenderLLMResponseWindow(app_state);

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