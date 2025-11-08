#include "PhysicsShapesInterface.h"
#include "Physics/BodyManager.h"
#include "imgui.h"
#include "ECS/Transform.h"
#include "ECS/Scene.h"
#include "EngineAPI/ECS/SceneAPI.h"
#include <Jolt/Jolt.h>
#include "Physics/PhysicsCore.h"
#include "Physics/PhysicsShapes.h"
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/TaperedCapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/TaperedCylinderShape.h>
#include <Jolt/Physics/Collision/Shape/PlaneShape.h>
#include <Jolt/Physics/Collision/Shape/TriangleShape.h>
#include <Jolt/Physics/Collision/Shape/CompoundShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>

namespace mofu::editor::physics::shapes {
namespace {
constexpr const char* PRIMITIVE_SHAPES_STRINGS[mofu::physics::shapes::PrimitiveShapes::Count]{
	"Box",
	"Capsule",
	"Sphere",
	"Cylinder",
	"Plane",
	"Triangle",
	"TaperedCapsule",
	"TaperedCylinder",
	"Compound",
	"ConvexHull",
};

constexpr const char* MOTION_TYPES_STRINGS[3]{
    "Static",
    "Kinematic",
    "Dynamic",
};

} // anonymous namespace

void 
DisplayMotionTypeOptions(ecs::Entity entity)
{
    if (ImGui::Button("Change Motion Type")) ImGui::OpenPopup("MotionTypeOptions");
    if (ImGui::BeginPopup("MotionTypeOptions"))
    {
        ecs::component::Collider& collider{ ecs::scene::GetEntityComponent<ecs::component::Collider>(entity) };
        const JPH::BodyID bodyId{ collider.BodyID };
        JPH::BodyLockRead lock{ mofu::physics::core::PhysicsSystem().GetBodyLockInterface(), bodyId };

        const JPH::Body& body{ lock.GetBody() };
        const JPH::EMotionType currentMotionType{ body.GetMotionType() };
        u32 chosenMotion{ (u32)currentMotionType };
        lock.ReleaseLock();

        if (ImGui::BeginListBox("Motion Types"))
        {
            for (u32 i{ 0 }; i < 3; ++i)
            {
                const bool isSelected = (chosenMotion == i);
                if (ImGui::Selectable(MOTION_TYPES_STRINGS[i], isSelected) && i != (u32)currentMotionType)
                {
                    if (chosenMotion != (u32)JPH::EMotionType::Dynamic && body.IsSoftBody())
                    {
                        log::Error("A Soft Body cannot be Dynamic");
                        continue;
                    }

                    chosenMotion = i;
                    if (currentMotionType != JPH::EMotionType::Static)
                    {
                        // have to deactivate the body first
                        mofu::physics::core::BodyInterface().DeactivateBody(bodyId);
                    }

                    JPH::BodyLockWrite lockWR{ mofu::physics::core::PhysicsSystem().GetBodyLockInterface(), bodyId };
                    lockWR.GetBody().SetMotionType((JPH::EMotionType)i);

                    if (!lockWR.GetBody().IsActive()) 
                    {
                        lockWR.ReleaseLock();
                        mofu::physics::core::BodyInterface().ActivateBody(bodyId);
                    }

                    if (currentMotionType == JPH::EMotionType::Static)
                    {
                        assert(ecs::scene::EntityHasComponent<ecs::component::StaticObject>(entity));
                        //TODO: something that makes this migration more effective
                        ecs::scene::RemoveComponent<ecs::component::StaticObject>(entity);
                        ecs::scene::AddComponent<ecs::component::DynamicObject>(entity);
                    }

                    ImGui::CloseCurrentPopup();
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndListBox();
        }

        ImGui::EndPopup();
    }
}

void
DisplayColliderOptions(ecs::Entity entity)
{
    if (ImGui::Button("Collider")) ImGui::OpenPopup("ChangeColliderPopup");
    if (ImGui::BeginPopup("ChangeColliderPopup"))
    {
        static u32 currentShape{ 0 };

        if (ImGui::BeginListBox("Primitive Shapes"))
        {
            for (u32 i{ 0 }; i < mofu::physics::shapes::PrimitiveShapes::Count; ++i)
            {
                const bool isSelected = (currentShape == i);
                if (ImGui::Selectable(PRIMITIVE_SHAPES_STRINGS[i], isSelected) && i != currentShape)
                {
                    currentShape = i;
                    mofu::physics::ChangePhysicsShape(entity, (mofu::physics::shapes::PrimitiveShapes::Type)i);
                    log::Info("JOLT: Changed physics shape to %s", PRIMITIVE_SHAPES_STRINGS[currentShape]);
                    ImGui::CloseCurrentPopup();
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndListBox();
        }

        ImGui::EndPopup();
    }
}

void
DisplayColliderEditor(ecs::Entity entity)
{
    ecs::component::Collider& collider{ ecs::scene::GetEntityComponent<ecs::component::Collider>(entity) };
    const JPH::BodyID bodyId{ collider.BodyID };
    if (ImGui::Button("Toggle Collider Edit"))
    {
        TODO_("Make writable etc");
    }
    JPH::BodyLockRead lock{ mofu::physics::core::PhysicsSystem().GetBodyLockInterface(), bodyId };
    if (!lock.Succeeded())
    {
        ImGui::Text("Can't lock the body for reading");
        return;
    }
    const JPH::Shape* shape{ lock.GetBody().GetShape() };
    JPH::Shape::Stats stats{ shape->GetStats() };

    switch (shape->GetSubType())
    {
    case JPH::EShapeSubType::Box:
    {
        const JPH::BoxShape* box = static_cast<const JPH::BoxShape*>(shape);
        JPH::Vec3 halfExtent = box->GetHalfExtent();
        ImGui::Text("Half Extents: %.2f, %.2f, %.2f", halfExtent.GetX(), halfExtent.GetY(), halfExtent.GetZ());
        break;
    }
    case JPH::EShapeSubType::Sphere:
    {
        const JPH::SphereShape* sphere = static_cast<const JPH::SphereShape*>(shape);
        float radius = sphere->GetRadius();
        ImGui::Text("Radius: %.2f", radius);
        break;
    }
    case JPH::EShapeSubType::Capsule:
    {
        const JPH::CapsuleShape* capsule = static_cast<const JPH::CapsuleShape*>(shape);
        ImGui::Text("Cylinder HalfHeight: %.2f", capsule->GetHalfHeightOfCylinder());
        ImGui::Text("Radius: %.2f", capsule->GetRadius());
        ImGui::Text("Inner Radius: %.2f", capsule->GetInnerRadius());
        break;
    }
    case JPH::EShapeSubType::Mesh:
        ImGui::Text("Mesh shape");
        break;

    default:
        ImGui::Text("Some shape");
        break;
    }
}

}