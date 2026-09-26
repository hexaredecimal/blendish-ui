//
// Blendish IDE - Visual Studio / Blender-Style IDE Workspace Example
// Featuring:
// - Top Menu Bar as a standard H-Box panel with flat label-rendered menus (not buttons)
// - Viewpickers with titles (Solution Config, Tabs Position, Editor Mode) inside the menu bar
// - Dynamic Tabs supporting Left, Right, Top, and Bottom orientations
// - Parent Splitter Panel with working Blender-style corner-drag splitting (duplicates editor into 2 panes)
// - Resizable middle splitter divider with drag & double-click reset
// - Fully editable, responsive TextArea with line numbers, caret navigation, typing, and clipboard
// - Solution Explorer project tree with clean vector chevrons and no forced default icons
// - Bottom Output / Terminal panel and rich Status Bar
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <algorithm>

#ifdef NANOVG_GLEW
#	include <GL/glew.h>
#endif
#ifdef __APPLE__
#	define GLFW_INCLUDE_GLCOREARB
#endif
#include <GLFW/glfw3.h>

#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg.h>
#include <nanovg_gl.h>

#define BLENDISH_IMPLEMENTATION
#include "blendish.h"

#define OUI_IMPLEMENTATION
#include "oui.h"

////////////////////////////////////////////////////////////////////////////////
// UI SubTypes & Data structures following Blendish/OUI conventions
////////////////////////////////////////////////////////////////////////////////

typedef enum {
    ST_LABEL = 0,
    ST_BUTTON,
    ST_RADIO,
    ST_SLIDER,
    ST_COLUMN,
    ST_ROW,
    ST_CHECK,
    ST_PANEL,
    ST_TEXT,
    ST_HBOX,
    ST_VBOX,
    ST_PROGRESSBAR,
    ST_TEXTAREA,
    ST_STATUSBAR,
    ST_TAB,
    ST_TREEITEM,
    ST_MENUHEADER,
    ST_VIEWPICKER,
    ST_SPLITTER_CORNER,
    ST_SPLITTER_DIVIDER
} SubType;

typedef struct {
    int subtype;
    UIhandler handler;
} UIData;

typedef struct {
    UIData head;
    int iconid;
    const char *label;
} UIButtonData;

typedef struct {
    UIData head;
    const char *label;
    int *option;
} UICheckData;

typedef struct {
    UIData head;
    int iconid;
    const char *label;
    int *value;
    int id;
} UIRadioData;

typedef struct {
    UIData head;
    const char *label;
    float *progress;
} UISliderData;

typedef struct {
    UIData head;
    char *text;
    int maxsize;
} UITextData;

typedef struct {
    UIData head;
    const char *label;
    float *progress;
    int flags;
} UIProgressBarData;

typedef struct {
    UIData head;
    char *text;
    int maxsize;
    int *cursor_pos;
    int *select_start;
    int *select_end;
    float *scroll_y;
    int show_line_numbers;
    int doc_id;
} UITextAreaData;

typedef struct {
    UIData head;
    int iconid;
    const char *status_text;
    const char *hints_text;
    const char *stats_text;
} UIStatusBarData;

typedef struct {
    UIData head;
    int iconid;
    const char *label;
    int tab_id;
    int *active_tab;
    int position;
} UITabData;

typedef struct {
    UIData head;
    int depth;
    int iconid;
    const char *label;
    int *is_open;
    int is_leaf;
    int *is_selected;
    int *view_flag;
    int *select_flag;
    int *render_flag;
    int node_id;
} UITreeItemData;

typedef struct {
    UIData head;
    int iconid;
    const char *title;
    const char *value;
    UIhandler on_click;
} UIViewPickerData;

typedef struct {
    UIData head;
    int pane_id;
} UISplitterCornerData;

typedef struct {
    UIData head;
    int is_vertical;
} UISplitterDividerData;

////////////////////////////////////////////////////////////////////////////////
// Application State & Document Buffers
////////////////////////////////////////////////////////////////////////////////

static GLFWwindow *g_window = NULL;
static NVGcontext *g_vg = NULL;

// Multiple Document Buffers
typedef struct {
    const char *name;
    const char *path;
    char buffer[4096];
    int cursor_pos;
    int select_start;
    int select_end;
    float scroll_y;
    int line_numbers;
} Document;

static Document g_docs[] = {
    {
        "main.cpp", "BlendishIDE/src/main.cpp",
        "// Blendish IDE - Visual Studio Workspace\n"
        "#include <iostream>\n"
        "#include <vector>\n"
        "#include \"blendish.h\"\n"
        "\n"
        "int main(int argc, char **argv) {\n"
        "    std::cout << \"Blendish IDE Initialized!\" << std::endl;\n"
        "    std::vector<std::string> tools = {\n"
        "        \"Editor\", \"Solution Explorer\", \"Splitter\"\n"
        "    };\n"
        "    for (const auto &t : tools) {\n"
        "        std::cout << \"  [Ready] \" << t << std::endl;\n"
        "    }\n"
        "    return 0;\n"
        "}\n",
        35, -1, -1, 0.0f, 1
    },
    {
        "blendish.h", "BlendishIDE/include/blendish.h",
        "/*\n"
        " * Blendish - Blender 2.5 UI Theme & Widgets\n"
        " * Components: Tabs, Splitter, Menus, TextArea\n"
        " */\n"
        "#ifndef BLENDISH_H\n"
        "#define BLENDISH_H\n"
        "\n"
        "typedef enum BNDtabPosition {\n"
        "    BND_TAB_TOP = 0,\n"
        "    BND_TAB_BOTTOM = 1,\n"
        "    BND_TAB_LEFT = 2,\n"
        "    BND_TAB_RIGHT = 3\n"
        "} BNDtabPosition;\n"
        "\n"
        "#endif // BLENDISH_H\n",
        10, -1, -1, 0.0f, 1
    },
    {
        "app_config.json", "BlendishIDE/config/app_config.json",
        "{\n"
        "  \"workspace\": \"Visual Studio Theme\",\n"
        "  \"editor\": {\n"
        "    \"tab_size\": 4,\n"
        "    \"line_numbers\": true,\n"
        "    \"word_wrap\": false\n"
        "  },\n"
        "  \"splitter\": {\n"
        "    \"mode\": \"blender_corner_drag\",\n"
        "    \"default_ratio\": 0.5\n"
        "  }\n"
        "}\n",
        15, -1, -1, 0.0f, 1
    },
    {
        "shader.frag", "BlendishIDE/shaders/shader.frag",
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "in vec2 TexCoords;\n"
        "uniform sampler2D texture_diffuse;\n"
        "\n"
        "void main() {\n"
        "    FragColor = texture(texture_diffuse, TexCoords);\n"
        "}\n",
        20, -1, -1, 0.0f, 1
    }
};
static const int g_doc_count = sizeof(g_docs) / sizeof(g_docs[0]);

// Active document in Pane 1 and Pane 2
static int g_active_doc_pane1 = 0;
static int g_active_doc_pane2 = 1;
static int g_focused_pane = 1; // 1 or 2

// Tabs positioning (BND_TAB_TOP, BND_TAB_BOTTOM, BND_TAB_LEFT, BND_TAB_RIGHT)
static int g_tab_position = BND_TAB_TOP;
static const char *g_tab_pos_names[] = { "Top", "Bottom", "Left", "Right" };

// Splitter state
static int g_is_split = 0;          // 0 = single editor, 1 = dual split
static int g_split_type = 0;        // 0 = Horizontal (left/right panes), 1 = Vertical (top/bottom panes)
static float g_split_ratio = 0.50f; // divider position ratio 0.15 - 0.85

// Corner dragging state for Blender-style split
static int g_dragging_corner = 0;
static int g_drag_source_pane = 1;
static float g_drag_start_x = 0.0f;
static float g_drag_start_y = 0.0f;
static float g_drag_cur_x = 0.0f;
static float g_drag_cur_y = 0.0f;

// Divider dragging state
static int g_dragging_divider = 0;

// Configuration choice (Debug x64, Release x64, etc.)
static int g_config_choice = 0;
static const char *g_config_names[] = { "Debug (x64)", "Release (x64)", "Debug (ARM64)", "Release (ARM64)" };

// Left Sidebar Tab & Bottom Panel Tab
static int g_sidebar_tab = 0; // 0 = Solution Explorer, 1 = Git Changes, 2 = Toolbox
static int g_bottom_tab = 0;  // 0 = Output, 1 = Terminal, 2 = Error List

// Output window text
static char g_output_buffer[2048] = 
    "1>------ Build started: Project: BlendishIDE, Configuration: Debug x64 ------\n"
    "1>  Compiling main.cpp...\n"
    "1>  Linking BlendishIDE.exe...\n"
    "1>  BlendishIDE.exe successfully compiled.\n"
    "========== Build: 1 succeeded, 0 failed, 0 up-to-date, 0 skipped ==========\n";

// Status message
static char g_status_msg[128] = "Ready";

////////////////////////////////////////////////////////////////////////////////
// Solution Explorer Tree Structure (No forced default icons!)
////////////////////////////////////////////////////////////////////////////////

typedef struct SolutionNode {
    int id;
    int depth;
    int iconid; // -1 for no icon
    const char *label;
    int is_open;
    int is_leaf;
    int is_selected;
    int target_doc; // -1 or index in g_docs
} SolutionNode;

