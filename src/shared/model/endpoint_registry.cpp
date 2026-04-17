#include "endpoint_registry.h"

void EndpointRegistry::add(const std::string &name, std::shared_ptr<Endpoint> endpoint)
{
    std::lock_guard<std::mutex> lk(mutex_);
    if (map_.find(name) == map_.end())
        order_.push_back(name);
    map_[name] = std::move(endpoint);
}

std::shared_ptr<Endpoint> EndpointRegistry::get(const std::string &name) const
{
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = map_.find(name);
    return (it != map_.end()) ? it->second : nullptr;
}

std::vector<std::shared_ptr<Endpoint>> EndpointRegistry::all() const
{
    std::lock_guard<std::mutex> lk(mutex_);
    std::vector<std::shared_ptr<Endpoint>> result;
    result.reserve(order_.size());
    for (const auto &key : order_)
        result.push_back(map_.at(key));
    return result;
}

std::size_t EndpointRegistry::size() const
{
    std::lock_guard<std::mutex> lk(mutex_);
    return map_.size();
}

void EndpointRegistry::clear()
{
    std::lock_guard<std::mutex> lk(mutex_);
    map_.clear();
    order_.clear();
}
