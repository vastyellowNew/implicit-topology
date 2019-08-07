/*
 * Call3DInteraction.h
 *
 * Copyright (C) 2019 by Universitaet Stuttgart (VISUS).
 * All rights reserved.
 */

#ifndef CALL_3D_INTERACTION
#define CALL_3D_INTERACTION
#if (defined(_MSC_VER) && (_MSC_VER > 1000))
#    pragma once
#endif /* (defined(_MSC_VER) && (_MSC_VER > 1000)) */

#include "mmcore/AbstractGetDataCall.h"
#include "mesh.h"

#include "3DInteractionCollection.h"

namespace megamol {
namespace mesh {

class MESH_API Call3DInteraction : public megamol::core::AbstractGetDataCall {
public:
    inline Call3DInteraction() : AbstractGetDataCall() {}
    ~Call3DInteraction() = default;

    /**
     * Answer the name of the objects of this description.
     *
     * @return The name of the objects of this description.
     */
    static const char* ClassName(void) { return "Call3DInteraction"; }

    /**
     * Gets a human readable description of the module.
     *
     * @return A human readable description of the module.
     */
    static const char* Description(void) { return "Call that transports"; }

    /**
     * Answer the number of functions used for this call.
     *
     * @return The number of functions used for this call.
     */
    static unsigned int FunctionCount(void) { return AbstractGetDataCall::FunctionCount(); }

    /**
     * Answer the name of the function used for this call.
     *
     * @param idx The index of the function to return it's name.
     *
     * @return The name of the requested function.
     */
    static const char* FunctionName(unsigned int idx) { return AbstractGetDataCall::FunctionName(idx); }

    void setInteractionCollection(std::shared_ptr<ThreeDimensionalInteractionCollection> interaction_collection)
    {
        m_interaction_collection = interaction_collection;
    }

    std::shared_ptr<ThreeDimensionalInteractionCollection> getInteractionCollection()
    {
        return m_interaction_collection;
    }

private:

    std::shared_ptr<ThreeDimensionalInteractionCollection> m_interaction_collection;

};

/** Description class typedef */
typedef megamol::core::factories::CallAutoDescription<Call3DInteraction> Call3DInteractionDescription;

}
}


#endif // !CALL_3D_INTERACTION
