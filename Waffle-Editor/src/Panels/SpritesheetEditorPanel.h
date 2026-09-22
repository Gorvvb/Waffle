#pragma once

#include "Waffle/Renderer/Texture.h"
#include "Waffle/Renderer/SubTexture2D.h"

#include <filesystem>
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace Waffle {

    struct SpriteRegion
    {
        std::string Name = "Sprite";
        glm::vec2   Min = { 0.0f,  0.0f }; // Pixel coords in texture space
        glm::vec2   Max = { 32.0f, 32.0f }; // Pixel coords in texture space

        // Custom pivot, normalized within the region (0,0 = top-left,
        // 1,1 = bottom-right, image space y-down). Negative = unset: the
        // frame inherits the animator's Frame Pivot. Set by dragging the
        // pivot handle in the canvas.
        glm::vec2   Pivot = { -1.0f, -1.0f };
    };

    // A named collection of sprite regions (e.g. "Walk", "Idle", "Attack")
    struct SpriteGroup
    {
        std::string      Name = "Group";
        std::vector<int> RegionIndices; // Indices into the region list
    };

    class SpritesheetEditorPanel
    {
    public:
        SpritesheetEditorPanel();
        ~SpritesheetEditorPanel() = default;

        void OnImGuiRender();

        // Opens a texture or .spritesheet asset for editing. Textures with
        // sibling sprite metadata load that metadata instead of starting
        // from a blank sheet.
        void Open(const std::filesystem::path& path);

    private:
        // Texture
        std::filesystem::path m_TexturePath;
        Ref<Texture2D>        m_Texture;

        // Auto-slice grid
        int m_GridCols = 1;
        int m_GridRows = 1;
        int m_PaddingX = 0;
        int m_PaddingY = 0;

        // Region list
        std::vector<SpriteRegion> m_Regions;
        int                       m_SelectedRegionIndex = -1;

        // Named groups of regions
        std::vector<SpriteGroup> m_Groups;
        int                      m_SelectedGroupIndex = -1;

        // Canvas drag-draw state (pixel-space coords)
        bool      m_IsDraggingBox = false;
        glm::vec2 m_DragStartPixel = { 0.0f, 0.0f };
        glm::vec2 m_DragCurrentPixel = { 0.0f, 0.0f };

        // Edge resize drag: index of the region being resized plus a
        // bitmask of the edges being dragged (1=L, 2=R, 4=T, 8=B).
        int       m_ResizeRegionIndex = -1;
        int       m_ResizeEdges = 0;

        // Pivot handle drag: index of the region whose pivot is dragged.
        int       m_PivotDragRegionIndex = -1;

        // Canvas pan/zoom
        float     m_CanvasZoom = 1.0f;
        glm::vec2 m_CanvasOffset = { 0.0f, 0.0f };
        bool      m_IsPanning = false;
        glm::vec2 m_PanLastMouse = { 0.0f, 0.0f }; // glm, not ImVec2

        void LoadTexture(const std::filesystem::path& path);
        void LoadSpritesheetAsset(const std::filesystem::path& path);
        void HandleContentBrowserDrop();
        void RemoveRegion(int index);
        void AutoSliceGrid();
        void SaveSpritesheetAsset();
    };

}