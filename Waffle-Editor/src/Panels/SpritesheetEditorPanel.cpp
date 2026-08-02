#include "wfpch.h"
#include "SpritesheetEditorPanel.h"
#include "Waffle/Utils/PlatformUtils.h"
#include "Waffle/Core/Log.h"

#include <glad/glad.h>

#include <imgui/imgui.h>
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <algorithm>


namespace Waffle {

    extern std::filesystem::path g_AssetPath;

    // =========================================================================
    //  Construction
    // =========================================================================

    SpritesheetEditorPanel::SpritesheetEditorPanel() {}

    // =========================================================================
    //  Private helpers
    // =========================================================================

    void SpritesheetEditorPanel::LoadTexture(const std::filesystem::path& path)
    {
        m_TexturePath = path;
        m_Texture = Texture2D::Create(path.string(), TextureFilter::Nearest);
        m_CanvasZoom = 1.0f;
        m_CanvasOffset = { 0.0f, 0.0f };
        AutoSliceGrid();
    }

    void SpritesheetEditorPanel::AutoSliceGrid()
    {
        if (!m_Texture || m_GridCols <= 0 || m_GridRows <= 0) return;

        m_Regions.clear();
        const float totalW = (float)m_Texture->GetWidth();
        const float totalH = (float)m_Texture->GetHeight();

        float cellW = (totalW - m_PaddingX * (m_GridCols + 1)) / (float)m_GridCols;
        float cellH = (totalH - m_PaddingY * (m_GridRows + 1)) / (float)m_GridRows;
        if (cellW <= 0.0f) cellW = totalW / (float)m_GridCols;
        if (cellH <= 0.0f) cellH = totalH / (float)m_GridRows;

        int counter = 0;
        for (int r = 0; r < m_GridRows; r++)
        {
            for (int c = 0; c < m_GridCols; c++)
            {
                SpriteRegion region;
                region.Name = "Sprite_" + std::to_string(counter++);
                const float minX = m_PaddingX + c * (cellW + m_PaddingX);
                const float minY = m_PaddingY + r * (cellH + m_PaddingY);
                region.Min = { minX,         minY };
                region.Max = { minX + cellW, minY + cellH };
                m_Regions.push_back(region);
            }
        }
    }

    void SpritesheetEditorPanel::SaveSpritesheetAsset()
    {
        if (!m_Texture || m_TexturePath.empty()) return;

        const std::filesystem::path sheetPath =
            m_TexturePath.parent_path() /
            (m_TexturePath.stem().string() + ".spritesheet");

        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Spritesheet" << YAML::Value << m_TexturePath.filename().string();
        out << YAML::Key << "Columns" << YAML::Value << m_GridCols;
        out << YAML::Key << "Rows" << YAML::Value << m_GridRows;
        out << YAML::Key << "PaddingX" << YAML::Value << m_PaddingX;
        out << YAML::Key << "PaddingY" << YAML::Value << m_PaddingY;

        out << YAML::Key << "Regions" << YAML::Value << YAML::BeginSeq;
        for (const auto& region : m_Regions)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "Name" << YAML::Value << region.Name;
            out << YAML::Key << "Min" << YAML::Value << YAML::Flow
                << YAML::BeginSeq << (int)region.Min.x << (int)region.Min.y << YAML::EndSeq;
            out << YAML::Key << "Max" << YAML::Value << YAML::Flow
                << YAML::BeginSeq << (int)region.Max.x << (int)region.Max.y << YAML::EndSeq;
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;
        out << YAML::EndMap;

        std::ofstream fout(sheetPath);
        fout << out.c_str();
        WF_CORE_INFO("Saved spritesheet asset: {0}", sheetPath.string());
    }

    // =========================================================================
    //  Main render
    // =========================================================================

    void SpritesheetEditorPanel::OnImGuiRender()
    {
        ImGui::Begin("Spritesheet Editor");

        // ── Top toolbar ───────────────────────────────────────────────────────
        if (ImGui::Button("Browse..."))
        {
            const std::string file = FileDialogs::OpenFile(
                "Image Files (*.png *.jpg *.jpeg)\0*.png;*.jpg;*.jpeg\0All Files (*.*)\0*.*\0");
            if (!file.empty())
                LoadTexture(file);
        }

        ImGui::SameLine();
        if (m_Texture)
            ImGui::TextDisabled("%s  (%dx%d)",
                m_TexturePath.filename().string().c_str(),
                m_Texture->GetWidth(), m_Texture->GetHeight());
        else
            ImGui::TextDisabled("No texture loaded — drag a PNG here or use Browse");

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 90.0f);
        if (ImGui::Button("Save Asset") && m_Texture)
            SaveSpritesheetAsset();

