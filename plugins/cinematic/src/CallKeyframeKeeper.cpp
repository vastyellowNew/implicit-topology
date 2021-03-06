/*
* CallKeyframeKeeper.cpp
*
* Copyright (C) 2017 by VISUS (Universitaet Stuttgart).
* Alle Rechte vorbehalten.
*/

#include "stdafx.h"
#include "CallKeyframeKeeper.h"


using namespace megamol;
using namespace megamol::cinematic;


CallKeyframeKeeper::CallKeyframeKeeper(void) : core::AbstractGetDataCall(),
    cameraParam(nullptr),
    interpolCamPos(nullptr),
    keyframes(nullptr),
    boundingbox(nullptr),
    interpolSteps(10),
    selectedKeyframe(),
    dropAnimTime(0.0f),
    dropSimTime(0.0f),
    totalAnimTime(1.0f),
    totalSimTime(1.0f),
    bboxCenter(0.0f, 0.0f, 0.0f),
    fps(24),
    startCtrllPos(0.0f, 0.0f, 0.0f),
    endCtrllPos(0.0f, 0.0f, 0.0f)
{

}


CallKeyframeKeeper::~CallKeyframeKeeper(void) {

	this->keyframes      = nullptr;
    this->interpolCamPos = nullptr;
    this->boundingbox    = nullptr;
}
