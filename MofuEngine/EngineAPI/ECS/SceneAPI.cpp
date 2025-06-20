#include "SceneAPI.h"
#include "ECS/Scene.h"
#include "ECS/QueryView.h"

namespace mofu::ecs::scene {

void DestroyEntity(Entity entity)
{
	if (!id::IsValid(entity)) return;
	//TODO: remove render item?
	//graphics::RemoveRenderItem(entity);
	scene::RemoveEntity(entity);
}

void DestroyScene()
{
	scene::UnloadScene();
}

}