/*
 * Window.h
 *
 * Copyright (C) 2008, 2016 MegaMol Team
 * Alle Rechte vorbehalten.
 */

#ifndef MEGAMOLCON_GL_WINDOW_H_INCLUDED
#define MEGAMOLCON_GL_WINDOW_H_INCLUDED
#pragma once

#include "CoreHandle.h"
#include <memory>
#include <vector>
#include "vislib/graphics/gl/IncludeAllGL.h"
#include "GLFW/glfw3.h"
#include "gl/glfwInst.h"
#include "utility/ConfigHelper.h"
#include "mmcore/api/MegaMolCore.h"
#include "AbstractUILayer.h"
#include "vislib/graphics/FpsCounter.h"
#include <chrono>
#include <array>

namespace megamol {
namespace console {
namespace gl {

    /**
     * Class of viewing windows.
     */
    class Window {
    public:
        static void MEGAMOLCORE_CALLBACK RequestCloseCallback(void *w) {
            Window* win = static_cast<Window*>(w);
            if (win != nullptr) win->RequestClose();
        }

        Window(const char* title, const utility::WindowPlacement & placement, GLFWwindow* share);
        virtual ~Window();

        void EnableVSync();
        void AddUILayer(std::shared_ptr<AbstractUILayer> uiLayer);
        void RemoveUILayer(std::shared_ptr<AbstractUILayer> uiLayer);
        inline void ForceIssueResizeEvent() {
            on_resize(width, height);
        }

        inline void * Handle() {
            return hView;
        }
        inline void const * Handle() const {
            return hView;
        }
        inline GLFWwindow * WindowHandle() const {
            return hWnd;
        }
        inline const char* Name() const {
            return name.c_str();
        }
        inline const float& FPS() const {
            return fps;
        }
        inline float LiveFPS() const {
            return fpsCntr.FPS();
        }
        inline const std::array<float, 20>& LiveFPSList() const {
            return fpsList;
        }
        inline bool ShowFPSinTitle() const {
            return showFpsInTitle;
        }
        void SetShowFPSinTitle(bool show);

        inline bool IsAlive() const {
            return hWnd != nullptr;
        }
        void RequestClose();

        void Update();

    private:

        static void glfw_onKey_func(GLFWwindow* wnd, int k, int s, int a, int m);
        static void glfw_onChar_func(GLFWwindow* wnd, unsigned int charcode);
        static void glfw_onMouseMove_func(GLFWwindow* wnd, double x, double y);
        static void glfw_onMouseButton_func(GLFWwindow* wnd, int b, int a, int m);
        static void glfw_onMouseWheel_func(GLFWwindow* wnd, double x, double y);

        void on_resize(int w, int h);
        void on_fps_value(float fps_val);

        std::shared_ptr<glfwInst> glfw;
        CoreHandle hView; // Handle to the core view instance
        GLFWwindow *hWnd; // glfw window handle
        int width, height;
        mmcRenderViewContext renderContext;
        std::vector<std::shared_ptr<AbstractUILayer> > uiLayers;
        std::shared_ptr<AbstractUILayer> mouseCapture;
        std::string name;
        vislib::graphics::FpsCounter fpsCntr;
        float fps;
        std::array<float, 20> fpsList;
        bool showFpsInTitle;
        std::chrono::system_clock::time_point fpsSyncTime;
        bool topMost;

    };

} /* end namespace gl */
} /* end namespace console */
} /* end namespace megamol */

#endif /* MEGAMOLCON_GL_WINDOW_H_INCLUDED */