#include "builders.h"

#include "tesselator.h"
#include "rectangle.h"
#include "geom.h"
#include "glm/gtx/rotate_vector.hpp"

#include <memory>

void* alloc(void* _userData, unsigned int _size) {
    return malloc(_size);
}

void* realloc(void* _userData, void* _ptr, unsigned int _size) {
    return realloc(_ptr, _size);
}

void free(void* _userData, void* _ptr) {
    free(_ptr);
}

static TESSalloc allocator = {&alloc, &realloc, &free, nullptr,
                              64, // meshEdgeBucketSize
                              64, // meshVertexBucketSize
                              16,  // meshFaceBucketSize
                              64, // dictNodeBucketSize
                              16,  // regionBucketSize
                              64  // extraVertices
                             };

void Builders::buildPolygon(const Polygon& _polygon, PolygonOutput& _out) {
    
    TESStesselator* tesselator = tessNewTess(&allocator);
    
    bool useTexCoords = false; // FIXME (&_out.texcoords != &NO_TEXCOORDS);
    
    // get the number of vertices already added
    // int vertexDataOffset = _out.numVertices; //(int)_out.points.size();
    
    Rectangle bBox;
    
    if (useTexCoords && _polygon.size() > 0 && _polygon[0].size() > 0) {
        // initialize the axis-aligned bounding box of the polygon
        bBox.set(_polygon[0][0].x, _polygon[0][0].y, 0, 0);
    }
    
    // add polygon contour for every ring
    for (auto& line : _polygon) {
        if (useTexCoords) {
            bBox.growToInclude(line);
        }
        tessAddContour(tesselator, 3, line.data(), sizeof(Point), (int)line.size());
    }
    
    // call the tesselator
    glm::vec3 normal(0.0, 0.0, 1.0);
    
    if ( tessTesselate(tesselator, TessWindingRule::TESS_WINDING_NONZERO, TessElementType::TESS_POLYGONS, 3, 3, &normal[0]) ) {
        
        const int numElements = tessGetElementCount(tesselator);
        const TESSindex* tessElements = tessGetElements(tesselator);
        _out.indices.reserve(_out.indices.size() + numElements * 3); // Pre-allocate index vector
        for (int i = 0; i < numElements; i++) {
            const TESSindex* tessElement = &tessElements[i * 3];
            for (int j = 0; j < 3; j++) {
                _out.indices.push_back(tessElement[j] + _out.numVertices);
            }
        }
        
        const int numVertices = tessGetVertexCount(tesselator);
        const float* tessVertices = tessGetVertices(tesselator);

        _out.numVertices += numVertices;
        _out.sizeHint(_out.numVertices);

        for (int i = 0; i < numVertices; i++) {
            glm::vec3 coord(tessVertices[3*i], tessVertices[3*i+1], tessVertices[3*i+2]);
            glm::vec2 uv(0);

            if (useTexCoords) {
                float u = mapValue(tessVertices[3*i], bBox.getMinX(), bBox.getMaxX(), 0., 1.);
                float v = mapValue(tessVertices[3*i+1], bBox.getMinY(), bBox.getMaxY(), 0., 1.);
                uv = glm::vec2(u, v);
            }
            _out.addVertex(coord, normal, uv);
        }
    } else {
        logMsg("Tesselator cannot tesselate!!\n");
    }

    tessDeleteTess(tesselator);
}

void Builders::buildPolygonExtrusion(const Polygon& _polygon, const float& _minHeight, PolygonOutput& _out) {
    
    int vertexDataOffset = (int)_out.numVertices;
    
    glm::vec3 upVector(0.0f, 0.0f, 1.0f);
    glm::vec3 normalVector;
    
    for (auto& line : _polygon) {
        
        size_t lineSize = line.size();
        _out.indices.reserve(_out.indices.size() + lineSize * 6); // Pre-allocate index vector

        _out.numVertices += (lineSize - 1) * 4;
        _out.sizeHint(_out.numVertices);

        for (size_t i = 0; i < lineSize - 1; i++) {
            
            normalVector = glm::cross(upVector, (line[i+1] - line[i]));
            normalVector = glm::normalize(normalVector);
            
            // 1st vertex top
            _out.addVertex(line[i], normalVector, glm::vec2(1.,0.));

            // 2nd vertex top
            _out.addVertex(line[i+1], normalVector, glm::vec2(0.,0.));

            // 1st vertex bottom
            _out.addVertex(glm::vec3(line[i].x, line[i].y, _minHeight),
                           normalVector, glm::vec2(1.,1.));

            // 2nd vertex bottom
            _out.addVertex(glm::vec3(line[i+1].x, line[i+1].y, _minHeight),
                           normalVector, glm::vec2(0.,1.));

            // Start the index from the previous state of the vertex Data
            _out.indices.push_back(vertexDataOffset);
            _out.indices.push_back(vertexDataOffset + 1);
            _out.indices.push_back(vertexDataOffset + 2);
            
            _out.indices.push_back(vertexDataOffset + 1);
            _out.indices.push_back(vertexDataOffset + 3);
            _out.indices.push_back(vertexDataOffset + 2);
            
            vertexDataOffset += 4;
        }
    }
}

