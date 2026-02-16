#include "io/json_reader.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace crosside::io
{

    nlohmann::json loadJsonFile(const std::filesystem::path &path)
    {
        std::ifstream in(path);
        if (!in.is_open())
        {
            throw std::runtime_error("Could not open JSON file: " + path.string());
        }

        nlohmann::json data;
        in >> data;
        if (!data.is_object())
        {
            throw std::runtime_error("JSON root is not object: " + path.string());
        }
        return data;
    }

    std::vector<std::string> splitFlags(const std::string &text)
    {
        std::vector<std::string> out;
        std::istringstream input(text);
        std::string token;
        while (input >> token)
        {
            if (!token.empty())
            {
                out.push_back(token);
            }
        }
        return out;
    }

} // namespace crosside::io
