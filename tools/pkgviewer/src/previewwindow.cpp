#include <algorithm>
#include <string>
#include <vector>
#include <imgui.h>
#include "imgf.hpp"
#include "sysconst.hpp"
#include "mainwindow.hpp"
#include "previewwindow.hpp"

void PreviewWindow::draw()
{
    if(!m_open){
        return;
    }
    if(m_resizeRequested){
        const float width = std::max(200, m_texture.width() + 40);
        const float height = std::max(200, m_texture.height() + 60);
        ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
        m_resizeRequested = false;
    }
    const auto title = "Index_" + std::to_string(m_imageIndex);
    if(ImGui::Begin(title.c_str(), &m_open)){
        const auto canvasPos = ImGui::GetCursorScreenPos();
        const auto canvasSize = ImGui::GetContentRegionAvail();
        auto *drawList = ImGui::GetWindowDrawList();
        if(m_owner->clearBackgroundEnabled()){
            drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32_BLACK);
        }
        if(m_texture.valid()){
            const float imageX = m_owner->offsetDrawEnabled()
                ? canvasPos.x + canvasSize.x / 2 + m_imageOffX - SYS_MAPGRIDXP / 2
                : canvasPos.x + (canvasSize.x - m_texture.width()) / 2;
            const float imageY = m_owner->offsetDrawEnabled()
                ? canvasPos.y + canvasSize.y / 2 + m_imageOffY - SYS_MAPGRIDYP / 2
                : canvasPos.y + (canvasSize.y - m_texture.height()) / 2;
            const ImVec2 imageMin(imageX, imageY);
            const ImVec2 imageMax(imageX + m_texture.width(), imageY + m_texture.height());
            drawList->AddImage(m_texture.id(), imageMin, imageMax);
            drawList->AddRect(imageMin, imageMax, IM_COL32(255, 0, 0, 255));
            if(m_owner->showOffsetCrossEnabled()){
                const ImVec2 origin(imageX - m_imageOffX, imageY - m_imageOffY);
                const ImVec2 cross(origin.x + SYS_MAPGRIDXP / 2, origin.y + SYS_MAPGRIDYP / 2);
                drawList->AddLine(imageMin, origin, IM_COL32(0, 0, 255, 255));
                drawList->AddLine(origin, cross, IM_COL32(0, 0, 255, 255));
                drawList->AddLine(ImVec2(cross.x - 5, cross.y - 5), ImVec2(cross.x + 5, cross.y + 5), IM_COL32(0, 255, 0, 255));
                drawList->AddLine(ImVec2(cross.x - 5, cross.y + 5), ImVec2(cross.x + 5, cross.y - 5), IM_COL32(0, 255, 0, 255));
            }
        }
        ImGui::InvisibleButton("PreviewCanvas", canvasSize);
    }
    ImGui::End();
}

bool PreviewWindow::loadImage()
{
    auto *package = m_owner->package();
    if(!package || !package->setIndex(m_owner->selectedImageIndex())){
        return false;
    }
    const auto width = package->currImageInfo()->width;
    const auto height = package->currImageInfo()->height;
    m_imageOffX = package->currImageInfo()->px;
    m_imageOffY = package->currImageInfo()->py;
    m_imageBuf.assign(width * height, 0);
    const auto layer = package->decode(false, m_owner->removeShadowMosaicEnabled(), m_owner->autoAlphaEnabled());
    if(layer[0] && m_owner->layerIndexEnabled(0)){
        imgf::blendImageBuffer(m_imageBuf.data(), width, height, layer[0], width, height, 0, 0);
    }
    if(layer[1] && m_owner->layerIndexEnabled(1)){
        imgf::blendImageBuffer(m_imageBuf.data(), width, height, layer[1], width, height, 0, 0);
    }
    if(layer[2] && m_owner->layerIndexEnabled(2)){
        imgf::blendImageBuffer(m_imageBuf.data(), width, height, layer[2], width, height, 0, 0);
    }
    m_imageIndex = m_owner->selectedImageIndex();
    m_resizeRequested = true;
    return m_texture.loadRGBA(m_imageBuf.data(), width, height);
}
