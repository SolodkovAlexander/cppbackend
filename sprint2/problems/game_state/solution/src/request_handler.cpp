#include "request_handler.h"

#include <boost/algorithm/string.hpp>

#include <unordered_map>

namespace http_handler {

std::string_view RequestHandler::ContentType::GetContentTypeByFileExtension(fs::path file_path) {
    const static std::unordered_map<std::string, std::string_view> file_extension_to_content_type = {
        {".htm"s, ContentType::TEXT_HTML},
        {".html"s, ContentType::TEXT_HTML},
        {".css"s, ContentType::TEXT_CSS},
        {".txt"s, ContentType::TEXT_PLAIN},
        {".js"s, ContentType::TEXT_JAVASCRIPT},
        {".json"s, ContentType::APPLICATION_JSON},
        {".xml"s, ContentType::APPLICATION_XML},
        {".png"s, ContentType::IMAGE_PNG},
        {".jpg"s, ContentType::IMAGE_JPEG},
        {".jpe"s, ContentType::IMAGE_JPEG},
        {".jpeg"s, ContentType::IMAGE_JPEG},
        {".gif"s, ContentType::IMAGE_GIF},
        {".bmp"s, ContentType::IMAGE_BMP},
        {".ico"s, ContentType::IMAGE_MICROSOFT_ICON},
        {".tiff"s, ContentType::IMAGE_TIFF},
        {".tif"s, ContentType::IMAGE_TIFF},
        {".svg"s, ContentType::IMAGE_SVG_XML},
        {".svgz"s, ContentType::IMAGE_SVG_XML},
        {".mp3"s, ContentType::AUDIO_MPEG}
    };
    auto file_extension = boost::algorithm::to_lower_copy(file_path.extension().string());
    if (!file_extension_to_content_type.contains(file_extension)) {
        return ContentType::UNKNOWN;
    }
    return file_extension_to_content_type.at(file_extension);
}

json::array RequestHandler::MapsToShortJson(const Game::Maps& maps) {
    json::array maps_data;
    for (const auto& map : maps) {
        maps_data.emplace_back(json::object({
            {"id", *map.GetId()},
            {"name", map.GetName()}
        }));
    }
    return maps_data;
}

json::object RequestHandler::MapToJson(const Map* map) {
    json::object map_data;
    map_data["id"] = *(map->GetId());
    map_data["name"] = map->GetName();

    json::array roads;
    for (const auto& road : map->GetRoads()) {
        roads.emplace_back(RequestHandler::RoadToJson(road));
    }
    map_data["roads"] = std::move(roads);

    json::array buildings;
    for (const auto& building : map->GetBuildings()) {
        buildings.emplace_back(RequestHandler::BuildingToJson(building));
    }
    map_data["buildings"] = std::move(buildings);

    json::array offices;
    for (const auto& office : map->GetOffices()) {
        offices.emplace_back(RequestHandler::OfficeToJson(office));
    }
    map_data["offices"] = std::move(offices);

    return map_data;
}

json::object RequestHandler::RoadToJson(const Road& road) {
    json::object road_data({
        {"x0", road.GetStart().x},
        {"y0", road.GetStart().y}
    });
    if (road.IsHorizontal()) {
        road_data["x1"] = road.GetEnd().x;
    } else {
        road_data["y1"] = road.GetEnd().y;
    }
    return road_data;
}

json::object RequestHandler::BuildingToJson(const Building& building) {
    return json::object({
        {"x", building.GetBounds().position.x},
        {"y", building.GetBounds().position.y},
        {"w", building.GetBounds().size.width},
        {"h", building.GetBounds().size.height}
    });
}

json::object RequestHandler::OfficeToJson(const Office& office) {
    return json::object({
        {"id", *(office.GetId())},
        {"x", office.GetPosition().x},
        {"y", office.GetPosition().y},
        {"offsetX", office.GetOffset().dx},
        {"offsetY", office.GetOffset().dy}
    });
}

// Возвращает true, если каталог p содержится внутри base_path.
bool RequestHandler::IsSubPath(fs::path wc_path, fs::path wc_base) {
    // Проверяем, что все компоненты base содержатся внутри path
    for (auto b = wc_base.begin(), p = wc_path.begin(); b != wc_base.end(); ++b, ++p) {
        if (p == wc_path.end() || *p != *b) {
            return false;
        }
    }
    return true;
}

}  // namespace http_handler
