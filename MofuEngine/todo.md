
not recreating resources, keeping an asset library

import materials from fbx

culling

lights

ECS:
optimize
add filtering by components - not always data needed, just whether it has the component or not; etc

Scene space partitioning
	then can do culling
	and merging for acceleration structures?
merging different nearby meshes for one BLAS

(for now, just make different BLAS for everyone?)

AND SUPPORT FOR INSTANCING TOO SINCE IT REQUIRES THAT ANYWAYS:
in AssetInteraction - when spawning renderables - make a midpoint that checks if the AssetHandle already has a resource, then, 
give a bool to geometry backend so it just assigns the id it already has for that model

default to not-updateable --> make a DynamicObject and a StaticObject component, 
make an ASUpdateable component and build only these with allow_update flag
try compaction for static objects


texture packing

Project stuff
Handling Asset deletion

Asset Reimporting
(and reimport the geometry)

build a nice test scene
lights
then maybe ibl

vertex color test shader
resource reuse

smaller stuff:
- gooch shading
- output a screen normal buffer to use in post process
- and output object IDS and material ID, maybe ill find use for that:
- try out some stylized rendering rechniques

batching

ExecuteIndirect
try work graphs

rewrite mesh stuff - LODs, 1 material per, inspired by anvil
some spatial data structures for the scene
maybe integrate a physics engine
oob calculation
cpu frustum culling

material shaders:
- shader assets - how to handle backend dependencies? - the most simple - make a shader asset have handles
- how to expose properties from shaders - make own format that is useful in the shader / reflect the shader / just put the necessary things in both the shader and the asset file

transparency

reflections

image-space

shadows

sound tracing
make realistic instruments with sound tracing


guizmos

handling cursor in scene editor view:
detecting models at casted pos
dropping models at casted pos

