#pragma once

#include "textUtil.h"

// For textParameters
#include "style/textStyle.h"
#include "labels/textLabel.h"

#include "alfons/alfons.h"
#include "alfons/fontManager.h"
#include "alfons/atlas.h"
#include "alfons/textBatch.h"
#include "alfons/textShaper.h"
#include "alfons/font.h"
#include "alfons/inputSource.h"

#include "gl/texture.h"

#include <bitset>
#include <mutex>
#include <atomic>

namespace Tangram {

struct FontMetrics {
    float ascender, descender, lineHeight;
};

// TODO could be a shared_ptr<Texture>
struct GlyphTexture {

    static constexpr int size = 256;

    GlyphTexture() : texture(size, size) {
        texData.resize(size * size);
    }

    std::vector<unsigned char> texData;
    Texture texture;

    bool dirty = false;
    size_t refCount = 0;
};

enum class FontType {
    wof,
    ttf,
};

struct FontDescription {
    std::string uri;
    std::string alias;
    std::string bundleAlias;

    FontType type;

    FontDescription(std::string family, std::string style, std::string weight, std::string uri, FontType type = FontType::ttf)
        : uri(uri), type(type) {
        alias = Alias(family, style, weight);
        bundleAlias = BundleAlias(family, style, weight, type);
    }

    static std::string Alias(const std::string& family, const std::string& style, const std::string& weight) {
        return family + "_" + weight + "_" + style;
    }

    static std::string BundleAlias(const std::string& family, const std::string& style, const std::string& weight, FontType type) {
        std::string alias = family + "-" + weight + style;
        switch(type) {
            case FontType::wof: alias += ".woff"; break;
            case FontType::ttf: alias += ".ttf"; break;
        }
        return alias;
    }
};

class FontContext : public alfons::TextureCallback {

public:

    static constexpr int max_textures = 64;

    FontContext();

    /* Synchronized on m_mutex on tile-worker threads
     * Called from alfons when a texture atlas needs to be created
     * Triggered from TextStyleBuilder::prepareLabel
     */
    void addTexture(alfons::AtlasID id, uint16_t width, uint16_t height) override;

    /* Synchronized on m_mutex, called tile-worker threads
     * Called from alfons when a glyph needs to be added the the atlas identified by id
     * Triggered from TextStyleBuilder::prepareLabel
     */
    void addGlyph(alfons::AtlasID id, uint16_t gx, uint16_t gy, uint16_t gw, uint16_t gh,
                  const unsigned char* src, uint16_t pad) override;

    void releaseAtlas(std::bitset<max_textures> _refs);

    alfons::GlyphAtlas& atlas() { return m_atlas; }

    /* Update all textures batches, uploads the data to the GPU */
    void updateTextures();

    std::shared_ptr<alfons::Font> getFont(const std::string& _family, const std::string& _style,
                                          const std::string& _weight, float _size);

    size_t glyphTextureCount() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_textures.size();
    }

    void bindTexture(alfons::AtlasID _id, GLuint _unit);

    float maxStrokeWidth() { return m_sdfRadius; }

    bool layoutText(TextStyle::Parameters& _params, const std::string& _text,
                    std::vector<GlyphQuad>& _quads, std::bitset<max_textures>& _refs, glm::vec2& _bbox);

    struct ScratchBuffer : public alfons::MeshCallback {
        void drawGlyph(const alfons::Quad& q, const alfons::AtlasGlyph& altasGlyph) override {}
        void drawGlyph(const alfons::Rect& q, const alfons::AtlasGlyph& atlasGlyph) override;
        std::vector<GlyphQuad>* quads;
    };

    void setSceneResourceRoot(const std::string& sceneResourceRoot) { m_sceneResourceRoot = sceneResourceRoot; }

    void addFontDescription(FontDescription _ft);

    void download(const FontDescription& _ft);

    bool isLoadingResources() const;

    std::atomic_ushort resourceLoad;

private:

    float m_sdfRadius;
    ScratchBuffer m_scratch;
    std::vector<unsigned char> m_sdfBuffer;

    std::mutex m_mutex;
    std::array<int, max_textures> m_atlasRefCount = {{0}};
    alfons::GlyphAtlas m_atlas;

    alfons::FontManager m_alfons;
    std::array<std::shared_ptr<alfons::Font>, 3> m_font;

    std::vector<GlyphTexture> m_textures;

    // TextShaper to create <LineLayout> for a given text and Font
    alfons::TextShaper m_shaper;

    // TextBatch to 'draw' <LineLayout>s, i.e. creating glyph textures and glyph quads.
    // It is intialized with a TextureCallback implemented by FontContext for adding glyph
    // textures and a MeshCallback implemented by TextStyleBuilder for adding glyph quads.
    alfons::TextBatch m_batch;
    TextWrapper m_textWrapper;
    std::string m_sceneResourceRoot = "";

};

}
