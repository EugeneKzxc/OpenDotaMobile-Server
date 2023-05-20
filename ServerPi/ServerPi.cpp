// ServerPi.cpp: определяет точку входа для приложения.

#include <sstream>
#include <served/served.hpp>
#include <jsoncons/json.hpp>
#include <jsoncons_ext/jsonschema/jsonschema.hpp>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/stdx.hpp>
#include <mongocxx/uri.hpp>

#include "ServerPi.h"

#define THREAD_COUNT 1

mongocxx::instance instance{}; // This should be done only once.
mongocxx::uri uri("mongodb://localhost:27017");
mongocxx::client client(uri);

std::string schema_user_profile = R"(
        {
            "type": "object",
            "properties": {
                "_steam_id": { "type": "string" },
                "data": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "properties": {
                            "content": { "type": "string" },
                            "author": { "type": "string" }
                        },
                        "required": ["content", "author"]
                    }
                }
            },
            "required": ["_steam_id", "data"],
            "additionalProperties": false
        }
    )";

std::string schema_firebase_user = R"(
    {
        "type": "object",
        "properties": {
            "_firebase_id": { "type": "string" },
            "_name": { "type": "string" },
            "players": {
                "type": "array",
                "items": {
                    "type": "object",
                    "properties": {
                        "steam_id": { "type": "string" }
                    },
                    "required": ["steam_id"]
                }
            },
            "matches": {
                "type": "array",
                "items": {
                    "type": "object",
                    "properties": {
                        "match_id": { "type": "string" }
                    },
                    "required": ["match_id"]
                }
            }
        },
        "required": ["_firebase_id", "players", "matches"],
        "additionalProperties": false
    }
)";

mongocxx::v_noabi::database db = client["OpenDotaMobile_MongoDB"];
mongocxx::v_noabi::collection DotaUserProfilesCollection = db["DotaUserProfiles"];
mongocxx::v_noabi::collection FireBaseUserFavouritesCollection = db["FireBaseUserFavourites"];

jsoncons::json requestToMongoDB(std::string key, std::string userId)
{
    mongocxx::v_noabi::collection collection;
    std::string field;
    if (key == "dotaProfile")
    {
        collection = DotaUserProfilesCollection;
        field = "_steam_id";
    }
    if (key == "firebaseProfile")
    {
        collection = FireBaseUserFavouritesCollection;
        field = "_firebase_id";
    }

    bsoncxx::builder::basic::document filter_builder;
    filter_builder.append(bsoncxx::builder::basic::kvp(field, userId));
    bsoncxx::document::value query = filter_builder.extract();
    try
    {
        auto find_result = collection.find_one(query.view()); //segmentation fault error, Aborted
        if (find_result)
        {
            bsoncxx::document::view result_doc = find_result.value().view();
            std::string json_str = bsoncxx::to_json(result_doc);
            jsoncons::json json_result = jsoncons::json::parse(json_str);
            return json_result;
        }
        else
        {
            std::string json_str = "{\"error\": \"not found\"}";
            jsoncons::json json_result = jsoncons::json::parse(json_str);

            return json_result;
        }
    }
    catch (const std::exception& e)
    {   
        std::string exceptionMessage = e.what();
        std::string json_str = "{\"exception\": \"" + exceptionMessage + "\"}";
        jsoncons::json json_result = jsoncons::json::parse(json_str);

        return json_result;
    }
}

void RestGetRequest(served::response& res, const served::request& req)
{
    //res.set_header("content-type", "application/json");
    res.set_header("access-control-allow-origin", "*");

    jsoncons::json jdoc = requestToMongoDB(req.query["key"], req.query["id"]);

    std::stringstream ssReplay;
    ssReplay << jsoncons::pretty_print(jdoc);
    res << ssReplay.str().c_str();
}