static SolutionNode g_solution_tree[] = {
    { 0, 0, BND_ICON_PACKAGE,    "Solution 'BlendishIDE' (1 project)", 1, 0, 0, -1 },
    { 1, 1, BND_ICON_GROUP,      "BlendishCore (v1.4)",                1, 0, 0, -1 },
    { 2, 2, BND_ICON_FILE_FOLDER,"Header Files",                       1, 0, 0, -1 },
    { 3, 3, BND_ICON_FILE_TEXT,  "blendish.h",                         1, 1, 0, 1  },
    { 4, 3, BND_ICON_FILE_TEXT,  "oui.h",                              1, 1, 0, -1 },
    { 5, 2, BND_ICON_FILE_FOLDER,"Source Files",                       1, 0, 0, -1 },
    { 6, 3, BND_ICON_FILE_TEXT,  "main.cpp",                           1, 1, 1, 0  },
    { 7, 2, BND_ICON_FILE_FOLDER,"Resources",                          1, 0, 0, -1 },
    { 8, 3, BND_ICON_FILE_TEXT,  "app_config.json",                    1, 1, 0, 2  },
    { 9, 3, BND_ICON_FILE_TEXT,  "shader.frag",                        1, 1, 0, 3  }
};
static const int g_solution_tree_count = sizeof(g_solution_tree) / sizeof(g_solution_tree[0]);

////////////////////////////////////////////////////////////////////////////////
// Dropdown Menu System & Data
////////////////////////////////////////////////////////////////////////////////

typedef enum {
    MENU_NONE = 0,
    MENU_FILE,
    MENU_EDIT,
    MENU_VIEW,
    MENU_PROJECT,
    MENU_BUILD,
    MENU_DEBUG,
    MENU_TOOLS,
    MENU_HELP,
    MENU_CONFIG_PICKER,
    MENU_TABS_PICKER,
    MENU_SPLIT_PICKER,
    MENU_CONTEXT
} ActiveMenuType;

static ActiveMenuType active_menu = MENU_NONE;
static float menu_popup_x = 0.0f;
static float menu_popup_y = 0.0f;
static float context_menu_x = 0.0f;
static float context_menu_y = 0.0f;

typedef struct MenuItemDef {
    int iconid; // -1 for clean text without default icons
    const char *label;
    const char *shortcut;
    int is_checked;
    int is_submenu;
    int is_disabled;
    int is_separator;
    int action_id;
} MenuItemDef;

static MenuItemDef file_menu_items[] = {
    { -1, "New File...",            "Ctrl+N",       0, 0, 0, 0, 101 },
    { -1, "Open File...",           "Ctrl+O",       0, 0, 0, 0, 102 },
    { -1, "Open Folder...",         "Ctrl+K Ctrl+O",0, 0, 0, 0, 103 },
    { -1, "",                       NULL,           0, 0, 0, 1, 0   },
    { -1, "Save",                   "Ctrl+S",       0, 0, 0, 0, 104 },
    { -1, "Save All",               "Ctrl+Shift+S", 0, 0, 0, 0, 105 },
    { -1, "",                       NULL,           0, 0, 0, 1, 0   },
    { -1, "Close Solution",         NULL,           0, 0, 0, 0, 106 },
    { -1, "Exit",                   "Alt+F4",       0, 0, 0, 0, 107 }
};

static MenuItemDef edit_menu_items[] = {
    { -1, "Undo",                   "Ctrl+Z",       0, 0, 0, 0, 201 },
    { -1, "Redo",                   "Ctrl+Y",       0, 0, 0, 0, 202 },
    { -1, "",                       NULL,           0, 0, 0, 1, 0   },
    { -1, "Cut",                    "Ctrl+X",       0, 0, 0, 0, 203 },
    { -1, "Copy",                   "Ctrl+C",       0, 0, 0, 0, 204 },
    { -1, "Paste",                  "Ctrl+V",       0, 0, 0, 0, 205 },
    { -1, "",                       NULL,           0, 0, 0, 1, 0   },
    { -1, "Select All",             "Ctrl+A",       0, 0, 0, 0, 206 },
    { -1, "Toggle Line Comment",    "Ctrl+/",       0, 0, 0, 0, 207 }
};

static MenuItemDef view_menu_items[] = {
    { -1, "Solution Explorer",      "Ctrl+Alt+L",   1, 0, 0, 0, 301 },
    { -1, "Output Window",          "Ctrl+Alt+O",   1, 0, 0, 0, 302 },
    { -1, "Terminal",               "Ctrl+`",       0, 0, 0, 0, 303 },
    { -1, "",                       NULL,           0, 0, 0, 1, 0   },
    { -1, "Split Editor Right",     "Ctrl+\\",      0, 0, 0, 0, 304 },
    { -1, "Split Editor Down",      "Ctrl+K Ctrl+\\",0, 0, 0, 0, 305 },
    { -1, "Single Editor View",     NULL,           0, 0, 0, 0, 306 },
    { -1, "",                       NULL,           0, 0, 0, 1, 0   },
    { -1, "Tab Position: Top",      NULL,           0, 0, 0, 0, 310 },
    { -1, "Tab Position: Bottom",   NULL,           0, 0, 0, 0, 311 },
    { -1, "Tab Position: Left",     NULL,           0, 0, 0, 0, 312 },
    { -1, "Tab Position: Right",    NULL,           0, 0, 0, 0, 313 }
};

static MenuItemDef build_menu_items[] = {
    { -1, "Build Solution",         "Ctrl+Shift+B", 0, 0, 0, 0, 401 },
    { -1, "Rebuild Solution",       NULL,           0, 0, 0, 0, 402 },
    { -1, "Clean Solution",         NULL,           0, 0, 0, 0, 403 },
    { -1, "Batch Build...",         NULL,           0, 0, 0, 0, 404 }
};

static MenuItemDef debug_menu_items[] = {
    { -1, "Start Debugging",        "F5",           0, 0, 0, 0, 501 },
    { -1, "Start Without Debugging","Ctrl+F5",      0, 0, 0, 0, 502 },
    { -1, "Step Into",              "F11",          0, 0, 0, 0, 503 },
    { -1, "Step Over",              "F10",          0, 0, 0, 0, 504 },
    { -1, "Stop Debugging",         "Shift+F5",     0, 0, 0, 0, 505 }
};

static MenuItemDef help_menu_items[] = {
    { -1, "View Documentation",     "F1",           0, 0, 0, 0, 601 },
    { -1, "Keyboard Shortcuts",     NULL,           0, 0, 0, 0, 602 },
    { -1, "",                       NULL,           0, 0, 0, 1, 0   },
    { -1, "About Blendish IDE",     NULL,           0, 0, 0, 0, 603 }
};

static MenuItemDef context_menu_items[] = {
    { -1, "Cut",                    "Ctrl+X",       0, 0, 0, 0, 203 },
    { -1, "Copy",                   "Ctrl+C",       0, 0, 0, 0, 204 },
    { -1, "Paste",                  "Ctrl+V",       0, 0, 0, 0, 205 },
    { -1, "",                       NULL,           0, 0, 0, 1, 0   },
    { -1, "Split Editor Right",     NULL,           0, 0, 0, 0, 304 },
    { -1, "Split Editor Down",      NULL,           0, 0, 0, 0, 305 },
    { -1, "Close Split Pane",       NULL,           0, 0, 0, 0, 306 }
};

static MenuItemDef config_picker_items[] = {
    { -1, "Debug (x64)",            NULL,           0, 0, 0, 0, 700 },
    { -1, "Release (x64)",          NULL,           0, 0, 0, 0, 701 },
    { -1, "Debug (ARM64)",          NULL,           0, 0, 0, 0, 702 },
    { -1, "Release (ARM64)",        NULL,           0, 0, 0, 0, 703 }
};

static MenuItemDef tabs_picker_items[] = {
    { -1, "Top",                    NULL,           0, 0, 0, 0, 800 },
    { -1, "Bottom",                 NULL,           0, 0, 0, 0, 801 },
    { -1, "Left",                   NULL,           0, 0, 0, 0, 802 },
    { -1, "Right",                  NULL,           0, 0, 0, 0, 803 }
};

static MenuItemDef split_picker_items[] = {
    { -1, "Single Editor",          NULL,           0, 0, 0, 0, 900 },
    { -1, "Horizontal Split (Side by Side)", NULL,  0, 0, 0, 0, 901 },
    { -1, "Vertical Split (Stacked)", NULL,         0, 0, 0, 0, 902 }
};

////////////////////////////////////////////////////////////////////////////////
// Menu Action Dispatcher
////////////////////////////////////////////////////////////////////////////////

