//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "marble.h"

//----------------------------------------------------------------------------

static U32 sCollisionMask = StaticObjectType |
                            AtlasObjectType |
                            InteriorMapObjectType |
                            ShapeBaseObjectType |
                            PlayerObjectType |
                            VehicleBlockerObjectType;

static U32 sContactMask = StaticObjectType |
                          AtlasObjectType |
                          InteriorMapObjectType |
                          ShapeBaseObjectType |
                          PlayerObjectType |
                          VehicleBlockerObjectType;

bool gMarbleAxisSet = false;
Point3F gWorkGravityDir;
Point3F gMarbleSideDir;

Point3D Marble::getVelocityD() const
{
    return mVelocity;
}

void Marble::setVelocityD(const Point3D& vel)
{
    dMemcpy(mVelocity, vel, sizeof(mVelocity));
    mSinglePrecision.mVelocity = vel;

    setMaskBits(MoveMask);
}

void Marble::setVelocityRotD(const Point3D& rot)
{
    dMemcpy(mOmega, rot, sizeof(mOmega));

    setMaskBits(MoveMask);
}

void Marble::applyImpulse(const Point3F& pos, const Point3F& vec)
{
    // TODO: Implement applyImpulse
    Parent::applyImpulse(pos, vec);
}

void Marble::clearMarbleAxis()
{
    gMarbleAxisSet = false;
    mGravityFrame.mulP(Point3F(0.0f, 0.0f, -1.0f), &gWorkGravityDir);
}

void Marble::applyContactForces(const Move* move, bool isCentered, Point3D& aControl, const Point3D& desiredOmega, F64 timeStep, Point3D& A, Point3D& a, F32& slipAmount)
{
    // TODO: Implement applyContactForces
}

void Marble::getMarbleAxis(Point3D& sideDir, Point3D& motionDir, Point3D& upDir)
{
    if (!gMarbleAxisSet)
    {
        MatrixF camMat;
        mGravityFrame.setMatrix(&camMat);


        MatrixF xRot;
        m_matF_set_euler(Point3F(mMouseY, 0, 0), xRot);

        MatrixF zRot;
        m_matF_set_euler(Point3F(0, 0, mMouseX), zRot);

        camMat.mul(zRot);
        camMat.mul(xRot);

        gMarbleMotionDir.x = camMat[1];
        gMarbleMotionDir.y = camMat[5];
        gMarbleMotionDir.z = camMat[9];

        mCross(gMarbleMotionDir, -gWorkGravityDir, gMarbleSideDir);
        m_point3F_normalize(&gMarbleSideDir.x);
        
        mCross(-gWorkGravityDir, gMarbleSideDir, gMarbleMotionDir);
        
        gMarbleAxisSet = 1;
    }

    sideDir = gMarbleSideDir;
    motionDir = gMarbleMotionDir;
    upDir = -gWorkGravityDir;
}

const Point3F& Marble::getMotionDir()
{
    Point3D side;
    Point3D motion;
    Point3D up;
    Marble::getMarbleAxis(side, motion, up);

    return gMarbleMotionDir;
}

bool Marble::computeMoveForces(Point3D& aControl, Point3D& desiredOmega, const Move* move)
{
    // TODO: Implement computeMoveForces

    return false;
}

void Marble::velocityCancel(bool surfaceSlide, bool noBounce, bool& bouncedYet, bool& stoppedPaths, Vector<PathedInterior*>& pitrVec)
{
    // TODO: Implement velocityCancel
}

Point3D Marble::getExternalForces(const Move* move, F64 timeStep)
{
    // TODO: Implement getExternalForces
    return Point3D();
}