// Get 2D perpendicular of two points
glm::vec2 perp2d(const glm::vec3& _v1, const glm::vec3& _v2 ){
    return glm::vec2(_v2.y - _v1.y, _v1.x - _v2.x);
}

// Helper function for polyline tesselation
inline void addPolyLineVertex(const glm::vec3& _coord, const glm::vec2& _normal, const glm::vec2& _uv, PolyLineOutput& _out) {
  _out.numVertices++;
  _out.addVertex(_coord, _normal, _uv);
}

// Helper function for polyline tesselation; adds indices for pairs of vertices arranged like a line strip
void indexPairs( int _nPairs, int _nVertices, std::vector<int>& _indicesOut) {
    for (int i = 0; i < _nPairs; i++) {
        _indicesOut.push_back(_nVertices - 2*i - 4);
        _indicesOut.push_back(_nVertices - 2*i - 2);
        _indicesOut.push_back(_nVertices - 2*i - 3);
        
        _indicesOut.push_back(_nVertices - 2*i - 3);
        _indicesOut.push_back(_nVertices - 2*i - 2);
        _indicesOut.push_back(_nVertices - 2*i - 1);
    }
}

//  Tessalate a fan geometry between points A       B
//  using their normals from a center        \ . . /
//  and interpolating their UVs               \ p /
//                                             \./
//                                              C
void addFan(const glm::vec3& _pC,
            const glm::vec2& _nA, const glm::vec2& _nB, const glm::vec2& _nC,
            const glm::vec2& _uA, const glm::vec2& _uB, const glm::vec2& _uC,
            int _numTriangles, PolyLineOutput& _out) {
    
    // Find angle difference
    float cross = _nA.x * _nB.y - _nA.y * _nB.x; // z component of cross(_CA, _CB)
    float angle = atan2f(cross, glm::dot(_nA, _nB));
    
    int startIndex = _out.numVertices;
    
    // Add center vertex
    addPolyLineVertex(_pC, _nC, _uC, _out);
    
    // Add vertex for point A
    addPolyLineVertex(_pC, _nA, _uA, _out);
    
    // Add radial vertices
    glm::vec2 radial = _nA;
    for (int i = 0; i < _numTriangles; i++) {
        float frac = (i + 1)/(float)_numTriangles;
        radial = glm::rotate(_nA, angle * frac);
        glm::vec2 uv = (1.f - frac) * _uA + frac * _uB;
        addPolyLineVertex(_pC, radial, uv, _out);
        
        // Add indices
        _out.indices.push_back(startIndex); // center vertex
        _out.indices.push_back(startIndex + i + (angle > 0 ? 1 : 2));
        _out.indices.push_back(startIndex + i + (angle > 0 ? 2 : 1));
    }
    
}

// Function to add the vertices for line caps
void addCap(const glm::vec3& _coord, const glm::vec2& _normal, int _numCorners, bool _isBeginning, PolyLineOutput& _out) {

    float v = _isBeginning ? 0.f : 1.f; // length-wise tex coord
    
    if (_numCorners < 1) {
        // "Butt" cap needs no extra vertices
        return;
    } else if (_numCorners == 2) {
        // "Square" cap needs two extra vertices
        glm::vec2 tangent(-_normal.y, _normal.x);
        addPolyLineVertex(_coord, _normal + tangent, {0.f, v}, _out);
        addPolyLineVertex(_coord, -_normal + tangent, {0.f, v}, _out);
        if (!_isBeginning) { // At the beginning of a line we can't form triangles with previous vertices
            indexPairs(1, _out.numVertices, _out.indices);
        }
        return;
    }
    
    // "Round" cap type needs a fan of vertices
    glm::vec2 nA(_normal), nB(-_normal), nC(0.f, 0.f), uA(1.f, v), uB(0.f, v), uC(0.5f, v);
    if (_isBeginning) {
        nA *= -1.f; // To flip the direction of the fan, we negate the normal vectors
        nB *= -1.f;
        uA.x = 0.f; // To keep tex coords consistent, we must reverse these too
        uB.x = 1.f;
    }
    addFan(_coord, nA, nB, nC, uA, uB, uC, _numCorners, _out);
}

float valuesWithinTolerance(float _a, float _b, float _tolerance = 0.001) {
    return fabsf(_a - _b) < _tolerance;
}

// Tests if a line segment (from point A to B) is nearly coincident with the edge of a tile
bool isOnTileEdge(const glm::vec3& _pa, const glm::vec3& _pb) {
    
    float tolerance = 0.0002; // tweak this adjust if catching too few/many line segments near tile edges
    // TODO: make tolerance configurable by source if necessary
    glm::vec2 tile_min(-1.0, -1.0);
    glm::vec2 tile_max(1.0, 1.0);

    return (valuesWithinTolerance(_pa.x, tile_min.x, tolerance) && valuesWithinTolerance(_pb.x, tile_min.x, tolerance)) ||
           (valuesWithinTolerance(_pa.x, tile_max.x, tolerance) && valuesWithinTolerance(_pb.x, tile_max.x, tolerance)) ||
           (valuesWithinTolerance(_pa.y, tile_min.y, tolerance) && valuesWithinTolerance(_pb.y, tile_min.y, tolerance)) ||
           (valuesWithinTolerance(_pa.y, tile_max.y, tolerance) && valuesWithinTolerance(_pb.y, tile_max.y, tolerance));
}

