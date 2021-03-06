#pragma once

#include "dataSource.h"

namespace Tangram {

class MVTSource : public DataSource {

protected:

    virtual std::shared_ptr<TileData> parse(const TileTask& _task,
                                            const MapProjection& _projection) const override;

public:

    MVTSource(const std::string& _name, const std::string& _urlTemplate, int32_t _maxZoom);

};

}
