/*
 * clip_plane.h
 *
 * Copyright (C) 2019 by Universitaet Stuttgart (VIS).
 * Alle Rechte vorbehalten.
 */
#pragma once

#include "mmcore/Call.h"
#include "mmcore/CalleeSlot.h"
#include "mmcore/param/ParamSlot.h"
#include "mmcore/view/CallRender3D_2.h"
#include "mmcore/view/Renderer3DModule_2.h"

#include "vislib/math/Plane.h"

namespace megamol {
namespace flowvis {

/**
 * Module for rendering and providing a clip plane.
 *
 * @author Alexander Straub
 */
class clip_plane : public core::view::Renderer3DModule_2 {
public:
    /**
     * Answer the name of this module.
     *
     * @return The name of this module.
     */
    static inline const char* ClassName() { return "clip_plane"; }

    /**
     * Answer a human readable description of this module.
     *
     * @return A human readable description of this module.
     */
    static inline const char* Description() { return "Render and provide a clip plane"; }

    /**
     * Answers whether this module is available on the current system.
     *
     * @return 'true' if the module is available, 'false' otherwise.
     */
    static inline bool IsAvailable() { return true; }

    /**
     * Initialises a new instance.
     */
    clip_plane();

    /**
     * Finalises an instance.
     */
    virtual ~clip_plane();

protected:
    /**
     * Implementation of 'Create'.
     *
     * @return 'true' on success, 'false' otherwise.
     */
    virtual bool create() override;

    /**
     * Implementation of 'Release'.
     */
    virtual void release() override;

    /** Callbacks for the computed streamlines */
    virtual bool GetExtents(core::view::CallRender3D_2& call) override;
    virtual bool Render(core::view::CallRender3D_2& call) override;

private:
    /** Provide the clip plane to incoming call */
    bool get_clip_plane_callback(core::Call& call);

    /** Input slot for getting the model matrix */
    core::CallerSlot model_matrix_slot;

    /** Output slot for providing the clip plane */
    core::CalleeSlot clip_plane_slot;

    /** Parameter for setting the plane color */
    core::param::ParamSlot color;

    /** The clip plane */
    vislib::math::Plane<float> plane;

    /** Struct for storing data needed for rendering */
    struct render_data_t {
        GLuint vs, fs, prog;
    } render_data;

    /** Initialization status */
    bool initialized;
};

} // namespace flowvis
} // namespace megamol