void Marble::advancePhysics(const Move* move, U32 timeDelta)
{
    dMemcpy(&delta.posVec, &mPosition, sizeof(delta.posVec));

    smPathItrVec.clear();

    F32 dt = timeDelta / 1000.0;

    Box3F extrudedMarble = this->mWorldBox;

    Point3F thing = (mVelocity * dt) * 1.100000023841858;
    Point3F absThing = thing.abs();

    extrudedMarble.min += (thing - absThing) * 0.5f;
    extrudedMarble.max += (thing + absThing) * 0.5f;

    extrudedMarble.min -= dt * 25.0;
    extrudedMarble.max += dt * 25.0;

    for (PathedInterior* obj = PathedInterior::getPathedInteriors(this); ; obj = obj->getNext())
    {
        if (!obj)
            break;

        if (extrudedMarble.isOverlapped(obj->getExtrudedBox()))
        {
            obj->pushTickState();
            obj->computeNextPathStep(timeDelta);
            smPathItrVec.push_back(obj);
        }
    }

    resetObjectsAndPolys(sContactMask, extrudedMarble);

    bool bouncedYet = false;
    
    mMovePathSize = 0;
    
    F64 timeRemaining = timeDelta / 1000.0;
    F64 startTime = timeRemaining;
    F32 slipAmount = 0.0;
    F64 contactTime = 0.0;

    U32 it = 0;
    do
    {
        if (timeRemaining == 0.0)
            break;

        F64 timeStep = 0.00800000037997961;
        if (timeRemaining < 0.00800000037997961)
            timeStep = timeRemaining;

        Point3D aControl;
        Point3D desiredOmega;

        bool isCentered = computeMoveForces(aControl, desiredOmega, move);

        findContacts(sContactMask, 0, 0);

        bool stoppedPaths;
        velocityCancel(isCentered, 0, bouncedYet, stoppedPaths, smPathItrVec);
        getExternalForces(move, timeStep);

        Point3D a(0, 0, 0);
        Point3D A(0, 0, 0);
        applyContactForces(move, isCentered, aControl, desiredOmega, timeStep, A, a, slipAmount);

        mVelocity += A * timeStep;
        mOmega += a * timeStep;

        if ((mMode & RestrictXYZMode) != 0)
            mVelocity.set(0, 0, 0);

        velocityCancel(isCentered, 1, bouncedYet, stoppedPaths, smPathItrVec);

        F64 moveTime = timeStep;
        computeFirstPlatformIntersect(moveTime, smPathItrVec);
        testMove(mVelocity, mPosition, moveTime, mRadius, sCollisionMask, 0);

        if (!mMovePathSize && timeStep * 0.99 > moveTime && moveTime > 0.001000000047497451)
        {
            F64 diff = startTime - timeRemaining;

            mMovePath[0] = mPosition;
            mMovePathTime[mMovePathSize] = (diff + moveTime) / startTime;

            mMovePathSize++;
        }

        F64 currentTimeStep = timeStep;
        if (timeStep != moveTime)
        {
            F64 diff = timeStep - moveTime;

            mVelocity -= A * diff;
            mOmega -= a * diff;

            currentTimeStep = moveTime;
        }

        if (this->mContacts.size())
            contactTime += currentTimeStep;

        timeRemaining -= currentTimeStep;

        timeStep = (startTime - timeRemaining) * 1000.0;

        for (S32 i = 0; i < smPathItrVec.size(); i++)
        {
            PathedInterior* pint = smPathItrVec[i];
            pint->resetTickState(0);
            pint->advance(timeStep);
        }

        it++;
    } while (it <= 10);

    for (S32 i = 0; i < smPathItrVec.size(); i++)
        smPathItrVec[i]->popTickState();

    F32 contactPct = contactTime * 1000.0 / timeDelta;

    Con::setFloatVariable("testCount", contactPct);
    Con::setFloatVariable("marblePitch", mMouseY);

    updateRollSound(contactPct, slipAmount);

    dMemcpy(&delta.pos, &mPosition, sizeof(delta.pos));

    delta.posVec -= delta.pos;

    setPosition(mPosition, 0);
}

ConsoleMethod(Marble, setVelocityRot, bool, 3, 3, "(vel)")
{
    Point3F rot;
    dSscanf(argv[2], "%f %f %f", &rot.x, &rot.y, &rot.z);
    object->setVelocityRotD(rot);

    return 1;
}
