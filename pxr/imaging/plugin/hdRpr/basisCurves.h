#ifndef HDRPR_BASIS_CURVES_H
#define HDRPR_BASIS_CURVES_H

#include "pxr/imaging/hd/basisCurves.h"
#include "pxr/base/gf/matrix4f.h"
#include "pxr/base/gf/vec2f.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdRprApi;
class HdRprMaterial;
class RprApiObject;
using RprApiObjectPtr = std::unique_ptr<RprApiObject>;

class HdRprBasisCurves : public HdBasisCurves {

public:
    HdRprBasisCurves(SdfPath const& id,
                     SdfPath const& instancerId);

    ~HdRprBasisCurves() override = default;

    void Sync(HdSceneDelegate* delegate,
              HdRenderParam* renderParam,
              HdDirtyBits* dirtyBits,
              TfToken const& reprSelector) override;

    void Finalize(HdRenderParam* renderParam) override;

    HdDirtyBits GetInitialDirtyBitsMask() const override;

protected:
    HdDirtyBits _PropagateDirtyBits(HdDirtyBits bits) const override;

    void _InitRepr(TfToken const& reprName,
                   HdDirtyBits* dirtyBits) override;

private:
    RprApiObjectPtr CreateRprCurve(HdRprApi* rprApi);

private:
    RprApiObjectPtr m_rprCurve;
    RprApiObjectPtr m_fallbackMaterial;
    HdRprMaterial const* m_cachedMaterial;

    HdBasisCurvesTopology m_topology;
    VtIntArray m_indices;
    VtFloatArray m_widths;
    HdInterpolation m_widthsInterpolation;
    VtVec2fArray m_uvs;
    HdInterpolation m_uvsInterpolation;
    VtVec3fArray m_points;
    GfMatrix4f m_transform;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDRPR_BASIS_CURVES_H
