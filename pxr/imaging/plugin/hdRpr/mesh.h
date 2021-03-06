#ifndef HDRPR_MESH_H
#define HDRPR_MESH_H

#include "pxr/imaging/hd/mesh.h"
#include "pxr/imaging/hd/vertexAdjacency.h"
#include "pxr/base/vt/array.h"
#include "pxr/base/gf/vec2f.h"
#include "pxr/base/gf/matrix4f.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdRprApi;
class HdRprParam;
class RprApiObject;
using RprApiObjectPtr = std::unique_ptr<RprApiObject>;

class HdRprMesh final : public HdMesh {
public:
    HF_MALLOC_TAG_NEW("new HdRprMesh");

    HdRprMesh(SdfPath const& id, SdfPath const& instancerId = SdfPath());
    ~HdRprMesh() override = default;

    void Sync(HdSceneDelegate* sceneDelegate,
              HdRenderParam* renderParam,
              HdDirtyBits* dirtyBits,
              TfToken const& reprName) override;

    void Finalize(HdRenderParam* renderParam) override;

    HdDirtyBits GetInitialDirtyBitsMask() const override;

protected:
    HdDirtyBits _PropagateDirtyBits(HdDirtyBits bits) const override;

    void _InitRepr(TfToken const& reprName, HdDirtyBits* dirtyBits) override;

private:
    template <typename T>
    bool GetPrimvarData(TfToken const& name,
                        HdSceneDelegate* sceneDelegate,
                        std::map<HdInterpolation, HdPrimvarDescriptorVector> primvarDescsPerInterpolation,
                        VtArray<T>& out_data,
                        VtIntArray& out_indices);

    RprApiObject const* GetFallbackMaterial(HdSceneDelegate* sceneDelegate, HdRprApi* rprApi, HdDirtyBits dirtyBits);

private:
    std::vector<RprApiObjectPtr> m_rprMeshes;
    std::vector<std::vector<RprApiObjectPtr>> m_rprMeshInstances;
    RprApiObjectPtr m_fallbackMaterial;
    SdfPath m_cachedMaterialId;
    GfMatrix4f m_transform;

    HdMeshTopology m_topology;
    HdGeomSubsets m_geomSubsets;
    VtVec3fArray m_points;
    VtIntArray m_faceVertexCounts;
    VtIntArray m_faceVertexIndices;
    bool m_enableSubdiv = false;

    Hd_VertexAdjacency m_adjacency;
    bool m_adjacencyValid = false;

    VtVec3fArray m_normals;
    VtIntArray m_normalIndices;
    bool m_normalsValid = false;
    bool m_authoredNormals = false;
    bool m_smoothNormals = false;

    VtVec2fArray m_uvs;
    VtIntArray m_uvIndices;

    HdDisplayStyle m_displayStyle;
    int m_refineLevel = 0;
    bool m_doublesided = false;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDRPR_MESH_H
