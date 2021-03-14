#pragma once

#include <BlueBlur.h>
#include <Hedgehog/Yggdrasill/hhYggSceneRenderer.h>

namespace Sonic
{
    class CFxSceneRenderer : public Hedgehog::Yggdrasill::CYggSceneRenderer
    {
    public:
        INSERT_PADDING(0xC);
        uint8_t m_BackgroundColor[4];
        INSERT_PADDING(0x40);
        Eigen::Matrix4f m_LightView;
        Eigen::Matrix4f m_VerticalLightView;
        Eigen::Matrix4f m_LightProjection;
        Eigen::Matrix4f m_VerticalLightProjection;
        INSERT_PADDING(0x10);
    };

    ASSERT_OFFSETOF(CFxSceneRenderer, m_BackgroundColor, 0xBC);
    ASSERT_OFFSETOF(CFxSceneRenderer, m_LightView, 0x100);
    ASSERT_OFFSETOF(CFxSceneRenderer, m_VerticalLightView, 0x140);
    ASSERT_OFFSETOF(CFxSceneRenderer, m_LightProjection, 0x180);
    ASSERT_OFFSETOF(CFxSceneRenderer, m_VerticalLightProjection, 0x1C0);
    ASSERT_SIZEOF(CFxSceneRenderer, 0x210);
}