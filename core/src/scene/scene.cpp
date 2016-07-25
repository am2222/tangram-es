#include "scene.h"

#include "data/dataSource.h"
#include "gl/shaderProgram.h"
#include "platform.h"
#include "scene/dataLayer.h"
#include "scene/light.h"
#include "scene/spriteAtlas.h"
#include "scene/stops.h"
#include "style/material.h"
#include "style/style.h"
#include "text/fontContext.h"
#include "util/mapProjection.h"
#include "util/util.h"
#include "view/view.h"

#include <atomic>
#include <algorithm>
#include <regex>

namespace Tangram {

static std::atomic<int32_t> s_serial;

Scene::Scene(const std::string& _path)
    : id(s_serial++),
      m_path(_path),
      m_fontContext(std::make_shared<FontContext>()) {

    resourceLoad = 0;

    std::regex r("^(http|https):/");
    std::smatch match;

    if (std::regex_search(_path, match, r)) {
        m_resourceRoot = "";
        m_path = _path;
    } else {

        auto split = _path.find_last_of("/");
        if (split == std::string::npos) {
            m_resourceRoot = "";
            m_path = _path;
        } else {
            m_resourceRoot = _path.substr(0, split + 1);
            m_path = _path.substr(split + 1);
        }
    }

    LOGD("Scene '%s' => '%s' : '%s'", _path.c_str(), m_resourceRoot.c_str(), m_path.c_str());

    m_fontContext->setSceneResourceRoot(m_resourceRoot);

    // For now we only have one projection..
    // TODO how to share projection with view?
    m_mapProjection.reset(new MercatorProjection());
}

Scene::Scene(const Scene& _other)
    : id(s_serial++) {

    m_config = _other.m_config;
    m_fontContext = _other.m_fontContext;

    m_path = _other.m_path;
    m_resourceRoot = _other.m_resourceRoot;

    m_mapProjection.reset(new MercatorProjection());
}

Scene::~Scene() {}

const Style* Scene::findStyle(const std::string& _name) const {

    for (auto& style : m_styles) {
        if (style->getName() == _name) { return style.get(); }
    }
    return nullptr;

}

Style* Scene::findStyle(const std::string& _name) {

    for (auto& style : m_styles) {
        if (style->getName() == _name) { return style.get(); }
    }
    return nullptr;
}

int Scene::addIdForName(const std::string& _name) {
    int id = getIdForName(_name);

    if (id < 0) {
        m_names.push_back(_name);
        return m_names.size() - 1;
    }
    return id;
}

int Scene::getIdForName(const std::string& _name) const {
    auto it = std::find(m_names.begin(), m_names.end(), _name);
    if (it == m_names.end()) {
        return -1;
    }
    return it - m_names.begin();
}

const Light* Scene::findLight(const std::string &_name) const {
    for (auto& light : m_lights) {
        if (light->getInstanceName() == _name) { return light.get(); }
    }
    return nullptr;
}

std::shared_ptr<Texture> Scene::getTexture(const std::string& textureName) const {
    auto texIt = m_textures.find(textureName);
    if (texIt == m_textures.end()) {
        return nullptr;
    }
    return texIt->second;
}

std::shared_ptr<DataSource> Scene::getDataSource(const std::string& name) {
    auto it = std::find_if(m_dataSources.begin(), m_dataSources.end(),
                           [&](auto& s){ return s->name() == name; });
    if (it != m_dataSources.end()) {
        return *it;
    }
    return nullptr;
}

}
