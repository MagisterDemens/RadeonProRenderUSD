#include "renderPass.h"
#include "renderDelegate.h"
#include "config.h"
#include "rprApi.h"
#include "renderBuffer.h"
#include "renderParam.h"

#include "pxr/imaging/hd/renderPassState.h"
#include "pxr/imaging/hd/renderIndex.h"

#include <GL/glew.h>

PXR_NAMESPACE_OPEN_SCOPE

HdRprRenderPass::HdRprRenderPass(HdRenderIndex* index,
                                 HdRprimCollection const& collection,
                                 HdRprRenderParam* renderParam)
    : HdRenderPass(index, collection)
    , m_renderParam(renderParam) {

}

void HdRprRenderPass::_Execute(HdRenderPassStateSharedPtr const& renderPassState, TfTokenVector const& renderTags) {
    // To avoid potential deadlock:
    //   main thread locks config instance and requests render stop and
    //   in the same time render thread trying to lock config instance to update its settings.
    // We should release config instance lock before requesting render thread to stop.
    // It could be solved in another way by using shared_mutex and
    // marking current write-lock as read-only after successful config->Sync
    // in such a way main and render threads would have read-only-locks that could coexist
    bool stopRender = false;
    {
        HdRprConfig* config;
        auto configInstanceLock = HdRprConfig::GetInstance(&config);
        config->Sync(GetRenderIndex()->GetRenderDelegate());
        if (config->IsDirty(HdRprConfig::DirtyAll)) {
            stopRender = true;
        }
    }
    if (stopRender) {
        m_renderParam->GetRenderThread()->StopRender();
    }

    auto rprApiConst = m_renderParam->GetRprApi();

    auto& vp = renderPassState->GetViewport();
    GfVec2i newViewportSize(static_cast<int>(vp[2]), static_cast<int>(vp[3]));
    auto oldViewportSize = rprApiConst->GetViewportSize();
    if (oldViewportSize != newViewportSize) {
        m_renderParam->AcquireRprApiForEdit()->SetViewportSize(newViewportSize);
    }

    if (rprApiConst->GetAovBindings() != renderPassState->GetAovBindings()) {
        m_renderParam->AcquireRprApiForEdit()->SetAovBindings(renderPassState->GetAovBindings());
    }

    if (rprApiConst->GetCamera() != renderPassState->GetCamera()) {
        m_renderParam->AcquireRprApiForEdit()->SetCamera(renderPassState->GetCamera());
    }

    if (rprApiConst->IsChanged()) {
        for (auto& aovBinding : renderPassState->GetAovBindings()) {
            if (aovBinding.renderBuffer) {
                auto rprRenderBuffer = static_cast<HdRprRenderBuffer*>(aovBinding.renderBuffer);
                rprRenderBuffer->SetConverged(false);
            }
        }
        m_renderParam->GetRenderThread()->StartRender();
    }
}

bool HdRprRenderPass::IsConverged() const {
    for (auto& aovBinding : m_renderParam->GetRprApi()->GetAovBindings()) {
        if (aovBinding.renderBuffer &&
            !aovBinding.renderBuffer->IsConverged()) {
            return false;
        }
    }
    return true;
}

PXR_NAMESPACE_CLOSE_SCOPE