void Builders::buildPolyLine(const Line& _line, const PolyLineOptions& _options, PolyLineOutput& _out) {
    
    int lineSize = (int)_line.size();
    
    if (lineSize < 2) {
        return;
    }
    
    // TODO: pre-allocate output vectors; try estimating worst-case space usage
    
    glm::vec3 coordPrev(_line[0]), coordCurr(_line[0]), coordNext(_line[1]);
    glm::vec2 normPrev, normNext, miterVec;

    int cornersOnCap = (int)_options.cap;
    int trianglesOnJoin = (int)_options.join;
    
    // Process first point in line with an end cap
    normNext = glm::normalize(perp2d(coordCurr, coordNext));
    addCap(coordCurr, normNext, cornersOnCap, true, _out);
    addPolyLineVertex(coordCurr, normNext, {1.0f, 0.0f}, _out); // right corner
    addPolyLineVertex(coordCurr, -normNext, {0.0f, 0.0f}, _out); // left corner
    
    // Process intermediate points
    for (int i = 1; i < lineSize - 1; i++) {

        coordPrev = coordCurr;
        coordCurr = coordNext;
        coordNext = _line[i + 1];
        
        normPrev = normNext;
        normNext = glm::normalize(perp2d(coordCurr, coordNext));

        // Compute "normal" for miter joint
        miterVec = normPrev + normNext;
        float scale = sqrtf(2.0f / (1.0f + glm::dot(normPrev, normNext)) / glm::dot(miterVec, miterVec) );
        miterVec *= fminf(scale, 5.0f); // clamps our miter vector to an arbitrary length
        
        float v = i / (float)lineSize;
        
        if (trianglesOnJoin == 0) {
            // Join type is a simple miter
            
            addPolyLineVertex(coordCurr, miterVec, {1.0, v}, _out); // right corner
            addPolyLineVertex(coordCurr, -miterVec, {0.0, v}, _out); // left corner
            indexPairs(1, _out.numVertices, _out.indices);
            
        } else {
            // Join type is a fan of triangles
            
            bool isRightTurn = (normNext.x * normPrev.y - normNext.y * normPrev.x) > 0; // z component of cross(normNext, normPrev)
            
            if (isRightTurn) {
                
                addPolyLineVertex(coordCurr, miterVec, {1.0f, v}, _out); // right (inner) corner
                addPolyLineVertex(coordCurr, -normPrev, {0.0f, v}, _out); // left (outer) corner
                indexPairs(1, _out.numVertices, _out.indices);
                
                addFan(coordCurr, -normPrev, -normNext, miterVec, {0.f, v}, {0.f, v}, {1.f, v}, trianglesOnJoin, _out);
                
                addPolyLineVertex(coordCurr, miterVec, {1.0f, v}, _out); // right (inner) corner
                addPolyLineVertex(coordCurr, -normNext, {0.0f, v}, _out); // left (outer) corner
                
            } else {
                
                addPolyLineVertex(coordCurr, normPrev, {1.0f, v}, _out); // right (outer) corner
                addPolyLineVertex(coordCurr, -miterVec, {0.0f, v}, _out); // left (inner) corner
                indexPairs(1, _out.numVertices, _out.indices);
                
                addFan(coordCurr, normPrev, normNext, -miterVec, {1.f, v}, {1.f, v}, {0.0f, v}, trianglesOnJoin, _out);
                
                addPolyLineVertex(coordCurr, normNext, {1.0f, v}, _out); // right (outer) corner
                addPolyLineVertex(coordCurr, -miterVec, {0.0f, v}, _out); // left (inner) corner
                
            }
            
        }
    }
    
    // Process last point in line with a cap
    addPolyLineVertex(coordNext, normNext, {1.f, 1.f}, _out); // right corner
    addPolyLineVertex(coordNext, -normNext, {0.f, 1.f}, _out); // left corner
    indexPairs(1, _out.numVertices, _out.indices);
    addCap(coordNext, normNext, cornersOnCap , false, _out);
    
}

void Builders::buildOutline(const Line& _line, const PolyLineOptions& _options, PolyLineOutput& _out) {
    
    int cut = 0;
    
    for (size_t i = 0; i < _line.size() - 1; i++) {
        const glm::vec3& coordCurr = _line[i];
        const glm::vec3& coordNext = _line[i+1];
        if (isOnTileEdge(coordCurr, coordNext)) {
            Line line = Line(&_line[cut], &_line[i+1]);
            buildPolyLine(line, _options, _out);
            cut = i + 1;
        }
    }
    
    Line line = Line(&_line[cut], &_line[_line.size()]);
    buildPolyLine(line, _options, _out);
    
}

void Builders::buildQuadAtPoint(const Point& _point, const glm::vec3& _normal, float halfWidth, float height, PolygonOutput& _out) {

}