        ImGui::Separator();

        // ── Grid slice controls ───────────────────────────────────────────────
        ImGui::SetNextItemWidth(60);
        if (ImGui::DragInt("Cols", &m_GridCols, 1, 1, 64)) AutoSliceGrid();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60);
        if (ImGui::DragInt("Rows", &m_GridRows, 1, 1, 64)) AutoSliceGrid();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60);
        if (ImGui::DragInt("PadX", &m_PaddingX, 1, 0, 64)) AutoSliceGrid();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60);
        if (ImGui::DragInt("PadY", &m_PaddingY, 1, 0, 64)) AutoSliceGrid();
        ImGui::SameLine();
        if (ImGui::Button("Auto Slice")) AutoSliceGrid();
        ImGui::SameLine();
        if (ImGui::Button("Clear All")) { m_Regions.clear(); m_SelectedRegionIndex = -1; }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90);
        if (ImGui::DragFloat("Zoom", &m_CanvasZoom, 0.05f, 0.1f, 8.0f, "%.2fx"))
            m_CanvasZoom = std::max(0.1f, std::min(m_CanvasZoom, 8.0f));
        ImGui::SameLine();
        ImGui::TextDisabled("(Scroll=zoom  MMB=pan)");

        ImGui::Separator();

        // ── Layout sizes ──────────────────────────────────────────────────────
        const float availH = ImGui::GetContentRegionAvail().y;
        const float inspW = 260.0f;
        const float canvasW = ImGui::GetContentRegionAvail().x - inspW - 8.0f;

        // =====================================================================
        //  CANVAS child window
        // =====================================================================
        ImGui::BeginChild("##CanvasChild",
            ImVec2(canvasW, availH), true,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        if (!m_Texture)
        {
            // ── Empty drop-zone ───────────────────────────────────────────────
            const ImVec2 zoneMin = ImGui::GetCursorScreenPos();
            const ImVec2 zoneMax = { zoneMin.x + canvasW - 2.0f,
                                     zoneMin.y + availH - 2.0f };
            ImDrawList* dl = ImGui::GetWindowDrawList();

            dl->AddRectFilled(zoneMin, zoneMax, IM_COL32(30, 30, 30, 255));
            dl->AddRect(zoneMin, zoneMax, IM_COL32(80, 80, 80, 200), 4.0f, 0, 1.5f);

            // Centred hint text
            const char* hint1 = "Drop a PNG / JPG here";
            const char* hint2 = "or use  Browse...  above";
            const ImVec2 sz1 = ImGui::CalcTextSize(hint1);
            const ImVec2 sz2 = ImGui::CalcTextSize(hint2);
            const float  cx = zoneMin.x + (zoneMax.x - zoneMin.x) * 0.5f;
            const float  cy = zoneMin.y + (zoneMax.y - zoneMin.y) * 0.5f;
            dl->AddText({ cx - sz1.x * 0.5f, cy - sz1.y - 4.0f },
                IM_COL32(140, 140, 140, 200), hint1);
            dl->AddText({ cx - sz2.x * 0.5f, cy + 4.0f },
                IM_COL32(100, 100, 100, 180), hint2);

            // Animated border
            const float  t = (float)ImGui::GetTime();
            const ImU32  col = IM_COL32(
                (int)(120 + 80 * sinf(t * 2.0f)),
                (int)(120 + 80 * sinf(t * 2.0f + 1.0f)),
                180, 200);
            dl->AddRect({ zoneMin.x + 8.0f, zoneMin.y + 8.0f },
                { zoneMax.x - 8.0f, zoneMax.y - 8.0f },
                col, 6.0f, 0, 1.0f);

            ImGui::InvisibleButton("##DropZone",
                ImVec2(canvasW - 2.0f, availH - 2.0f));

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload =
                    ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                {
                    const wchar_t* pathStr = (const wchar_t*)payload->Data;
                    std::filesystem::path path =
                        std::filesystem::path(g_AssetPath) / pathStr;
                    std::string ext = path.extension().string();
                    for (auto& c : ext) c = (char)tolower(c);
                    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg")
                        LoadTexture(path);
                }
                ImGui::EndDragDropTarget();
            }
        }
        else
        {
            // ── Texture canvas ────────────────────────────────────────────────
            const float texW = (float)m_Texture->GetWidth();
            const float texH = (float)m_Texture->GetHeight();

            // Fit-to-canvas base scale, then apply user zoom
            const float fitScale = std::min((canvasW - 16.0f) / texW,
                (availH - 16.0f) / texH);
            const float displayW = texW * fitScale * m_CanvasZoom;
            const float displayH = texH * fitScale * m_CanvasZoom;

            // Centre image in child window + apply pan offset
            const ImVec2 childTL = ImGui::GetCursorScreenPos();
            const ImVec2 canvasPos = {
                childTL.x + (canvasW - displayW) * 0.5f + m_CanvasOffset.x,
                childTL.y + (availH - displayH) * 0.5f + m_CanvasOffset.y
            };

            // Coordinate helpers as lambdas — ImVec2 is available here
            auto PixelToScreen = [&](const glm::vec2& px) -> ImVec2
                {
                    return {
                        canvasPos.x + (px.x / texW) * displayW,
                        canvasPos.y + (px.y / texH) * displayH
                    };
                };

            auto ScreenToPixel = [&](const ImVec2& scr) -> glm::vec2
                {
                    const float px = ((scr.x - canvasPos.x) / displayW) * texW;
                    const float py = ((scr.y - canvasPos.y) / displayH) * texH;
                    return {
                        std::max(0.0f, std::min(px, texW)),
                        std::max(0.0f, std::min(py, texH))
                    };
                };

            ImDrawList* dl = ImGui::GetWindowDrawList();

            // Checkerboard background
            {
                const float  cell = 8.0f;
                const ImVec2 p0 = canvasPos;
                const ImVec2 p1 = { canvasPos.x + displayW, canvasPos.y + displayH };
                dl->PushClipRect(p0, p1, true);
                for (float gy = p0.y; gy < p1.y; gy += cell)
                    for (float gx = p0.x; gx < p1.x; gx += cell)
                    {
                        const bool even =
                            ((int)((gx - p0.x) / cell) +
                                (int)((gy - p0.y) / cell)) % 2 == 0;
                        dl->AddRectFilled(
                            { gx, gy },
                            { std::min(gx + cell, p1.x),
                              std::min(gy + cell, p1.y) },
                            even ? IM_COL32(60, 60, 60, 255)
                            : IM_COL32(80, 80, 80, 255));
                    }
                dl->PopClipRect();
            }

            // Texture image (flip V for OpenGL)
            GLuint texID = (GLuint)m_Texture->GetRendererID();

            dl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
                // Unbind any sampler object so texture params take effect
                glBindSampler(0, 0);
            }, nullptr);

            dl->AddImage(
                (ImTextureID)(uintptr_t)texID,
                canvasPos,
                { canvasPos.x + displayW, canvasPos.y + displayH },
                ImVec2(0, 1), ImVec2(1, 0));

            // Restore after the image — reset is needed so ImGui's own
            // sampler state (if any) resumes for subsequent draw commands
            dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

            // Region overlays
            for (int i = 0; i < (int)m_Regions.size(); i++)
            {
                const auto& reg = m_Regions[i];
                const ImVec2 pMin = PixelToScreen(reg.Min);
                const ImVec2 pMax = PixelToScreen(reg.Max);
                const bool   sel = (m_SelectedRegionIndex == i);
                const ImU32  col = sel ? IM_COL32(0, 230, 120, 255)
                    : IM_COL32(255, 60, 60, 200);
                const ImU32  fillCol = sel ? IM_COL32(0, 230, 120, 28)
                    : IM_COL32(255, 60, 60, 14);
                dl->AddRectFilled(pMin, pMax, fillCol);
                dl->AddRect(pMin, pMax, col, 0.0f, 0,
                    sel ? 2.5f : 1.2f);
                if ((pMax.x - pMin.x) > 24.0f)
                    dl->AddText({ pMin.x + 3.0f, pMin.y + 2.0f },
                        col, reg.Name.c_str());
            }

            // Invisible interaction button over the whole canvas area
            ImGui::SetCursorScreenPos(childTL);
            ImGui::InvisibleButton("##Canvas",
                ImVec2(canvasW - 2.0f, availH - 2.0f),
                ImGuiButtonFlags_MouseButtonLeft |
                ImGuiButtonFlags_MouseButtonMiddle);

            const ImGuiIO& io = ImGui::GetIO();
            const ImVec2   mousePos = io.MousePos;
            const bool     hovered = ImGui::IsItemHovered();

            // Scroll-wheel zoom
            if (hovered && io.MouseWheel != 0.0f)
            {
                const float factor = (io.MouseWheel > 0.0f) ? 1.1f : (1.0f / 1.1f);
                m_CanvasZoom = std::max(0.1f, std::min(m_CanvasZoom * factor, 8.0f));
            }

            // Middle-mouse pan — store in glm::vec2, convert only when needed
            if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
            {
                m_IsPanning = true;
                m_PanLastMouse = { mousePos.x, mousePos.y };
            }
            if (m_IsPanning)
            {
                if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
                {
                    m_CanvasOffset.x += mousePos.x - m_PanLastMouse.x;
                    m_CanvasOffset.y += mousePos.y - m_PanLastMouse.y;
                    m_PanLastMouse = { mousePos.x, mousePos.y };
                }
                else
                {
                    m_IsPanning = false;
                }
            }

            // Left-click: select existing region or start drag-draw
            const bool insideImage =
                mousePos.x >= canvasPos.x &&
                mousePos.x <= canvasPos.x + displayW &&
                mousePos.y >= canvasPos.y &&
                mousePos.y <= canvasPos.y + displayH;

            if (hovered && insideImage &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                const glm::vec2 clickPx = ScreenToPixel(mousePos);
                int hit = -1;
                for (int i = (int)m_Regions.size() - 1; i >= 0; i--)
                {
                    if (clickPx.x >= m_Regions[i].Min.x &&
                        clickPx.x <= m_Regions[i].Max.x &&
                        clickPx.y >= m_Regions[i].Min.y &&
                        clickPx.y <= m_Regions[i].Max.y)
                    {
                        hit = i;
                        break;
                    }
                }

                if (hit >= 0)
                {
                    m_SelectedRegionIndex = hit;
                }
                else
                {
                    m_IsDraggingBox = true;
                    m_DragStartPixel = { std::round(clickPx.x), std::round(clickPx.y) };
                    m_DragCurrentPixel = m_DragStartPixel;
                    m_SelectedRegionIndex = -1;
                }
            }

            // Active drag-draw
            if (m_IsDraggingBox)
            {
                const glm::vec2 rawPx = ScreenToPixel(mousePos);
                m_DragCurrentPixel = { std::round(rawPx.x), std::round(rawPx.y) };

                const ImVec2 p0 = PixelToScreen(m_DragStartPixel);
                const ImVec2 p1 = PixelToScreen(m_DragCurrentPixel);
                dl->AddRectFilled(p0, p1, IM_COL32(255, 240, 0, 20));
                dl->AddRect(p0, p1, IM_COL32(255, 240, 0, 255), 0.0f, 0, 2.0f);

                // Live pixel-size tooltip
                char tip[64];
                snprintf(tip, sizeof(tip), "%dx%d px",
                    (int)std::abs(m_DragCurrentPixel.x - m_DragStartPixel.x),
                    (int)std::abs(m_DragCurrentPixel.y - m_DragStartPixel.y));
                dl->AddText({ p1.x + 6.0f, p1.y + 6.0f },
                    IM_COL32(255, 240, 0, 255), tip);

                if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
                {
                    m_IsDraggingBox = false;

                    const glm::vec2 pMin = {
                        std::floor(std::min(m_DragStartPixel.x, m_DragCurrentPixel.x)),
                        std::floor(std::min(m_DragStartPixel.y, m_DragCurrentPixel.y))
                    };
                    const glm::vec2 pMax = {
                        std::ceil(std::max(m_DragStartPixel.x, m_DragCurrentPixel.x)),
                        std::ceil(std::max(m_DragStartPixel.y, m_DragCurrentPixel.y))
                    };

                    if ((pMax.x - pMin.x) >= 4.0f && (pMax.y - pMin.y) >= 1.0f)
                    {
                        SpriteRegion reg;
                        reg.Name = "Region_" + std::to_string(m_Regions.size());
                        reg.Min = pMin;
                        reg.Max = pMax;
                        m_Regions.push_back(reg);
                        m_SelectedRegionIndex = (int)m_Regions.size() - 1;
                    }
                }
            }

            // Allow dropping a replacement texture onto the canvas
            ImGui::SetCursorScreenPos(childTL);
            ImGui::InvisibleButton("##CanvasDrop",
                ImVec2(canvasW - 2.0f, availH - 2.0f));
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload =
                    ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                {
                    const wchar_t* pathStr = (const wchar_t*)payload->Data;
                    std::filesystem::path path =
                        std::filesystem::path(g_AssetPath) / pathStr;
                    std::string ext = path.extension().string();
                    for (auto& c : ext) c = (char)tolower(c);
                    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg")
                        LoadTexture(path);
                }
                ImGui::EndDragDropTarget();
            }
        }

        ImGui::EndChild(); // CanvasChild

        // =====================================================================
        //  INSPECTOR  (right panel)
        // =====================================================================
        ImGui::SameLine();
        ImGui::BeginChild("##Inspector", ImVec2(inspW, availH), true);

        ImGui::TextDisabled("Regions  (%zu)", m_Regions.size());
        ImGui::SameLine();

        const float texWInsp = m_Texture ? (float)m_Texture->GetWidth() : 4096.0f;
        const float texHInsp = m_Texture ? (float)m_Texture->GetHeight() : 4096.0f;

        if (ImGui::SmallButton("+ Manual"))
        {
            SpriteRegion reg;
            reg.Name = "Region_" + std::to_string(m_Regions.size());
            reg.Min = { 0.0f, 0.0f };
            reg.Max = { std::min(32.0f, texWInsp), std::min(32.0f, texHInsp) };
            m_Regions.push_back(reg);
            m_SelectedRegionIndex = (int)m_Regions.size() - 1;
        }

        // Region list
        ImGui::BeginChild("##RegionList", ImVec2(0, 180.0f), true);
        for (int i = 0; i < (int)m_Regions.size(); i++)
        {
            ImGui::PushID(i);
            const bool sel = (m_SelectedRegionIndex == i);
            const std::string lbl =
                m_Regions[i].Name
                + "  [" + std::to_string((int)m_Regions[i].Min.x)
                + "," + std::to_string((int)m_Regions[i].Min.y) + "]";
            if (ImGui::Selectable(lbl.c_str(), sel))
                m_SelectedRegionIndex = i;
            ImGui::PopID();
        }
        ImGui::EndChild();

        // Selected region details
        if (m_SelectedRegionIndex >= 0 &&
            m_SelectedRegionIndex < (int)m_Regions.size())
        {
            auto& reg = m_Regions[m_SelectedRegionIndex];

            ImGui::Separator();
            ImGui::Text("Region Properties");

            char nameBuf[64];
            strcpy_s(nameBuf, sizeof(nameBuf), reg.Name.c_str());
            if (ImGui::InputText("Name##reg", nameBuf, sizeof(nameBuf)))
                reg.Name = nameBuf;

            if (ImGui::DragFloat2("Min  (px)", &reg.Min.x, 1.0f, 0.0f, texWInsp))
            {
                reg.Min.x = std::round(reg.Min.x);
                reg.Min.y = std::round(reg.Min.y);
            }
            if (ImGui::DragFloat2("Max  (px)", &reg.Max.x, 1.0f, 0.0f, texHInsp))
            {
                reg.Max.x = std::round(reg.Max.x);
                reg.Max.y = std::round(reg.Max.y);
            }

            const glm::vec2 sz = reg.Max - reg.Min;
            ImGui::TextDisabled("Size: %d x %d px", (int)sz.x, (int)sz.y);

            ImGui::Spacing();
            if (ImGui::Button("Delete##reg", ImVec2(-1, 0)))
            {
                m_Regions.erase(m_Regions.begin() + m_SelectedRegionIndex);
                m_SelectedRegionIndex = -1;
            }
        }

        ImGui::EndChild(); // Inspector

        ImGui::End();
    }

}