void RestPostRequest(served::response& res, const served::request& req)
{
    res.set_header("content-type", "application/json");
    res.set_header("access-control-allow-origin", "*");
    
    jsoncons::json request_data = jsoncons::json::parse(req.body());

    auto schemaUserProfile = jsoncons::jsonschema::make_schema(jsoncons::json::parse(schema_user_profile));
    auto schemaFirebaseUser = jsoncons::jsonschema::make_schema(jsoncons::json::parse(schema_firebase_user));
    jsoncons::jsonschema::json_validator<jsoncons::json> dotaProfileValidator(schemaUserProfile);
    jsoncons::jsonschema::json_validator<jsoncons::json> fireBaseProfileValidator(schemaFirebaseUser);
    if (dotaProfileValidator.is_valid(request_data))
    {
        bsoncxx::document::value bson_doc = bsoncxx::from_json(request_data.to_string());
        try
        {   
            std::string documentUserId = request_data["_steam_id"].as_string();
            bsoncxx::document::value filter_doc = bsoncxx::builder::basic::make_document(bsoncxx::builder::basic::kvp("_steam_id", documentUserId));
            auto existing_doc = DotaUserProfilesCollection.find_one(filter_doc.view());

            if (existing_doc)
            {
                auto update_result = DotaUserProfilesCollection.update_one(filter_doc.view(), bsoncxx::builder::basic::make_document(bsoncxx::builder::basic::kvp("$set", bson_doc.view())));
                std::string json_str = "{\"answer\": \"updated\"}";
                jsoncons::json json_result = jsoncons::json::parse(json_str);
                std::stringstream ssReplay;
                ssReplay << jsoncons::pretty_print(json_result);
                res << ssReplay.str().c_str();
                return;
            }
            else
            {
                auto insert_one_result = DotaUserProfilesCollection.insert_one(bson_doc.view());
            }
        }
        catch (const std::exception& e)
        {
            std::string json_str = "{\"answer\": \"internal error\"}";
            jsoncons::json json_result = jsoncons::json::parse(json_str);
            std::stringstream ssReplay;
            ssReplay << jsoncons::pretty_print(json_result);
            res << ssReplay.str().c_str();
        }
        std::string json_str = "{\"answer\": \"success\"}";
        jsoncons::json json_result = jsoncons::json::parse(json_str);
        std::stringstream ssReplay;
        ssReplay << jsoncons::pretty_print(json_result);
        res << ssReplay.str().c_str();
    }
    else if (fireBaseProfileValidator.is_valid(request_data))
    {
        bsoncxx::document::value bson_doc = bsoncxx::from_json(request_data.to_string());
        try
        {
            std::string documentUserId = request_data["_firebase_id"].as_string();
            bsoncxx::document::value filter_doc = bsoncxx::builder::basic::make_document(bsoncxx::builder::basic::kvp("_firebase_id", documentUserId));
            auto existing_doc = FireBaseUserFavouritesCollection.find_one(filter_doc.view());

            if (existing_doc)
            {
                auto update_result = FireBaseUserFavouritesCollection.update_one(filter_doc.view(), bsoncxx::builder::basic::make_document(bsoncxx::builder::basic::kvp("$set", bson_doc.view())));
                std::string json_str = "{\"answer\": \"updated\"}";
                jsoncons::json json_result = jsoncons::json::parse(json_str);
                std::stringstream ssReplay;
                ssReplay << jsoncons::pretty_print(json_result);
                res << ssReplay.str().c_str();
                return;
            }
            else
            {
                auto insert_one_result = FireBaseUserFavouritesCollection.insert_one(bson_doc.view());
            }
        }
        catch (const std::exception& e)
        {
            std::string json_str = "{\"answer\": \"internal error\"}";
            jsoncons::json json_result = jsoncons::json::parse(json_str);
            std::stringstream ssReplay;
            ssReplay << jsoncons::pretty_print(json_result);
            res << ssReplay.str().c_str();
        }
        std::string json_str = "{\"answer\": \"success\"}";
        jsoncons::json json_result = jsoncons::json::parse(json_str);
        std::stringstream ssReplay;
        ssReplay << jsoncons::pretty_print(json_result);
        res << ssReplay.str().c_str();
    }
    else
    {
        res.set_status(400);
        return;
    }
}

int main()
{
    // curl http://192.168.1.43:8123/request
    served::multiplexer mux;
    mux.handle("/request").get(RestGetRequest);
    mux.handle("/insert").post(RestPostRequest);

    served::net::server server("0.0.0.0", "8123", mux);
    server.run(THREAD_COUNT);
    return 0;
}