void execute_menu_action(int action_id) {
    switch(action_id) {
        case 104: // Save
            sprintf(g_status_msg, "Saved active document: %s", g_docs[g_focused_pane == 1 ? g_active_doc_pane1 : g_active_doc_pane2].name);
            break;
        case 105: // Save All
            sprintf(g_status_msg, "Saved all open documents");
            break;
        case 304: // Split Right (Horizontal split)
            g_is_split = 1;
            g_split_type = 0;
            g_active_doc_pane2 = (g_active_doc_pane1 + 1) % g_doc_count;
            sprintf(g_status_msg, "Editor split horizontally into 2 side-by-side panes");
            break;
        case 305: // Split Down (Vertical split)
            g_is_split = 1;
            g_split_type = 1;
            g_active_doc_pane2 = (g_active_doc_pane1 + 1) % g_doc_count;
            sprintf(g_status_msg, "Editor split vertically into 2 stacked panes");
            break;
        case 306: // Single View
            g_is_split = 0;
            sprintf(g_status_msg, "Editor returned to single view");
            break;
        case 310: case 800:
            g_tab_position = BND_TAB_TOP;
            sprintf(g_status_msg, "Document Tabs position set to TOP");
            break;
        case 311: case 801:
            g_tab_position = BND_TAB_BOTTOM;
            sprintf(g_status_msg, "Document Tabs position set to BOTTOM");
            break;
        case 312: case 802:
            g_tab_position = BND_TAB_LEFT;
            sprintf(g_status_msg, "Document Tabs position set to LEFT");
            break;
        case 313: case 803:
            g_tab_position = BND_TAB_RIGHT;
            sprintf(g_status_msg, "Document Tabs position set to RIGHT");
            break;
        case 401: // Build
        case 501: // Debug Start
            sprintf(g_status_msg, "Build started... All targets up to date.");
            strcat(g_output_buffer, "1> Build started: BlendishIDE (Debug x64) -> All targets built cleanly.\n");
            break;
        case 700: g_config_choice = 0; sprintf(g_status_msg, "Configuration changed: Debug (x64)"); break;
        case 701: g_config_choice = 1; sprintf(g_status_msg, "Configuration changed: Release (x64)"); break;
        case 702: g_config_choice = 2; sprintf(g_status_msg, "Configuration changed: Debug (ARM64)"); break;
        case 703: g_config_choice = 3; sprintf(g_status_msg, "Configuration changed: Release (ARM64)"); break;
        case 900: g_is_split = 0; sprintf(g_status_msg, "Editor set to Single View"); break;
        case 901: g_is_split = 1; g_split_type = 0; sprintf(g_status_msg, "Editor split horizontally (side by side)"); break;
        case 902: g_is_split = 1; g_split_type = 1; sprintf(g_status_msg, "Editor split vertically (stacked)"); break;
        default:
            sprintf(g_status_msg, "Action %d executed", action_id);
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Text Area Event Handler: Fully Interactive & Editable
////////////////////////////////////////////////////////////////////////////////

void ide_textarea_handler(int item, UIevent event) {
    UITextAreaData *data = (UITextAreaData *)uiGetHandle(item);
    if (!data) return;

    switch(event) {
        case UI_BUTTON0_DOWN: {
            uiFocus(item);
            g_focused_pane = (data->doc_id == g_active_doc_pane2 && g_is_split) ? 2 : 1;

            UIvec2 cursor = uiGetCursor();
            UIrect rect = uiGetRect(item);

            int pos = bndTextAreaTextPosition(g_vg, (float)rect.x, (float)rect.y,
                (float)rect.w, (float)rect.h, data->text, *data->scroll_y,
                data->show_line_numbers, cursor.x, cursor.y);

            *data->cursor_pos = pos;
            *data->select_start = pos;
            *data->select_end = pos;

            // Compute line and column for status
            int line = 1, col = 1;
            for (int i = 0; i < pos && data->text[i]; ++i) {
                if (data->text[i] == '\n') { line++; col = 1; }
                else col++;
            }
            sprintf(g_status_msg, "Cursor: Ln %d, Col %d", line, col);
        } break;

        case UI_BUTTON0_CAPTURE: {
            // Dragging to select text
            UIvec2 cursor = uiGetCursor();
            UIrect rect = uiGetRect(item);
            int pos = bndTextAreaTextPosition(g_vg, (float)rect.x, (float)rect.y,
                (float)rect.w, (float)rect.h, data->text, *data->scroll_y,
                data->show_line_numbers, cursor.x, cursor.y);
            *data->cursor_pos = pos;
            *data->select_end = pos;
        } break;

        case UI_KEY_DOWN: {
            unsigned int key = uiGetKey();
            unsigned int mod = uiGetModifier();
            int len = strlen(data->text);
            int cp = *data->cursor_pos;
            if (cp < 0) cp = 0;
            if (cp > len) cp = len;

            int s_start = *data->select_start;
            int s_end = *data->select_end;
            if (s_start > s_end) std::swap(s_start, s_end);
            int has_sel = (s_start >= 0 && s_end > s_start);

            // Clipboard Ctrl+A, Ctrl+C, Ctrl+V, Ctrl+X
            if (mod & GLFW_MOD_CONTROL) {
                if (key == GLFW_KEY_A) {
                    *data->select_start = 0;
                    *data->select_end = len;
                    *data->cursor_pos = len;
                    return;
                } else if (key == GLFW_KEY_C && has_sel && g_window) {
                    char sel_buf[2048];
                    int sel_len = s_end - s_start;
                    if (sel_len > 2047) sel_len = 2047;
                    strncpy(sel_buf, data->text + s_start, sel_len);
                    sel_buf[sel_len] = 0;
                    glfwSetClipboardString(g_window, sel_buf);
                    sprintf(g_status_msg, "Copied %d characters to clipboard", sel_len);
                    return;
                } else if (key == GLFW_KEY_X && has_sel && g_window) {
                    char sel_buf[2048];
                    int sel_len = s_end - s_start;
                    if (sel_len > 2047) sel_len = 2047;
                    strncpy(sel_buf, data->text + s_start, sel_len);
                    sel_buf[sel_len] = 0;
                    glfwSetClipboardString(g_window, sel_buf);
                    memmove(data->text + s_start, data->text + s_end, len - s_end + 1);
                    *data->cursor_pos = s_start;
                    *data->select_start = -1;
                    *data->select_end = -1;
                    sprintf(g_status_msg, "Cut %d characters to clipboard", sel_len);
                    return;
                } else if (key == GLFW_KEY_V && g_window) {
                    const char *clip = glfwGetClipboardString(g_window);
                    if (clip && *clip) {
                        if (has_sel) {
                            memmove(data->text + s_start, data->text + s_end, len - s_end + 1);
                            cp = s_start;
                            len = strlen(data->text);
                        }
                        int clen = strlen(clip);
                        if (len + clen < data->maxsize - 1) {
                            memmove(data->text + cp + clen, data->text + cp, len - cp + 1);
                            memcpy(data->text + cp, clip, clen);
                            *data->cursor_pos = cp + clen;
                            *data->select_start = -1;
                            *data->select_end = -1;
                            sprintf(g_status_msg, "Pasted %d characters", clen);
                        }
                    }
                    return;
                }
            }

            switch(key) {
                case GLFW_KEY_BACKSPACE: {
                    if (has_sel) {
                        memmove(data->text + s_start, data->text + s_end, len - s_end + 1);
                        *data->cursor_pos = s_start;
                        *data->select_start = -1;
                        *data->select_end = -1;
                    } else if (cp > 0 && len > 0) {
                        memmove(data->text + cp - 1, data->text + cp, len - cp + 1);
                        *data->cursor_pos = cp - 1;
                    }
                } break;

                case GLFW_KEY_DELETE: {
                    if (has_sel) {
                        memmove(data->text + s_start, data->text + s_end, len - s_end + 1);
                        *data->cursor_pos = s_start;
                        *data->select_start = -1;
                        *data->select_end = -1;
                    } else if (cp < len) {
                        memmove(data->text + cp, data->text + cp + 1, len - cp);
                    }
                } break;

                case GLFW_KEY_ENTER: {
                    if (has_sel) {
                        memmove(data->text + s_start, data->text + s_end, len - s_end + 1);
                        cp = s_start;
                        len = strlen(data->text);
                    }
                    if (len < data->maxsize - 2) {
                        memmove(data->text + cp + 1, data->text + cp, len - cp + 1);
                        data->text[cp] = '\n';
                        *data->cursor_pos = cp + 1;
                        *data->select_start = -1;
                        *data->select_end = -1;
                    }
                } break;

                case GLFW_KEY_TAB: {
                    // Insert 4 spaces
                    if (len + 4 < data->maxsize - 1) {
                        memmove(data->text + cp + 4, data->text + cp, len - cp + 1);
                        memcpy(data->text + cp, "    ", 4);
                        *data->cursor_pos = cp + 4;
                        *data->select_start = -1;
                        *data->select_end = -1;
                    }
                } break;

                case GLFW_KEY_LEFT: {
                    if (cp > 0) *data->cursor_pos = cp - 1;
                    *data->select_start = -1;
                    *data->select_end = -1;
                } break;

                case GLFW_KEY_RIGHT: {
                    if (cp < len) *data->cursor_pos = cp + 1;
                    *data->select_start = -1;
                    *data->select_end = -1;
                } break;

                case GLFW_KEY_UP: {
                    // Move to previous line
                    int line_start = cp;
                    while (line_start > 0 && data->text[line_start - 1] != '\n') line_start--;
                    int col = cp - line_start;
                    if (line_start > 0) {
                        int prev_end = line_start - 1;
                        int prev_start = prev_end;
                        while (prev_start > 0 && data->text[prev_start - 1] != '\n') prev_start--;
                        int prev_len = prev_end - prev_start;
                        *data->cursor_pos = prev_start + (col < prev_len ? col : prev_len);
                    }
                    *data->select_start = -1;
                    *data->select_end = -1;
                } break;

                case GLFW_KEY_DOWN: {
                    // Move to next line
                    int line_start = cp;
                    while (line_start > 0 && data->text[line_start - 1] != '\n') line_start--;
                    int col = cp - line_start;
                    int line_end = cp;
                    while (line_end < len && data->text[line_end] != '\n') line_end++;
                    if (line_end < len) {
                        int next_start = line_end + 1;
                        int next_end = next_start;
                        while (next_end < len && data->text[next_end] != '\n') next_end++;
                        int next_len = next_end - next_start;
                        *data->cursor_pos = next_start + (col < next_len ? col : next_len);
                    }
                    *data->select_start = -1;
                    *data->select_end = -1;
                } break;

                case GLFW_KEY_HOME: {
                    while (cp > 0 && data->text[cp - 1] != '\n') cp--;
                    *data->cursor_pos = cp;
                    *data->select_start = -1;
                    *data->select_end = -1;
                } break;

                case GLFW_KEY_END: {
                    while (cp < len && data->text[cp] != '\n') cp++;
                    *data->cursor_pos = cp;
                    *data->select_start = -1;
                    *data->select_end = -1;
                } break;
            }

            // Update status line/col
            int cur_cp = *data->cursor_pos;
            int line = 1, col = 1;
            for (int i = 0; i < cur_cp && data->text[i]; ++i) {
                if (data->text[i] == '\n') { line++; col = 1; }
                else col++;
            }
            sprintf(g_status_msg, "Cursor: Ln %d, Col %d", line, col);
        } break;

        case UI_CHAR: {
            unsigned int ch = uiGetKey();
            if (ch >= 32 && ch <= 126) {
                int len = strlen(data->text);
                int cp = *data->cursor_pos;
                if (cp < 0) cp = 0;
                if (cp > len) cp = len;

                int s_start = *data->select_start;
                int s_end = *data->select_end;
                if (s_start > s_end) std::swap(s_start, s_end);
                if (s_start >= 0 && s_end > s_start) {
                    memmove(data->text + s_start, data->text + s_end, len - s_end + 1);
                    cp = s_start;
                    len = strlen(data->text);
                }

                if (len < data->maxsize - 2) {
                    memmove(data->text + cp + 1, data->text + cp, len - cp + 1);
                    data->text[cp] = (char)ch;
                    *data->cursor_pos = cp + 1;
                    *data->select_start = -1;
                    *data->select_end = -1;
                }
            }
        } break;

        case UI_SCROLL: {
            UIvec2 scroll = uiGetScroll();
            *data->scroll_y -= scroll.y * 18.0f;
            if (*data->scroll_y < 0.0f) *data->scroll_y = 0.0f;
        } break;

        default: break;
    }
}

////////////////////////////////////////////////////////////////////////////////
// UI Item Constructors
////////////////////////////////////////////////////////////////////////////////

int panel() {
    int item = uiItem();
    UIData *data = (UIData *)uiAllocHandle(item, sizeof(UIData));
    data->subtype = ST_PANEL;
    data->handler = NULL;
    uiSetLayout(item, UI_LEFT | UI_TOP | UI_FILL);
    return item;
}

int label(int iconid, const char *text) {
    int item = uiItem();
    uiSetSize(item, 0, BND_WIDGET_HEIGHT);
    uiSetLayout(item, UI_LEFT | UI_TOP | UI_HFILL);
    UIButtonData *data = (UIButtonData *)uiAllocHandle(item, sizeof(UIButtonData));
    data->head.subtype = ST_LABEL;
    data->head.handler = NULL;
    data->iconid = iconid;
    data->label = text;
    return item;
}

int button(int iconid, const char *text, UIhandler handler) {
    int item = uiItem();
    uiSetSize(item, 0, BND_WIDGET_HEIGHT);
    uiSetLayout(item, UI_LEFT | UI_TOP);
    uiSetEvents(item, UI_BUTTON0_DOWN);
    UIButtonData *data = (UIButtonData *)uiAllocHandle(item, sizeof(UIButtonData));
    data->head.subtype = ST_BUTTON;
    data->head.handler = handler;
    data->iconid = iconid;
    data->label = text;
    return item;
}

int menu_header_label(const char *text, UIhandler handler) {
    int item = uiItem();
    int w = bndLabelWidth(g_vg, -1, text) + 16;
    uiSetSize(item, w, 24);
    uiSetEvents(item, UI_BUTTON0_DOWN);
    UIButtonData *data = (UIButtonData *)uiAllocHandle(item, sizeof(UIButtonData));
    data->head.subtype = ST_MENUHEADER;
    data->head.handler = handler;
    data->iconid = -1;
    data->label = text;
    return item;
}

int viewpicker(int iconid, const char *title, const char *value, UIhandler handler) {
    int item = uiItem();
    int w = (title ? bndLabelWidth(g_vg, -1, title) : 0) + bndLabelWidth(g_vg, -1, value) + 32;
    if (w < 110) w = 110;
    uiSetSize(item, w, 22);
    uiSetEvents(item, UI_BUTTON0_DOWN);
    UIViewPickerData *data = (UIViewPickerData *)uiAllocHandle(item, sizeof(UIViewPickerData));
    data->head.subtype = ST_VIEWPICKER;
    data->head.handler = handler;
    data->iconid = iconid;
    data->title = title;
    data->value = value;
    data->on_click = handler;
    return item;
}

int tab_item(int iconid, const char *label, int tab_id, int *active_tab, int position, UIhandler handler) {
    int item = uiItem();
    if (position == BND_TAB_LEFT || position == BND_TAB_RIGHT) {
        uiSetSize(item, 100, BND_WIDGET_HEIGHT + 2);
    } else {
        int w = bndLabelWidth(g_vg, iconid, label) + 20;
        uiSetSize(item, w, BND_WIDGET_HEIGHT + 2);
    }
    uiSetEvents(item, UI_BUTTON0_DOWN);
    UITabData *data = (UITabData *)uiAllocHandle(item, sizeof(UITabData));
    data->head.subtype = ST_TAB;
    data->head.handler = handler;
    data->iconid = iconid;
    data->label = label;
    data->tab_id = tab_id;
    data->active_tab = active_tab;
    data->position = position;
    return item;
}

int treeitem(int depth, int iconid, const char *label, int *is_open, int is_leaf, int *is_selected, int node_id, UIhandler handler) {
    int item = uiItem();
    uiSetSize(item, 0, 20);
    uiSetEvents(item, UI_BUTTON0_DOWN);
    UITreeItemData *data = (UITreeItemData *)uiAllocHandle(item, sizeof(UITreeItemData));
    data->head.subtype = ST_TREEITEM;
    data->head.handler = handler;
    data->depth = depth;
    data->iconid = iconid;
    data->label = label;
    data->is_open = is_open;
    data->is_leaf = is_leaf;
    data->is_selected = is_selected;
    data->view_flag = NULL;
    data->select_flag = NULL;
    data->render_flag = NULL;
    data->node_id = node_id;
    return item;
}

int textarea_widget(Document *doc, int doc_id) {
    int item = uiItem();
    uiSetEvents(item, UI_BUTTON0_DOWN | UI_BUTTON0_CAPTURE | UI_KEY_DOWN | UI_CHAR | UI_SCROLL);
    UITextAreaData *data = (UITextAreaData *)uiAllocHandle(item, sizeof(UITextAreaData));
    data->head.subtype = ST_TEXTAREA;
    data->head.handler = ide_textarea_handler;
    data->text = doc->buffer;
    data->maxsize = sizeof(doc->buffer);
    data->cursor_pos = &doc->cursor_pos;
    data->select_start = &doc->select_start;
    data->select_end = &doc->select_end;
    data->scroll_y = &doc->scroll_y;
    data->show_line_numbers = doc->line_numbers;
    data->doc_id = doc_id;
    return item;
}

int statusbar(int iconid, const char *status_text, const char *hints_text, const char *stats_text) {
    int item = uiItem();
    uiSetSize(item, 0, 22);
    UIStatusBarData *data = (UIStatusBarData *)uiAllocHandle(item, sizeof(UIStatusBarData));
    data->head.subtype = ST_STATUSBAR;
    data->head.handler = NULL;
    data->iconid = iconid;
    data->status_text = status_text;
    data->hints_text = hints_text;
    data->stats_text = stats_text;
    return item;
}

int splitter_corner(int pane_id) {
    int item = uiItem();
    uiSetSize(item, 16, 16);
    uiSetEvents(item, UI_BUTTON0_DOWN | UI_BUTTON0_CAPTURE);
    UISplitterCornerData *data = (UISplitterCornerData *)uiAllocHandle(item, sizeof(UISplitterCornerData));
    data->head.subtype = ST_SPLITTER_CORNER;
    data->head.handler = [](int it, UIevent ev) {
        UISplitterCornerData *d = (UISplitterCornerData *)uiGetHandle(it);
        if (ev == UI_BUTTON0_DOWN) {
            UIvec2 cur = uiGetCursor();
            g_dragging_corner = 1;
            g_drag_source_pane = d->pane_id;
            g_drag_start_x = (float)cur.x;
            g_drag_start_y = (float)cur.y;
            g_drag_cur_x = (float)cur.x;
            g_drag_cur_y = (float)cur.y;
            sprintf(g_status_msg, "Dragging corner: Drag left for vertical split, down for horizontal split");
        }
    };
    data->pane_id = pane_id;
    return item;
}

int splitter_divider(int is_vertical) {
    int item = uiItem();
    if (is_vertical) uiSetSize(item, 5, 0);
    else uiSetSize(item, 0, 5);
    uiSetEvents(item, UI_BUTTON0_DOWN | UI_BUTTON0_CAPTURE);
    UISplitterDividerData *data = (UISplitterDividerData *)uiAllocHandle(item, sizeof(UISplitterDividerData));
    data->head.subtype = ST_SPLITTER_DIVIDER;
    data->head.handler = [](int it, UIevent ev) {
        if (ev == UI_BUTTON0_DOWN) {
            g_dragging_divider = 1;
            sprintf(g_status_msg, "Resizing split ratio: drag to reposition divider");
        }
    };
    data->is_vertical = is_vertical;
    return item;
}

int hbox() {
    int item = uiItem();
    UIData *data = (UIData *)uiAllocHandle(item, sizeof(UIData));
    data->subtype = ST_HBOX;
    data->handler = NULL;
    uiSetBox(item, UI_ROW);
    uiSetLayout(item, UI_LEFT | UI_TOP);
    return item;
}

int vbox() {
    int item = uiItem();
    UIData *data = (UIData *)uiAllocHandle(item, sizeof(UIData));
    data->subtype = ST_VBOX;
    data->handler = NULL;
    uiSetBox(item, UI_COLUMN);
    uiSetLayout(item, UI_LEFT | UI_TOP);
    return item;
}

int column_append(int parent, int item) {
    uiInsert(parent, item);
    uiSetLayout(item, UI_LEFT | UI_TOP | UI_HFILL);
    uiSetMargins(item, 0, 1, 0, 0);
    return item;
}

int row_append(int parent, int item) {
    uiInsert(parent, item);
    uiSetLayout(item, UI_LEFT | UI_TOP | UI_HFILL);
    return item;
}

int row_append_fixed(int parent, int item) {
    uiInsert(parent, item);
    uiSetLayout(item, UI_LEFT | UI_TOP);
    return item;
}

////////////////////////////////////////////////////////////////////////////////
// UI Tree Drawing Function
////////////////////////////////////////////////////////////////////////////////

void drawUI(NVGcontext *vg, int item, int corners);

void drawUIItems(NVGcontext *vg, int item, int corners) {
    int kid = uiFirstChild(item);
    while (kid > 0) {
        drawUI(vg, kid, corners);
        kid = uiNextSibling(kid);
    }
}

void drawUI(NVGcontext *vg, int item, int corners) {
    const UIData *head = (const UIData *)uiGetHandle(item);
    UIrect rect = uiGetRect(item);

    if (uiGetState(item) == UI_FROZEN) {
        nvgGlobalAlpha(vg, BND_DISABLED_ALPHA);
    }

    if (head) {
        switch(head->subtype) {
            default: {
                drawUIItems(vg, item, corners);
            } break;

            case ST_PANEL: {
                bndBevel(vg, rect.x, rect.y, rect.w, rect.h);
                drawUIItems(vg, item, corners);
            } break;

            case ST_LABEL: {
                const UIButtonData *data = (UIButtonData*)head;
                bndLabel(vg, rect.x, rect.y, rect.w, rect.h, data->iconid, data->label);
            } break;

            case ST_BUTTON: {
                const UIButtonData *data = (UIButtonData*)head;
                bndToolButton(vg, rect.x, rect.y, rect.w, rect.h,
                    corners, (BNDwidgetState)uiGetState(item),
                    data->iconid, data->label);
            } break;

            case ST_MENUHEADER: {
                const UIButtonData *data = (UIButtonData*)head;
                bndMenuHeaderItem(vg, rect.x, rect.y, rect.w, rect.h,
                    (BNDwidgetState)uiGetState(item), data->iconid, data->label);
            } break;

            case ST_VIEWPICKER: {
                const UIViewPickerData *data = (UIViewPickerData*)head;
                bndViewPicker(vg, rect.x, rect.y, rect.w, rect.h,
                    corners, (BNDwidgetState)uiGetState(item),
                    data->iconid, data->title, data->value);
            } break;

            case ST_TAB: {
                const UITabData *data = (UITabData*)head;
                int is_active = (*data->active_tab == data->tab_id);
                BNDwidgetState state = (BNDwidgetState)uiGetState(item);
                bndTabPositioned(vg, rect.x, rect.y, rect.w, rect.h, corners, state,
                    data->iconid, data->label, is_active, data->position);
            } break;

            case ST_TREEITEM: {
                const UITreeItemData *data = (UITreeItemData*)head;
                BNDwidgetState state = (BNDwidgetState)uiGetState(item);
                bndTreeItem(vg, rect.x, rect.y, rect.w, rect.h, state,
                    data->depth, data->iconid, data->label,
                    *data->is_open, data->is_leaf, *data->is_selected,
                    -1, -1, -1); // No default restriction icons!
            } break;

            case ST_TEXTAREA: {
                const UITextAreaData *data = (UITextAreaData*)head;
                int is_focused = (uiGetFocusedItem() == item);
                BNDwidgetState state = is_focused ? BND_ACTIVE : (BNDwidgetState)uiGetState(item);
                bndTextArea(vg, rect.x, rect.y, rect.w, rect.h, corners, state,
                    data->text, *data->cursor_pos, *data->select_start, *data->select_end,
                    *data->scroll_y, data->show_line_numbers);
            } break;

            case ST_STATUSBAR: {
                const UIStatusBarData *data = (UIStatusBarData*)head;
                bndStatusBar(vg, rect.x, rect.y, rect.w, rect.h,
                    data->iconid, data->status_text, data->hints_text, data->stats_text);
            } break;

            case ST_SPLITTER_CORNER: {
                bndSplitterCorner(vg, rect.x, rect.y, (float)rect.w);
            } break;

            case ST_SPLITTER_DIVIDER: {
                const UISplitterDividerData *data = (UISplitterDividerData*)head;
                BNDwidgetState state = (BNDwidgetState)uiGetState(item);
                bndSplitterDivider(vg, rect.x, rect.y, rect.w, rect.h, data->is_vertical, state);
            } break;
        }
    } else {
        drawUIItems(vg, item, corners);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Dropdown Menu Drawing
////////////////////////////////////////////////////////////////////////////////

void draw_menu_dropdown(NVGcontext *vg, float x, float y,
    MenuItemDef *items, int item_count, int mx, int my, int mouse_down) {

    float item_h = 22.0f;
    float sep_h = 6.0f;
    float total_h = 6.0f;
    float menu_w = 210.0f;

    for (int i = 0; i < item_count; ++i) {
        total_h += items[i].is_separator ? sep_h : item_h;
    }

    nvgSave(vg);
    bndDropShadow(vg, x, y, menu_w, total_h, 3.0f, 8.0f, 0.45f);
    bndMenuBackground(vg, x, y, menu_w, total_h, BND_CORNER_ALL);

    float cur_y = y + 3.0f;
    for (int i = 0; i < item_count; ++i) {
        if (items[i].is_separator) {
            bndMenuSeparator(vg, x + 6.0f, cur_y + 2.0f, menu_w - 12.0f);
            cur_y += sep_h;
            continue;
        }

        int is_hover = (mx >= x && mx <= x + menu_w && my >= cur_y && my < cur_y + item_h);
        BNDwidgetState state = (is_hover && !items[i].is_disabled) ? BND_HOVER : BND_DEFAULT;

        if (is_hover && mouse_down && !items[i].is_disabled) {
            execute_menu_action(items[i].action_id);
            active_menu = MENU_NONE;
            nvgRestore(vg);
            return;
        }

        bndMenuItemShortcut(vg, x, cur_y, menu_w, item_h, state,
            items[i].iconid, items[i].label, items[i].shortcut,
            items[i].is_checked, items[i].is_submenu, items[i].is_disabled);

        cur_y += item_h;
    }
    nvgRestore(vg);
}

////////////////////////////////////////////////////////////////////////////////
// Single Editor Pane Builder (with Tabs positioned + Corner Splitter Handle)
////////////////////////////////////////////////////////////////////////////////

int build_editor_pane(int parent, int pane_id, int active_doc_idx) {
    Document *doc = &g_docs[active_doc_idx];

    int pane_box = vbox();
    uiSetLayout(pane_box, UI_LEFT | UI_TOP | UI_FILL);
    uiInsert(parent, pane_box);

    // Build Tab Bar according to g_tab_position
    auto build_tabs_row = [&]() -> int {
        int tabs_bar = hbox();
        uiSetSize(tabs_bar, 0, 24);
        uiSetLayout(tabs_bar, UI_LEFT | UI_TOP | UI_HFILL);

        int *active_doc_ptr = (pane_id == 1) ? &g_active_doc_pane1 : &g_active_doc_pane2;

        for (int i = 0; i < g_doc_count; ++i) {
            int t = tab_item(-1, g_docs[i].name, i, active_doc_ptr, g_tab_position,
                [](int it, UIevent ev) {
                    UITabData *d = (UITabData *)uiGetHandle(it);
                    if (d && ev == UI_BUTTON0_DOWN) {
                        *d->active_tab = d->tab_id;
                        sprintf(g_status_msg, "Opened tab: %s", d->label);
                    }
                });
            row_append_fixed(tabs_bar, t);
        }

        // Spacer to push corner split handle to the right
        int sp = uiItem();
        uiSetLayout(sp, UI_LEFT | UI_TOP | UI_HFILL);
        uiInsert(tabs_bar, sp);

        // If split, add a close split button
        if (g_is_split && pane_id == 2) {
            int close_btn = button(-1, "✕ Close", [](int it, UIevent ev) {
                if (ev == UI_BUTTON0_DOWN) {
                    g_is_split = 0;
                    sprintf(g_status_msg, "Closed split pane. Returned to single editor.");
                }
            });
            uiSetSize(close_btn, 65, 20);
            row_append_fixed(tabs_bar, close_btn);
        }

        // Blender-Style Corner Split Handle (diagonal ridges)
        int corner = splitter_corner(pane_id);
        row_append_fixed(tabs_bar, corner);

        return tabs_bar;
    };

    // Build vertical tabs if LEFT or RIGHT
    auto build_tabs_col = [&]() -> int {
        int tabs_col = vbox();
        uiSetSize(tabs_col, 110, 0);
        uiSetLayout(tabs_col, UI_LEFT | UI_TOP | UI_VFILL);

        int *active_doc_ptr = (pane_id == 1) ? &g_active_doc_pane1 : &g_active_doc_pane2;
        for (int i = 0; i < g_doc_count; ++i) {
            int t = tab_item(-1, g_docs[i].name, i, active_doc_ptr, g_tab_position,
                [](int it, UIevent ev) {
                    UITabData *d = (UITabData *)uiGetHandle(it);
                    if (d && ev == UI_BUTTON0_DOWN) {
                        *d->active_tab = d->tab_id;
                    }
                });
            column_append(tabs_col, t);
        }
        return tabs_col;
    };

    if (g_tab_position == BND_TAB_TOP) {
        uiInsert(pane_box, build_tabs_row());
    }

    if (g_tab_position == BND_TAB_LEFT || g_tab_position == BND_TAB_RIGHT) {
        int mid_h = hbox();
        uiSetLayout(mid_h, UI_LEFT | UI_TOP | UI_FILL);
        uiInsert(pane_box, mid_h);

        if (g_tab_position == BND_TAB_LEFT) {
            uiInsert(mid_h, build_tabs_col());
        }

        int ta = textarea_widget(doc, active_doc_idx);
        uiSetLayout(ta, UI_LEFT | UI_TOP | UI_FILL);
        uiInsert(mid_h, ta);

        if (g_tab_position == BND_TAB_RIGHT) {
            uiInsert(mid_h, build_tabs_col());
        }
    } else {
        int ta = textarea_widget(doc, active_doc_idx);
        uiSetLayout(ta, UI_LEFT | UI_TOP | UI_FILL);
        uiInsert(pane_box, ta);
    }

    if (g_tab_position == BND_TAB_BOTTOM) {
        uiInsert(pane_box, build_tabs_row());
    }

    return pane_box;
}

////////////////////////////////////////////////////////////////////////////////
// Splitter Panel (Parent of the Editor - Blender-style splitting)
////////////////////////////////////////////////////////////////////////////////

void build_splitter_editor_area(int parent) {
    if (!g_is_split) {
        // Single Editor Pane
        build_editor_pane(parent, 1, g_active_doc_pane1);
    } else {
        // Split Panes: parent splitter creates 2 panes separated by a divider
        if (g_split_type == 0) {
            // Horizontal Split (Left & Right side-by-side)
            int split_row = hbox();
            uiSetLayout(split_row, UI_LEFT | UI_TOP | UI_FILL);
            uiInsert(parent, split_row);

            // Left Pane (proportional width)
            int left_pane_container = vbox();
            uiSetLayout(left_pane_container, UI_LEFT | UI_TOP | UI_VFILL);
            uiInsert(split_row, left_pane_container);
            build_editor_pane(left_pane_container, 1, g_active_doc_pane1);

            // Resizable Splitter Divider
            int div = splitter_divider(1);
            uiInsert(split_row, div);

            // Right Pane (proportional width)
            int right_pane_container = vbox();
            uiSetLayout(right_pane_container, UI_LEFT | UI_TOP | UI_FILL);
            uiInsert(split_row, right_pane_container);
            build_editor_pane(right_pane_container, 2, g_active_doc_pane2);
        } else {
            // Vertical Split (Top & Bottom stacked)
            int split_col = vbox();
            uiSetLayout(split_col, UI_LEFT | UI_TOP | UI_FILL);
            uiInsert(parent, split_col);

            // Top Pane
            int top_pane_container = vbox();
            uiSetLayout(top_pane_container, UI_LEFT | UI_TOP | UI_HFILL);
            uiInsert(split_col, top_pane_container);
            build_editor_pane(top_pane_container, 1, g_active_doc_pane1);

            // Resizable Splitter Divider
            int div = splitter_divider(0);
            uiInsert(split_col, div);

            // Bottom Pane
            int btm_pane_container = vbox();
            uiSetLayout(btm_pane_container, UI_LEFT | UI_TOP | UI_FILL);
            uiInsert(split_col, btm_pane_container);
            build_editor_pane(btm_pane_container, 2, g_active_doc_pane2);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Complete Visual Studio IDE Layout
////////////////////////////////////////////////////////////////////////////////

void build_visual_studio_workspace(NVGcontext *vg, int root, float winWidth, float winHeight) {
    uiSetBox(root, UI_COLUMN);

    // 1. TOP MENU BAR (Built as a standard H-Box panel)
    int menubar = hbox();
    uiSetSize(menubar, 0, 26);
    uiSetLayout(menubar, UI_LEFT | UI_TOP | UI_HFILL);
    uiInsert(root, menubar);

    // Menus rendered as clean labels (not 3D buttons)
    row_append_fixed(menubar, menu_header_label("File", [](int it, UIevent ev) {
        if (ev == UI_BUTTON0_DOWN) {
            UIrect r = uiGetRect(it);
            active_menu = (active_menu == MENU_FILE) ? MENU_NONE : MENU_FILE;
            menu_popup_x = (float)r.x;
            menu_popup_y = (float)(r.y + r.h);
        }
    }));

    row_append_fixed(menubar, menu_header_label("Edit", [](int it, UIevent ev) {
        if (ev == UI_BUTTON0_DOWN) {
            UIrect r = uiGetRect(it);
            active_menu = (active_menu == MENU_EDIT) ? MENU_NONE : MENU_EDIT;
            menu_popup_x = (float)r.x;
            menu_popup_y = (float)(r.y + r.h);
        }
    }));

    row_append_fixed(menubar, menu_header_label("View", [](int it, UIevent ev) {
        if (ev == UI_BUTTON0_DOWN) {
            UIrect r = uiGetRect(it);
            active_menu = (active_menu == MENU_VIEW) ? MENU_NONE : MENU_VIEW;
            menu_popup_x = (float)r.x;
            menu_popup_y = (float)(r.y + r.h);
        }
    }));

    row_append_fixed(menubar, menu_header_label("Build", [](int it, UIevent ev) {
        if (ev == UI_BUTTON0_DOWN) {
            UIrect r = uiGetRect(it);
            active_menu = (active_menu == MENU_BUILD) ? MENU_NONE : MENU_BUILD;
            menu_popup_x = (float)r.x;
            menu_popup_y = (float)(r.y + r.h);
        }
    }));

    row_append_fixed(menubar, menu_header_label("Debug", [](int it, UIevent ev) {
        if (ev == UI_BUTTON0_DOWN) {
            UIrect r = uiGetRect(it);
            active_menu = (active_menu == MENU_DEBUG) ? MENU_NONE : MENU_DEBUG;
            menu_popup_x = (float)r.x;
            menu_popup_y = (float)(r.y + r.h);
        }
    }));

    row_append_fixed(menubar, menu_header_label("Help", [](int it, UIevent ev) {
        if (ev == UI_BUTTON0_DOWN) {
            UIrect r = uiGetRect(it);
            active_menu = (active_menu == MENU_HELP) ? MENU_NONE : MENU_HELP;
            menu_popup_x = (float)r.x;
            menu_popup_y = (float)(r.y + r.h);
        }
    }));

    // Other components added inside the same H-Box Menu Bar:
    // Spacer
    int sp1 = uiItem();
    uiSetSize(sp1, 12, 24);
    row_append_fixed(menubar, sp1);

    // Viewpicker with Title: Solution Configuration
    row_append_fixed(menubar, viewpicker(-1, "Config:", g_config_names[g_config_choice], [](int it, UIevent ev) {
        if (ev == UI_BUTTON0_DOWN) {
            UIrect r = uiGetRect(it);
            active_menu = (active_menu == MENU_CONFIG_PICKER) ? MENU_NONE : MENU_CONFIG_PICKER;
            menu_popup_x = (float)r.x;
            menu_popup_y = (float)(r.y + r.h);
        }
    }));

    // Viewpicker with Title: Tabs Orientation (Top, Bottom, Left, Right)
    row_append_fixed(menubar, viewpicker(-1, "Tabs:", g_tab_pos_names[g_tab_position], [](int it, UIevent ev) {
        if (ev == UI_BUTTON0_DOWN) {
            UIrect r = uiGetRect(it);
            active_menu = (active_menu == MENU_TABS_PICKER) ? MENU_NONE : MENU_TABS_PICKER;
            menu_popup_x = (float)r.x;
            menu_popup_y = (float)(r.y + r.h);
        }
    }));

    // Viewpicker with Title: Editor View (Single vs Dual Split)
    row_append_fixed(menubar, viewpicker(-1, "Editor:", g_is_split ? "Dual Split" : "Single", [](int it, UIevent ev) {
        if (ev == UI_BUTTON0_DOWN) {
            UIrect r = uiGetRect(it);
            active_menu = (active_menu == MENU_SPLIT_PICKER) ? MENU_NONE : MENU_SPLIT_PICKER;
            menu_popup_x = (float)r.x;
            menu_popup_y = (float)(r.y + r.h);
        }
    }));

    // Run / Debug Action Button
    row_append_fixed(menubar, button(BND_ICON_PLAY, "Start (F5)", [](int it, UIevent ev) {
        if (ev == UI_BUTTON0_DOWN) {
            execute_menu_action(501);
        }
    }));

    // Right spacer to push search bar to the right
    int sp_right = uiItem();
    uiSetLayout(sp_right, UI_LEFT | UI_TOP | UI_HFILL);
    uiInsert(menubar, sp_right);

    // Quick Search Button
    int search_btn = button(BND_ICON_VIEW3D, "Search (Ctrl+Q)", [](int it, UIevent ev) {
        if (ev == UI_BUTTON0_DOWN) {
            sprintf(g_status_msg, "Command Palette / Quick Search opened");
        }
    });
    uiSetSize(search_btn, 130, 22);
    row_append_fixed(menubar, search_btn);

    // 2. MAIN WORKSPACE (Solution Explorer + Splitter Editor + Output Panel)
    int main_body = hbox();
    uiSetLayout(main_body, UI_LEFT | UI_TOP | UI_FILL);
    uiInsert(root, main_body);

    // Left Sidebar: Solution Explorer
    int sidebar = vbox();
    uiSetSize(sidebar, 240, 0);
    uiSetLayout(sidebar, UI_LEFT | UI_TOP | UI_VFILL);
    uiInsert(main_body, sidebar);

    {
        // Sidebar tabs (Solution Explorer / Git)
        int sb_tabs = hbox();
        uiSetSize(sb_tabs, 0, 24);
        uiSetLayout(sb_tabs, UI_LEFT | UI_TOP | UI_HFILL);
        uiInsert(sidebar, sb_tabs);

        row_append_fixed(sb_tabs, tab_item(-1, "Solution", 0, &g_sidebar_tab, BND_TAB_TOP, [](int it, UIevent ev) {
            if (ev == UI_BUTTON0_DOWN) g_sidebar_tab = 0;
        }));
        row_append_fixed(sb_tabs, tab_item(-1, "Git Changes", 1, &g_sidebar_tab, BND_TAB_TOP, [](int it, UIevent ev) {
            if (ev == UI_BUTTON0_DOWN) g_sidebar_tab = 1;
        }));

        // Content panel
        int sb_content = panel();
        uiSetBox(sb_content, UI_COLUMN);
        uiSetLayout(sb_content, UI_LEFT | UI_TOP | UI_FILL);
        uiSetMargins(sb_content, 2, 2, 2, 2);
        uiInsert(sidebar, sb_content);

        if (g_sidebar_tab == 0) {
            for (int i = 0; i < g_solution_tree_count; ++i) {
                SolutionNode *n = &g_solution_tree[i];
                int ti = treeitem(n->depth, n->iconid, n->label, &n->is_open, n->is_leaf, &n->is_selected, n->id,
                    [](int it, UIevent ev) {
                        UITreeItemData *d = (UITreeItemData *)uiGetHandle(it);
                        if (d && ev == UI_BUTTON0_DOWN) {
                            UIvec2 cur = uiGetCursor();
                            UIrect r = uiGetRect(it);
                            float indent_x = r.x + d->depth * 14.0f + 6.0f;
                            // Click on disclosure chevron
                            if (!d->is_leaf && cur.x >= indent_x && cur.x <= indent_x + 14.0f) {
                                *d->is_open = !(*d->is_open);
                                return;
                            }
                            // Select node & open document if leaf
                            for (int j = 0; j < g_solution_tree_count; ++j) {
                                g_solution_tree[j].is_selected = (g_solution_tree[j].id == d->node_id);
                                if (g_solution_tree[j].id == d->node_id && g_solution_tree[j].target_doc >= 0) {
                                    if (g_focused_pane == 1) g_active_doc_pane1 = g_solution_tree[j].target_doc;
                                    else g_active_doc_pane2 = g_solution_tree[j].target_doc;
                                    sprintf(g_status_msg, "Active document: %s", g_docs[g_solution_tree[j].target_doc].name);
                                }
                            }
                        }
                    });
                column_append(sb_content, ti);
            }
        } else {
            column_append(sb_content, label(-1, "Git Repository: main"));
            column_append(sb_content, label(-1, "Changes (3 files):"));
            column_append(sb_content, label(-1, "  M  main.cpp"));
            column_append(sb_content, label(-1, "  M  blendish.h"));
            column_append(sb_content, label(-1, "  A  shader.frag"));
            column_append(sb_content, button(-1, "Commit & Push", [](int it, UIevent ev) {
                if (ev == UI_BUTTON0_DOWN) sprintf(g_status_msg, "Git commit successful");
            }));
        }
    }

    // Center & Bottom: Editor and Output Panels
    int center_and_bottom = vbox();
    uiSetLayout(center_and_bottom, UI_LEFT | UI_TOP | UI_FILL);
    uiInsert(main_body, center_and_bottom);

    {
        // Splitter Panel (Parent of Editor Area)
        int editor_area = vbox();
        uiSetLayout(editor_area, UI_LEFT | UI_TOP | UI_FILL);
        uiInsert(center_and_bottom, editor_area);

        build_splitter_editor_area(editor_area);

        // Bottom Output / Terminal Panel
        int bottom_panel = vbox();
        uiSetSize(bottom_panel, 0, 130);
        uiSetLayout(bottom_panel, UI_LEFT | UI_TOP | UI_HFILL);
        uiInsert(center_and_bottom, bottom_panel);

        // Bottom tabs (Output, Terminal, Error List)
        int btm_tabs = hbox();
        uiSetSize(btm_tabs, 0, 22);
        uiSetLayout(btm_tabs, UI_LEFT | UI_TOP | UI_HFILL);
        uiInsert(bottom_panel, btm_tabs);

        row_append_fixed(btm_tabs, tab_item(-1, "Output", 0, &g_bottom_tab, BND_TAB_TOP, [](int it, UIevent ev) {
            if (ev == UI_BUTTON0_DOWN) g_bottom_tab = 0;
        }));
        row_append_fixed(btm_tabs, tab_item(-1, "Terminal", 1, &g_bottom_tab, BND_TAB_TOP, [](int it, UIevent ev) {
            if (ev == UI_BUTTON0_DOWN) g_bottom_tab = 1;
        }));
        row_append_fixed(btm_tabs, tab_item(-1, "Error List (0)", 2, &g_bottom_tab, BND_TAB_TOP, [](int it, UIevent ev) {
            if (ev == UI_BUTTON0_DOWN) g_bottom_tab = 2;
        }));

        int btm_body = panel();
        uiSetBox(btm_body, UI_COLUMN);
        uiSetLayout(btm_body, UI_LEFT | UI_TOP | UI_FILL);
        uiSetMargins(btm_body, 2, 2, 2, 2);
        uiInsert(bottom_panel, btm_body);

        static Document out_doc = { "Output", "", "", 0, -1, -1, 0.0f, 0 };
        int out_ta = textarea_widget(&out_doc, -1);
        UITextAreaData *out_data = (UITextAreaData *)uiGetHandle(out_ta);
        if (out_data) {
            out_data->text = g_output_buffer;
            out_data->maxsize = sizeof(g_output_buffer);
        }
        uiSetLayout(out_ta, UI_LEFT | UI_TOP | UI_FILL);
        uiInsert(btm_body, out_ta);
    }

    // 3. BOTTOM STATUS BAR
    Document *cur_doc = &g_docs[g_focused_pane == 1 ? g_active_doc_pane1 : g_active_doc_pane2];
    int line = 1, col = 1;
    for (int i = 0; i < cur_doc->cursor_pos && cur_doc->buffer[i]; ++i) {
        if (cur_doc->buffer[i] == '\n') { line++; col = 1; }
        else col++;
    }
    char stats_buf[64];
    sprintf(stats_buf, "Ln %d, Col %d | Spaces: 4 | UTF-8 | CRLF | C++", line, col);

    int sbar = statusbar(-1, g_status_msg, "0 Errors, 0 Warnings | Git: main (clean)", stats_buf);
    uiSetLayout(sbar, UI_LEFT | UI_TOP | UI_HFILL);
    uiInsert(root, sbar);
}

////////////////////////////////////////////////////////////////////////////////
// GLFW Callbacks & Main Loop
////////////////////////////////////////////////////////////////////////////////

static void mousebutton(GLFWwindow *window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        active_menu = MENU_CONTEXT;
        context_menu_x = (float)mx;
        context_menu_y = (float)my;
    }

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        // If finishing a corner split drag
        if (g_dragging_corner) {
            float dx = g_drag_cur_x - g_drag_start_x;
            float dy = g_drag_cur_y - g_drag_start_y;
            float adx = fabsf(dx);
            float ady = fabsf(dy);

            if (adx > 25.0f || ady > 25.0f) {
                g_is_split = 1;
                // If dragged more horizontally than vertically: split side-by-side (Horizontal split)
                // If dragged more vertically than horizontally: split top-to-bottom (Vertical split)
                g_split_type = (adx >= ady) ? 0 : 1;
                g_active_doc_pane2 = (g_active_doc_pane1 + 1) % g_doc_count;
                sprintf(g_status_msg, "Split created! Duplicate editor pane active.");
            }
            g_dragging_corner = 0;
        }

        if (g_dragging_divider) {
            g_dragging_divider = 0;
        }
    }

    switch(button) {
        case 1: button = 2; break;
        case 2: button = 1; break;
    }
    uiSetButton(button, mods, (action == GLFW_PRESS) ? 1 : 0);
}

static void cursorpos(GLFWwindow *window, double x, double y) {
    uiSetCursor((int)x, (int)y);

    if (g_dragging_corner) {
        g_drag_cur_x = (float)x;
        g_drag_cur_y = (float)y;
    }

    // Auto-switch between open menus on hover across top menu bar headers
    if (active_menu >= MENU_FILE && active_menu <= MENU_HELP && y <= 26.0) {
        // Top menu headers roughly span x: 0..300
        if (x < 45.0) active_menu = MENU_FILE;
        else if (x < 85.0) active_menu = MENU_EDIT;
        else if (x < 130.0) active_menu = MENU_VIEW;
        else if (x < 175.0) active_menu = MENU_BUILD;
        else if (x < 225.0) active_menu = MENU_DEBUG;
        else if (x < 275.0) active_menu = MENU_HELP;
    }
}

static void scrollevent(GLFWwindow *window, double x, double y) {
    NVG_NOTUSED(window);
    uiSetScroll((int)x, (int)y);
}

static void charevent(GLFWwindow *window, unsigned int value) {
    NVG_NOTUSED(window);
    uiSetChar(value);
}

static void keyevent(GLFWwindow *window, int key, int scancode, int action, int mods) {
    NVG_NOTUSED(scancode);
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        if (active_menu != MENU_NONE) {
            active_menu = MENU_NONE;
            return;
        }
    }

    if (key == GLFW_KEY_F5 && action == GLFW_PRESS) {
        execute_menu_action(501);
        return;
    }

    // Spacebar ONLY opens context menu if NO text area is focused!
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS && uiGetFocusedItem() < 0) {
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        active_menu = MENU_CONTEXT;
        context_menu_x = (float)mx;
        context_menu_y = (float)my;
        return;
    }

    uiSetKey(key, mods, action);
}

void init(NVGcontext *vg) {
    int font = nvgCreateFont(vg, "system", "../DejaVuSans.ttf");
    if (font == -1) {
        font = nvgCreateFont(vg, "system", "DejaVuSans.ttf");
    }
    bndSetFont(font);

    int icon_image = nvgCreateImage(vg, "../blender_icons16.png", 0);
    if (icon_image == 0) {
        icon_image = nvgCreateImage(vg, "blender_icons16.png", 0);
    }
    bndSetIconImage(icon_image);
}

int main() {
    if (!glfwInit()) {
        printf("Failed to initialize GLFW\n");
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(1100, 720, "Visual Studio IDE - Blendish Workspace", NULL, NULL);
    if (!window) {
        printf("Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }
    g_window = window;

    glfwSetKeyCallback(window, keyevent);
    glfwSetCharCallback(window, charevent);
    glfwSetCursorPosCallback(window, cursorpos);
    glfwSetMouseButtonCallback(window, mousebutton);
    glfwSetScrollCallback(window, scrollevent);

    glfwMakeContextCurrent(window);

#ifdef NANOVG_GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        printf("Could not initialize GLEW\n");
        return -1;
    }
    glGetError();
#endif

    NVGcontext *vg = nvgCreateGL3(NVG_ANTIALIAS);
    if (!vg) {
        printf("Could not initialize NanoVG GL3\n");
        return -1;
    }
    init(vg);
    g_vg = vg;

    UIcontext *uictx = uiCreateContext(4096, 1 << 20);
    uiMakeCurrent(uictx);
    uiSetHandler([](int item, UIevent event) {
        UIData *data = (UIData *)uiGetHandle(item);
        if (data && data->handler) {
            data->handler(item, event);
        }
    });

    glfwSwapInterval(1);

    while (!glfwWindowShouldClose(window)) {
        double cur_time = glfwGetTime();

        int winWidth, winHeight;
        int fbWidth, fbHeight;
        glfwGetWindowSize(window, &winWidth, &winHeight);
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        float pxRatio = (float)fbWidth / (float)winWidth;

        glViewport(0, 0, fbWidth, fbHeight);
        glClearColor(0.18f, 0.18f, 0.18f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        nvgBeginFrame(vg, winWidth, winHeight, pxRatio);

        // Build UI declarations
        uiBeginLayout();

        int root = uiItem();
        uiSetSize(root, winWidth, winHeight);
        build_visual_studio_workspace(vg, root, (float)winWidth, (float)winHeight);

        uiEndLayout();

        // Render OUI tree
        drawUI(vg, 0, BND_CORNER_NONE);

        // Draw interactive split guide while dragging corner
        if (g_dragging_corner) {
            nvgSave(vg);
            nvgBeginPath(vg);
            float dx = g_drag_cur_x - g_drag_start_x;
            float dy = g_drag_cur_y - g_drag_start_y;
            if (fabsf(dx) >= fabsf(dy)) {
                // Vertical preview line (horizontal split)
                nvgMoveTo(vg, g_drag_cur_x, 26.0f);
                nvgLineTo(vg, g_drag_cur_x, (float)winHeight - 150.0f);
            } else {
                // Horizontal preview line (vertical split)
                nvgMoveTo(vg, 240.0f, g_drag_cur_y);
                nvgLineTo(vg, (float)winWidth, g_drag_cur_y);
            }
            nvgStrokeColor(vg, nvgRGBAf(0.337f, 0.502f, 0.761f, 0.9f));
            nvgStrokeWidth(vg, 2.0f);
            nvgStroke(vg);
            nvgRestore(vg);
        }

        // Render active dropdown menus
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        int mouse_down = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

        if (active_menu == MENU_FILE) {
            draw_menu_dropdown(vg, menu_popup_x, menu_popup_y, file_menu_items, sizeof(file_menu_items)/sizeof(file_menu_items[0]), (int)mx, (int)my, mouse_down);
        } else if (active_menu == MENU_EDIT) {
            draw_menu_dropdown(vg, menu_popup_x, menu_popup_y, edit_menu_items, sizeof(edit_menu_items)/sizeof(edit_menu_items[0]), (int)mx, (int)my, mouse_down);
        } else if (active_menu == MENU_VIEW) {
            draw_menu_dropdown(vg, menu_popup_x, menu_popup_y, view_menu_items, sizeof(view_menu_items)/sizeof(view_menu_items[0]), (int)mx, (int)my, mouse_down);
        } else if (active_menu == MENU_BUILD) {
            draw_menu_dropdown(vg, menu_popup_x, menu_popup_y, build_menu_items, sizeof(build_menu_items)/sizeof(build_menu_items[0]), (int)mx, (int)my, mouse_down);
        } else if (active_menu == MENU_DEBUG) {
            draw_menu_dropdown(vg, menu_popup_x, menu_popup_y, debug_menu_items, sizeof(debug_menu_items)/sizeof(debug_menu_items[0]), (int)mx, (int)my, mouse_down);
        } else if (active_menu == MENU_HELP) {
            draw_menu_dropdown(vg, menu_popup_x, menu_popup_y, help_menu_items, sizeof(help_menu_items)/sizeof(help_menu_items[0]), (int)mx, (int)my, mouse_down);
        } else if (active_menu == MENU_CONFIG_PICKER) {
            draw_menu_dropdown(vg, menu_popup_x, menu_popup_y, config_picker_items, sizeof(config_picker_items)/sizeof(config_picker_items[0]), (int)mx, (int)my, mouse_down);
        } else if (active_menu == MENU_TABS_PICKER) {
            draw_menu_dropdown(vg, menu_popup_x, menu_popup_y, tabs_picker_items, sizeof(tabs_picker_items)/sizeof(tabs_picker_items[0]), (int)mx, (int)my, mouse_down);
        } else if (active_menu == MENU_SPLIT_PICKER) {
            draw_menu_dropdown(vg, menu_popup_x, menu_popup_y, split_picker_items, sizeof(split_picker_items)/sizeof(split_picker_items[0]), (int)mx, (int)my, mouse_down);
        } else if (active_menu == MENU_CONTEXT) {
            draw_menu_dropdown(vg, context_menu_x, context_menu_y, context_menu_items, sizeof(context_menu_items)/sizeof(context_menu_items[0]), (int)mx, (int)my, mouse_down);
        }

        nvgEndFrame(vg);

        uiProcess((int)(cur_time * 1000.0));

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    uiDestroyContext(uictx);
    nvgDeleteGL3(vg);
    glfwTerminate();

    return 0;
}
