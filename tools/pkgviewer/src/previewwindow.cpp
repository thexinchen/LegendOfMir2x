#include <algorithm>
#include <vector>
#include <imgui.h>
#include "imgf.hpp"
#include "sysconst.hpp"
#include "mainwindow.hpp"
#include "previewwindow.hpp"

void PreviewWindow::draw()
{
    const auto canvasPos = ImGui::GetCursorScreenPos();
    auto canvasSize = ImGui::GetContentRegionAvail();
    canvasSize.x = std::max(1.0f, canvasSize.x);
    canvasSize.y = std::max(1.0f, canvasSize.y);
    auto *drawList = ImGui::GetWindowDrawList();
    if(m_owner->clearBackgroundEnabled()){
        drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32_BLACK);
    }
    if(m_texture.valid()){
        const float scale = std::min(canvasSize.x / m_texture.width(), canvasSize.y / m_texture.height());
        const float imageWidth = m_texture.width() * scale;
        const float imageHeight = m_texture.height() * scale;
        float imageX = canvasPos.x + (canvasSize.x - imageWidth) / 2;
        float imageY = canvasPos.y + (canvasSize.y - imageHeight) / 2;
        if(m_owner->offsetDrawEnabled()){
            imageX = canvasPos.x + canvasSize.x / 2 + (m_imageOffX - SYS_MAPGRIDXP / 2) * scale;
            imageY = canvasPos.y + canvasSize.y / 2 + (m_imageOffY - SYS_MAPGRIDYP / 2) * scale;
            imageX = std::clamp(imageX, canvasPos.x, canvasPos.x + std::max(0.0f, canvasSize.x - imageWidth));
            imageY = std::clamp(imageY, canvasPos.y, canvasPos.y + std::max(0.0f, canvasSize.y - imageHeight));
        }
        const ImVec2 imageMin(imageX, imageY);
        const ImVec2 imageMax(imageX + imageWidth, imageY + imageHeight);
        drawList->AddImage(m_texture.id(), imageMin, imageMax);
        drawList->AddRect(imageMin, imageMax, IM_COL32(255, 0, 0, 255));
        if(m_owner->showOffsetCrossEnabled()){
            const ImVec2 origin(imageX - m_imageOffX * scale, imageY - m_imageOffY * scale);
            const ImVec2 cross(origin.x + SYS_MAPGRIDXP * scale / 2, origin.y + SYS_MAPGRIDYP * scale / 2);
            drawList->AddLine(imageMin, origin, IM_COL32(0, 0, 255, 255));
            drawList->AddLine(origin, cross, IM_COL32(0, 0, 255, 255));
            drawList->AddLine(ImVec2(cross.x - 5, cross.y - 5), ImVec2(cross.x + 5, cross.y + 5), IM_COL32(0, 255, 0, 255));
            drawList->AddLine(ImVec2(cross.x - 5, cross.y + 5), ImVec2(cross.x + 5, cross.y - 5), IM_COL32(0, 255, 0, 255));
        }
    }
    ImGui::InvisibleButton("PreviewCanvas", canvasSize);
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
    return m_texture.loadRGBA(m_imageBuf.data(), width, height);
}

void PreviewWindow::clear()
{
    m_imageBuf.clear();
    m_texture.clear();
}
