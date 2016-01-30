#pragma once

#include "vboMesh.h"

#include <cstring> // for memcpy
#include <cassert>

namespace Tangram {

template<class T>
struct MeshData {
    std::vector<std::pair<uint32_t, uint32_t>> offsets;
    std::vector<T> vertices;
    std::vector<uint16_t> indices;

    void clear() {
        offsets.clear();
        indices.clear();
        vertices.clear();
    }
};

template<class T>
class TypedMesh : public VboMesh {

public:

    TypedMesh(std::shared_ptr<VertexLayout> _vertexLayout, GLenum _drawMode,
              GLenum _hint = GL_STATIC_DRAW, bool _keepMemoryData = false)
        : VboMesh(_vertexLayout, _drawMode, _hint, _keepMemoryData) {};

    /*
     * Update _nVerts vertices in the mesh with the new T value _newVertexValue
     * starting after _byteOffset in the mesh vertex data memory
     */
    void updateVertices(Range _vertexRange, const T& _newVertexValue);

    /*
     * Update _nVerts vertices in the mesh with the new attribute A
     * _newAttributeValue starting after _byteOffset in the mesh vertex data
     * memory
     */

    template<class A>
    void updateAttribute(Range _vertexRange, const A& _newAttributeValue, size_t _attribOffset = 0);

    void compile(std::vector<std::vector<T>>&& _vertices,
                 std::vector<std::vector<uint16_t>>&& _indices);

    void compile(const std::vector<MeshData<T>>& _meshes);

protected:

    void setDirty(GLintptr _byteOffset, GLsizei _byteSize);

    GLushort* compileIndices(GLushort* _dst, const MeshData<T>& _data);

};


// Add indices by collecting them into batches to draw as much as
// possible in one draw call.  The indices must be shifted by the
// number of vertices that are present in the current batch.
template<class T>
GLushort* TypedMesh<T>::compileIndices(GLushort* _dst, const MeshData<T>& _data) {
    m_vertexOffsets.emplace_back(0, 0);

    size_t curVertices = 0;
    size_t src = 0;

    for (auto& p : _data.offsets) {
        size_t nIndices = p.first;
        size_t nVertices = p.second;

        if (curVertices + nVertices > MAX_INDEX_VALUE) {
            m_vertexOffsets.emplace_back(0, 0);
            curVertices = 0;
        }
        for (size_t i = 0; i < nIndices; i++, _dst++) {
            *_dst = _data.indices[src++] + curVertices;
        }
        auto& offset = m_vertexOffsets.back();
        offset.first += nIndices;
        offset.second += nVertices;

        curVertices += nVertices;
    }
    return _dst;
}

template<class T>
void TypedMesh<T>::compile(const std::vector<MeshData<T>>& _meshes) {
    m_isCompiled = true;

    m_nVertices = 0;
    m_nIndices = 0;

    for (auto& m : _meshes) {
        m_nVertices += m.vertices.size();
        m_nIndices += m.indices.size();
    }

    int stride = m_vertexLayout->getStride();
    m_glVertexData = new GLbyte[m_nVertices * stride];
    m_glIndexData = new GLushort[m_nIndices];

    size_t byteOffset = 0;
    for (auto& m : _meshes) {
        std::memcpy(m_glVertexData + byteOffset,
                    reinterpret_cast<const GLbyte*>(m.vertices.data()),
                    m.vertices.size() * stride);

        byteOffset += m.vertices.size() * stride;
    }

    GLushort* dst = m_glIndexData;
    for (auto& m : _meshes) {
        dst = compileIndices(dst, m);
    }

    assert(dst == m_glIndexData + m_nIndices);
}

template<class T>
void TypedMesh<T>::compile(std::vector<std::vector<T>>&& _vertices,
                           std::vector<std::vector<uint16_t>>&& _indices) {

    std::vector<std::vector<T>> vertices;
    std::vector<std::vector<uint16_t>> indices;

    // take over contents
    std::swap(_vertices, vertices);
    std::swap(_indices, indices);

    int vertexOffset = 0, indexOffset = 0;

    // Buffer positions: vertex byte and index short
    int vPos = 0, iPos = 0;

    int stride = m_vertexLayout->getStride();
    m_glVertexData = new GLbyte[stride * m_nVertices];

    bool useIndices = m_nIndices > 0;
    if (useIndices) {
        m_glIndexData = new GLushort[m_nIndices];
    }

    for (size_t i = 0; i < vertices.size(); i++) {
        auto& curVertices = vertices[i];
        size_t nVertices = curVertices.size();
        int nBytes = nVertices * stride;

        std::memcpy(m_glVertexData + vPos, (GLbyte*)curVertices.data(), nBytes);
        vPos += nBytes;

        if (useIndices) {
            if (vertexOffset + nVertices > MAX_INDEX_VALUE) {
                //LOGD("Big Mesh %d\n", vertexOffset + nVertices);

                m_vertexOffsets.emplace_back(indexOffset, vertexOffset);
                vertexOffset = 0;
                indexOffset = 0;
            }

            for (int idx : indices[i]) {
                m_glIndexData[iPos++] = idx + vertexOffset;
            }
            indexOffset += indices[i].size();
        }
        vertexOffset += nVertices;
    }

    m_vertexOffsets.emplace_back(indexOffset, vertexOffset);

    m_isCompiled = true;
}

template<class T>
template<class A>
void TypedMesh<T>::updateAttribute(Range _vertexRange, const A& _newAttributeValue, size_t _attribOffset) {
    if (m_glVertexData == nullptr) {
        assert(false);
        return;
    }

    const size_t aSize = sizeof(A);
    const size_t tSize = sizeof(T);
    static_assert(aSize <= tSize, "Invalid attribute size");

    if (_vertexRange.start < 0 || _vertexRange.length < 1) {
        return;
    }
    if (size_t(_vertexRange.start + _vertexRange.length) > m_nVertices) {
        //LOGW("Invalid range");
        return;
    }
    if (_attribOffset >= tSize) {
        //LOGW("Invalid attribute offset");
        return;
    }

    size_t start = _vertexRange.start * tSize + _attribOffset;
    size_t end = start + _vertexRange.length * tSize;

    // update the vertices attributes
    for (size_t offset = start; offset < end; offset += tSize) {
        std::memcpy(m_glVertexData + offset, &_newAttributeValue, aSize);
    }

    // set all modified vertices dirty
    setDirty(start, (_vertexRange.length - 1) * tSize + aSize);
}

template<class T>
void TypedMesh<T>::setDirty(GLintptr _byteOffset, GLsizei _byteSize) {

    if (!m_dirty) {
        m_dirty = true;

        m_dirtySize = _byteSize;
        m_dirtyOffset = _byteOffset;

    } else {
        size_t end = std::max(m_dirtyOffset + m_dirtySize, _byteOffset + _byteSize);

        m_dirtyOffset = std::min(m_dirtyOffset, _byteOffset);
        m_dirtySize = end - m_dirtyOffset;
    }
}

template<class T>
void TypedMesh<T>::updateVertices(Range _vertexRange, const T& _newVertexValue) {
    if (m_glVertexData == nullptr) {
        assert(false);
        return;
    }

    size_t tSize = sizeof(T);

    if (_vertexRange.start + _vertexRange.length > int(m_nVertices)) {
        return;
    }


    size_t start = _vertexRange.start * tSize;
    size_t end = start + _vertexRange.length * tSize;

    // update the vertices
    for (size_t offset = start; offset < end; offset += tSize) {
        std::memcpy(m_glVertexData + offset, &_newVertexValue, tSize);
    }

    setDirty(start, end - start);
}

}
